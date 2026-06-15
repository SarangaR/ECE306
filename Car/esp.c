#include <msp430.h>
#include "include/esp.h"
#include "include/serial.h"
#include "include/robot.h"
#include "include/ports.h"
#include "include/menu.h"
#include "include/otos.h"
#include <string.h>

#pragma PERSISTENT(Port_event)

const ESPCommand esp_commands[ESP_CMD_COUNT] =
{
    { ESP_CMD_CHECK_COMM,        "AT",                                              "Check comm"        },
    { ESP_CMD_HARD_RESET,        "AT+RESTORE",                                      "Hard reset"        },
    { ESP_CMD_SOFT_RESET,        "AT+RST",                                          "Soft reset"        },
    { ESP_CMD_GET_VERSION,       "AT+GMR",                                          "Get version"       },
    { ESP_CMD_CHECK_STATUS,      "AT+CIPSTATUS",                                    "CIP status"        },
    { ESP_CMD_CHECK_UART,        "AT+UART_CUR?",                                    "UART status"       },
    { ESP_CMD_SYSSTORE_ON,       "AT+SYSSTORE=1",                                   "Save settings ON"  },
    { ESP_CMD_SET_STATION_MODE,  "AT+CWMODE=1",                                     "Station mode"      },
    { ESP_CMD_GET_IP_MAC,        "AT+CIFSR",                                        "Get IP/MAC"        },
    { ESP_CMD_GET_MAC,           "AT+CIPSTAMAC?",                                   "Get MAC"           },
    { ESP_CMD_LIST_NETWORKS,     "AT+CWLAP",                                        "List networks"     },
    { ESP_CMD_CONNECT_WIFI,      "AT+CWJAP=\"" ESP_WIFI_SSID "\",\"" ESP_WIFI_PASSWORD "\"", "Connect WiFi" },
    { ESP_CMD_SET_HOSTNAME,      "AT+CWHOSTNAME=\"" ESP_HOSTNAME "\"",              "Set hostname"      },
    { ESP_CMD_AUTO_CONNECT,      "AT+CWAUTOCONN=1",                                 "Auto-connect ON"   },
    { ESP_CMD_RECONN_CFG,        "AT+CWRECONNCFG=5,100",                            "Reconn cfg 5,100"  },
    { ESP_CMD_SYSSTORE_OFF,      "AT+SYSSTORE=0",                                   "Save settings OFF" },
    { ESP_CMD_MULTI_CONN,        "AT+CIPMUX=1",                                     "Multi-conn ON"     },
    { ESP_CMD_CREATE_SERVER,     "AT+CIPSERVER=1," ESP_TCP_PORT_STR,                "Create TCP server" },
    { ESP_CMD_CHECK_LINK_STATUS, "AT+STATUS?",                                      "Link status"       },
    { ESP_CMD_CHECK_CONN_STATE,  "AT+CWSTATE?",                                     "Conn state"        },
    { ESP_CMD_PING,              "AT+PING=\"www.google.com\"",                             "Ping 8.8.8.8"      },
    { ESP_CMD_SET_MAC,           "AT+CIPSTAMAC=\"" ESP_MAC_ADDRESS "\"",            "Set MAC"           }
};

static ESPStartupState s_startup_state = ESP_STARTUP_WAIT_READY;

static ESPCommandEvent s_pending_event;
static unsigned char   s_has_pending = 0U;

static char s_ip_string[16] = "No IP";
static unsigned long   s_last_ip_poll_tick = 0UL;
static uint8_t         s_wifi_connected = 0U;

#define ESP_CMD_Q_MASK  (ESP_CMD_QUEUE_SIZE - 1U)

static ESPCommandEvent s_cmd_queue[ESP_CMD_QUEUE_SIZE];
static uint8_t         s_q_head = 0U;
static uint8_t         s_q_tail = 0U;

static unsigned char   s_led_on        = 0U;
static unsigned long   s_led_on_tick   = 0UL;
static unsigned char   s_ip_poll_done  = 0U;

static void route_then_follow_line(void)
{
    chainWait(1).withBLState(BL_START)
        .andThenDriveStraight(30).until(&black_all)
        .andThenWait(1).withBLState(BL_INTERCEPT)
        .andThenAlignLeftToLine().withBLState(BL_TURN)
        .andThenFollowLine(10).withBLState(BL_TRAVEL)
        .andThenWait(1).withBLState(BL_CIRCLE)
        .andThenFollowLine(3600).withBLState(BL_CIRCLE)
        .schedule();
}

void ESP_Init(void)
{
    s_has_pending         = 0U;
    s_pending_event.valid = 0U;
    s_q_head              = 0U;
    s_q_tail              = 0U;
    s_wifi_connected      = 0U;

    P3OUT &= ~IOT_EN;
    __delay_cycles(800000UL);   /* ~100 ms reset pulse */
    P3OUT |= IOT_EN;

    s_startup_state = ESP_STARTUP_WAIT_READY;
}

static void ESP_ParseCIFSR(const char *frame)
{
    const char *p = strstr(frame, "+CIFSR:STAIP,\"");
    char        new_ip[16];
    unsigned int j;

    if (!p) { return; }
    p += 14U;
    j = 0U;
    while (j < 15U && *p != '"' && *p != '\0')
    {
        new_ip[j] = *p;
        j++;
        p++;
    }
    new_ip[j] = '\0';

    /* Only store real IPs — ignore 0.0.0.0 so the last known IP
       is preserved in FRAM across reboots and shown instantly on boot. */
    if (new_ip[0] != '0')
    {
        unsigned int k;
        for (k = 0U; k <= j; k++) { s_ip_string[k] = new_ip[k]; }
    }
}

void ESP_ProcessStartup(const char *frame)
{
    if (!frame) { return; }

    ESP_ParseCIFSR(frame);

    if (strstr(frame, "WIFI GOT IP") != 0)
    {
        ESP_SendCommand(ESP_CMD_GET_IP_MAC);
    }

    if (s_startup_state == ESP_STARTUP_DONE) { return; }

    if (s_startup_state == ESP_STARTUP_WAIT_READY)
    {
        if (strstr(frame, "OK") != 0)
        {
            s_startup_state = ESP_STARTUP_WAIT_MAC_OK;
            ESP_SendCommand(ESP_CMD_SET_MAC);
        }
        return;
    }

    if (s_startup_state == ESP_STARTUP_WAIT_MAC_OK)
    {
        if (strstr(frame, "OK") != 0)
        {
            /* MAC set command acknowledged — now verify it actually took */
            s_startup_state = ESP_STARTUP_WAIT_MAC_VERIFY;
            ESP_SendCommand(ESP_CMD_GET_MAC);   /* AT+CIPSTAMAC? */
        }
    }
    else if (s_startup_state == ESP_STARTUP_WAIT_MAC_VERIFY)
    {
        if (strstr(frame, ESP_MAC_ADDRESS) != 0)
        {
            /* MAC confirmed — proceed with WiFi check */
            s_startup_state = ESP_STARTUP_WAIT_WIFI;
            ESP_SendCommand(ESP_CMD_CHECK_CONN_STATE);
        }
        else if (strstr(frame, "OK") != 0)
        {
            /* Got OK but MAC doesn't match — retry the set command */
            s_startup_state = ESP_STARTUP_WAIT_MAC_OK;
            ESP_SendCommand(ESP_CMD_SET_MAC);
        }
    }
    else if (s_startup_state == ESP_STARTUP_WAIT_WIFI)
    {
        if (strstr(frame, "+CWSTATE:2") != 0)
        {
            s_wifi_connected = 1U;
        }
        if (strstr(frame, "OK") != 0)
        {
            if (s_wifi_connected)
            {
                ESP_SendCommand(ESP_CMD_MULTI_CONN);
                s_startup_state = ESP_STARTUP_WAIT_CIPMUX_OK;
            }
            else
            {
                ESP_SendCommand(ESP_CMD_CONNECT_WIFI);
                s_startup_state = ESP_STARTUP_WAIT_CWJAP;
            }
        }
    }
    else if (s_startup_state == ESP_STARTUP_WAIT_CWJAP)
    {
        if (strstr(frame, "OK") != 0)
        {
            ESP_SendCommand(ESP_CMD_MULTI_CONN);
            s_startup_state = ESP_STARTUP_WAIT_CIPMUX_OK;
        }
    }
    else if (s_startup_state == ESP_STARTUP_WAIT_CIPMUX_OK)
    {
        if (   (strstr(frame, "OK")        != 0)
            || (strstr(frame, "no change") != 0) )
        {
            ESP_SendCommand(ESP_CMD_CREATE_SERVER);
            s_startup_state = ESP_STARTUP_WAIT_SERVER_OK;
        }
    }
    else if (s_startup_state == ESP_STARTUP_WAIT_SERVER_OK)
    {
        if (   (strstr(frame, "OK")        != 0)
            || (strstr(frame, "no change") != 0) )
        {
            s_startup_state = ESP_STARTUP_DONE;
            ESP_SendCommand(ESP_CMD_GET_IP_MAC);
        }
    }
}

ESPStartupState ESP_GetStartupState(void)
{
    return s_startup_state;
}

void ESP_SendCommand(ESPCommandID id)
{
    if (id >= ESP_CMD_COUNT) { return; }
    uart_send_buf(esp_commands[id].cmd);
}

static float esp_parse_float(const char **pp)
{
    const char *p = *pp;
    float sign        = 1.0f;
    float int_part    = 0.0f;
    float frac_part   = 0.0f;
    float frac_scale  = 1.0f;

    if      (*p == '-') { sign = -1.0f; p++; }
    else if (*p == '+') {               p++; }

    while ((*p >= '0') && (*p <= '9'))
    {
        int_part = int_part * 10.0f + (float)(*p - '0');
        p++;
    }

    if (*p == '.')
    {
        p++;
        while ((*p >= '0') && (*p <= '9'))
        {
            frac_scale *= 10.0f;
            frac_part   = frac_part * 10.0f + (float)(*p - '0');
            p++;
        }
        int_part += frac_part / frac_scale;
    }

    *pp = p;
    return sign * int_part;
}

static int esp_is_num_start(char c)
{
    return (c == '-') || (c == '+') || ((c >= '0') && (c <= '9'));
}

uint8_t ESP_ParseIPDFrame(const char *frame, ESPCommandEvent *out)
{
    const char   *p;
    unsigned int  i;
    unsigned int  val;

    if (!frame || !out) { return 0U; }

    out->valid        = 0U;
    out->direction    = ESP_DIR_NONE;
    out->time_units   = 0U;
    out->fwd_percent  = 0.0f;
    out->turn_percent = 0.0f;
    out->float_value  = 0.0f;
    out->pad_number   = 0U;
    out->pin[0]       = '\0';

    p = frame;
    while ((*p != '\0') && (*p != (char)ESP_CMD_START_CHAR)) { p++; }
    if (*p != (char)ESP_CMD_START_CHAR) { return 0U; }
    p++;

    for (i = 0U; i < 4U; i++)
    {
        if (p[i] == '\0') { return 0U; }
        out->pin[i] = p[i];
    }
    out->pin[4U] = '\0';
    p += 4U;

    if (strncmp(out->pin, ESP_COMMAND_PIN, 4U) != 0) { return 0U; }

    if (*p == 'C')
    {
        p++;
        if (!esp_is_num_start(*p)) { return 0U; }
        out->fwd_percent = esp_parse_float(&p);
        if (*p != ',') { return 0U; }
        p++;
        if (!esp_is_num_start(*p)) { return 0U; }
        out->turn_percent = esp_parse_float(&p);
        out->direction    = ESP_DIR_CURVATURE;
        out->valid        = 1U;
        return 1U;
    }

    if (*p == 'P')
    {
        out->direction = ESP_DIR_FOLLOW_LINE;
        out->valid = 1U;
        return 1U;
    }

    if (*p == 'T')
    {
        out->direction = ESP_DIR_ROUTE;
        out->valid = 1U;
        return 1U;
    }

    if (*p == 'E')
    {
        out->direction = ESP_DIR_ENTER_CIRCLE;
        out->valid = 1U;
        return 1U;
    }

    if (*p == 'X')
    {
        out->direction = ESP_DIR_EXIT_CIRCLE;
        out->valid = 1U;
        return 1U;
    }

    if (*p == 'Z')
    {
        out->direction = ESP_DIR_ZERO_OTOS;
        out->valid = 1U;
        return 1U;
    }

    if (*p == 'D')
    {
        p++;
        if (!esp_is_num_start(*p)) { return 0U; }
        out->float_value = esp_parse_float(&p);
        out->direction   = ESP_DIR_DRIVE_DISTANCE;
        out->valid       = 1U;
        return 1U;
    }

    if (*p == 'A')
    {
        p++;
        if (!esp_is_num_start(*p)) { return 0U; }
        out->float_value = esp_parse_float(&p);
        out->direction   = ESP_DIR_TURN_ABSOLUTE;
        out->valid       = 1U;
        return 1U;
    }

    if (*p == 'N')
    {
        p++;
        if ((*p < '1') || (*p > '8')) { return 0U; }
        out->pad_number = (unsigned int)(*p - '0');
        out->direction  = ESP_DIR_PAD_DISPLAY;
        out->valid      = 1U;
        return 1U;
    }

    switch (*p)
    {
        case 'F':  out->direction = ESP_DIR_FORWARD;  break;
        case 'B':  out->direction = ESP_DIR_REVERSE;  break;
        case 'R':  out->direction = ESP_DIR_RIGHT;    break;
        case 'L':  out->direction = ESP_DIR_LEFT;     break;
        default:   return 0U;
    }
    p++;

    if ((*p < '0') || (*p > '9')) { return 0U; }

    val = 0U;
    while ((*p >= '0') && (*p <= '9'))
    {
        val = (val * 10U) + (unsigned int)(*p - '0');
        p++;
    }
    out->time_units = val;
    out->valid      = 1U;

    return 1U;
}

void ESP_ScheduleEvent(const ESPCommandEvent *evt)
{
    if (!evt || !evt->valid) { return; }

    switch (evt->direction)
    {
        case ESP_DIR_CURVATURE:
            applySpeedSet(evt->fwd_percent, evt->turn_percent);
            break;

        case ESP_DIR_FORWARD:
            chainDriveStraightMs(500U).schedule();
            break;

        case ESP_DIR_REVERSE:
            chainReverseMs(500U).schedule();
            break;

        case ESP_DIR_RIGHT:
            chainSpinCCWMs(500U, 40U).schedule();
            break;

        case ESP_DIR_LEFT:
            chainSpinCWMs(500U, 40U).schedule();
            break;
            
        case ESP_DIR_FOLLOW_LINE:
            chainWait(10).withBLState(BL_START)
                .andThenDriveStraight(30).until(&black_all)
                .andThenWait(10).withBLState(BL_INTERCEPT)
                .andThenAlignLeftToLine().withBLState(BL_TURN)
                .andThenWait(10)
                .andThenFollowLine(10).withBLState(BL_TRAVEL)
                .andThenWait(10).withBLState(BL_CIRCLE)
                .andThenFollowLine(3600)
                .schedule();
            break;

        case ESP_DIR_DRIVE_DISTANCE:
            chainDriveDistance(evt->float_value).schedule();
            break;

        case ESP_DIR_TURN_ABSOLUTE:
            chainTurnToAbsoluteAngle(evt->float_value).schedule();
            break;

        case ESP_DIR_ROUTE:
            chainWait(1)
                .andThenOTOSReset()
                .andThenDriveDistance(36.0f).withTimeout(5000)
                .andThenTurnToAbsoluteAngle(90.0f).withTimeout(2000)
                .andThenDriveDistance(42.0f).withTimeout(5000)
                .andThenTurnToAbsoluteAngle(180.0f).withTimeout(2000)
                .andThenDriveDistance(11.0f).withTimeout(5000)
                .andThenWait(1).withBLState(BL_PAD_8)
                .schedule();
            robotSetOnComplete(route_then_follow_line);
            break;

        case ESP_DIR_PAD_DISPLAY:
            Menu_SetPadArrival((int)evt->pad_number);
            break;

        case ESP_DIR_ENTER_CIRCLE:
            chainWait(1).withBLState(BL_CIRCLE)
            .andThenTurnToAngle(90.0f)
            .andThenWait(1)
            .andThenDriveDistance(24.0f)
            .andThenWait(1)
            .schedule();
            break;

        case ESP_DIR_EXIT_CIRCLE:
            chainWait(1).withBLState(BL_EXIT)
            .andThenTurnToAngle(-90.0f)
            .andThenDriveDistance(24.0).withTimeout(5000)
            .andThenWait(1).withBLState(BL_STOP)
            .schedule();
            break;

        case ESP_DIR_ZERO_OTOS:
        {
            OTOS_FullReset();
            __delay_cycles(8000000);
            break;
        }

        default:
            break;
    }
}

uint8_t ESP_HasQueuedCommand(void)
{
    return (s_q_head != s_q_tail) ? 1U : 0U;
}

uint8_t ESP_DequeueCommand(ESPCommandEvent *out)
{
    if (s_q_head == s_q_tail) { return 0U; }
    *out     = s_cmd_queue[s_q_tail];
    s_q_tail = (s_q_tail + 1U) & ESP_CMD_Q_MASK;
    return 1U;
}

void ESP_FlushQueue(void)
{
    s_q_head = s_q_tail;
}

uint8_t ESP_EnqueueFromFrame(const char *frame)
{
    uint8_t         next_head;
    ESPCommandEvent evt;

    if (!frame) { return 0U; }

    if (!ESP_ParseIPDFrame(frame, &evt)) { return 0U; }

    next_head = (s_q_head + 1U) & ESP_CMD_Q_MASK;
    if (next_head == s_q_tail) { return 0U; }

    s_cmd_queue[s_q_head] = evt;
    s_q_head              = next_head;
    return 1U;
}

void ESP_SetPendingEvent(const ESPCommandEvent *evt)
{
    if (!evt) { return; }
    s_pending_event = *evt;
    s_has_pending   = 1U;
}

unsigned char ESP_HasPendingEvent(void)
{
    return s_has_pending;
}

const ESPCommandEvent *ESP_GetPendingEvent(void)
{
    return s_has_pending ? &s_pending_event : 0;
}

void ESP_ConsumePendingEvent(void)
{
    s_has_pending = 0U;
}

const char *ESP_GetIPString(void)
{
    return s_ip_string;
}

void ESP_StopIPPolling(void)
{
    s_ip_poll_done = 1U;
}

void ESP_IPPollUpdate(unsigned long tick)
{
    /* Turn off activity LED after one main-loop tick (~20 ms) */
    if (s_led_on && (tick != s_led_on_tick))
    {
        P2OUT  &= ~IOT_RUN_RED;
        s_led_on = 0U;
    }

    if (s_startup_state == ESP_STARTUP_WAIT_READY)
    {
        /* Poll AT every second until the ESP responds OK */
        if ((tick - s_last_ip_poll_tick) >= 50UL)
        {
            s_last_ip_poll_tick = tick;
            ESP_SendCommand(ESP_CMD_CHECK_COMM);
        }
        return;
    }

    if (s_startup_state != ESP_STARTUP_DONE) { return; }

    if (s_ip_poll_done) {
        /* Send ping every 2 seconds (100 ticks × 20 ms) and blink IOT_RUN_RED */
        if ((tick - s_last_ip_poll_tick) >= 100UL)
        {
            s_last_ip_poll_tick = tick;
            P2OUT       |= IOT_RUN_RED;
            s_led_on     = 1U;
            s_led_on_tick = tick;
            ESP_SendCommand(ESP_CMD_PING);
        }
        return;
    }

    /* Poll CIFSR every main-loop tick (~20 ms) */
    if ((tick - s_last_ip_poll_tick) >= 1UL)
    {
        s_last_ip_poll_tick = tick;
        ESP_SendCommand(ESP_CMD_GET_IP_MAC);
    }
}

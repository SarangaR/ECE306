#ifndef ESP_H
#define ESP_H

#include <stdint.h>

#define ESP_MAC_ADDRESS   "02:1f:e8:02:26:10"

#define ESP_TCP_PORT      (3107U)   /* TCP server port  (choose 1024–65535) */
#define ESP_TCP_PORT_STR  "3107"    /* String form used in AT+CIPSERVER cmd  */
#define ESP_HOSTNAME      "Collapsar"
#define ESP_WIFI_SSID     "ncsu"
#define ESP_WIFI_PASSWORD ""

#define ESP_CMD_START_CHAR  '^'     /* Command sentinel — can be any character */
#define ESP_COMMAND_PIN     "1234"  /* 4-char secret PIN — change before demo  */

typedef enum
{
    ESP_CMD_CHECK_COMM       = 0,   /* AT                                    */
    ESP_CMD_HARD_RESET,             /* AT+RESTORE  (factory defaults)        */
    ESP_CMD_SOFT_RESET,             /* AT+RST      (soft reboot)             */
    ESP_CMD_GET_VERSION,            /* AT+GMR      (firmware version)        */
    ESP_CMD_CHECK_STATUS,           /* AT+CIPSTATUS                          */
    ESP_CMD_CHECK_UART,             /* AT+UART_CUR?                          */
    ESP_CMD_SYSSTORE_ON,            /* AT+SYSSTORE=1  (begin saving to flash)*/
    ESP_CMD_SET_STATION_MODE,       /* AT+CWMODE=1    (station / client mode)*/
    ESP_CMD_GET_IP_MAC,             /* AT+CIFSR       (IP + MAC)             */
    ESP_CMD_GET_MAC,                /* AT+CIPSTAMAC?  (MAC only)             */
    ESP_CMD_LIST_NETWORKS,          /* AT+CWLAP       (scan for APs)         */
    ESP_CMD_CONNECT_WIFI,           /* AT+CWJAP="<ssid>","<pass>"            */
    ESP_CMD_SET_HOSTNAME,           /* AT+CWHOSTNAME="<name>"                */
    ESP_CMD_AUTO_CONNECT,           /* AT+CWAUTOCONN=1                       */
    ESP_CMD_RECONN_CFG,             /* AT+CWRECONNCFG=5,100                  */
    ESP_CMD_SYSSTORE_OFF,           /* AT+SYSSTORE=0  (stop saving to flash) */
    ESP_CMD_MULTI_CONN,             /* AT+CIPMUX=1    (allow multi clients)  */
    ESP_CMD_CREATE_SERVER,          /* AT+CIPSERVER=1,<port>                 */
    ESP_CMD_CHECK_LINK_STATUS,      /* AT+STATUS?                            */
    ESP_CMD_CHECK_CONN_STATE,       /* AT+CWSTATE?                           */
    ESP_CMD_PING,                   /* AT+PING="8.8.8.8"                     */
    ESP_CMD_SET_MAC,                /* AT+CIPSTAMAC="xx:xx:xx:xx:xx:xx"      */
    ESP_CMD_COUNT                   /* Sentinel — do not use as a command ID */
} ESPCommandID;


typedef struct
{
    ESPCommandID  id;
    const char   *cmd;         /* AT command string — NO trailing CR/LF;
                                  uart_send_buf() appends \r\n for you.   */
    const char   *description; /* Human-readable label for logging/display */
} ESPCommand;

/* The table is defined in esp.c */
extern const ESPCommand esp_commands[ESP_CMD_COUNT];

typedef enum
{
    ESP_DIR_NONE           = 0,
    ESP_DIR_FORWARD        = 'F',
    ESP_DIR_REVERSE        = 'B',
    ESP_DIR_RIGHT          = 'R',
    ESP_DIR_LEFT           = 'L',
    ESP_DIR_CURVATURE      = 'C',
    ESP_DIR_FOLLOW_LINE    = 'P',
    ESP_DIR_DRIVE_DISTANCE = 'D',
    ESP_DIR_TURN_ABSOLUTE  = 'A',
    ESP_DIR_ROUTE          = 'T',
    ESP_DIR_PAD_DISPLAY    = 'N',
    ESP_DIR_ENTER_CIRCLE   = 'E',
    ESP_DIR_EXIT_CIRCLE    = 'X',
    ESP_DIR_ZERO_OTOS      = 'Z'
} ESPDirection;

typedef struct
{
    char          pin[5];        /* Received 4-char PIN + null terminator       */
    ESPDirection  direction;     /* Parsed direction character                   */
    unsigned int  time_units;    /* Seconds for F/B; degrees for R/L; 0 for C   */
    float         fwd_percent;   /* C: forward speed  -100.0 … 100.0            */
    float         turn_percent;  /* C: turn rate      -100.0 … 100.0            */
    float         float_value;   /* D: inches; A: absolute degrees               */
    unsigned int  pad_number;    /* N: pad number 1-8                            */
    unsigned char valid;         /* 1 = parse succeeded and PIN matched          */
} ESPCommandEvent;

/* Initialise internal state and perform the IOT_EN hardware reset — call once */
void ESP_Init(void);

/* Startup state — exposed here so the menu can display progress         */
typedef enum
{
    ESP_STARTUP_WAIT_READY = 0,   /* Polling AT until ESP responds OK      */
    ESP_STARTUP_WAIT_MAC_OK,      /* Sent AT+CIPSTAMAC=, waiting OK        */
    ESP_STARTUP_WAIT_MAC_VERIFY,  /* Sent AT+CIPSTAMAC?, verifying MAC set */
    ESP_STARTUP_WAIT_WIFI,        /* Sent AT+CWSTATE?, checking connection  */
    ESP_STARTUP_WAIT_CWJAP,       /* Sent AT+CWJAP, waiting WIFI GOT IP    */
    ESP_STARTUP_WAIT_CIPMUX_OK,   /* Sent AT+CIPMUX=1, waiting for OK      */
    ESP_STARTUP_WAIT_SERVER_OK,   /* Sent AT+CIPSERVER=1,port, waiting OK  */
    ESP_STARTUP_DONE              /* Server is up — normal operation       */
} ESPStartupState;

void ESP_ProcessStartup(const char *frame);

/* Returns the current startup state — used by the menu for display.     */
ESPStartupState ESP_GetStartupState(void);

/* Transmit an AT command from the table over UCA0 (IOT UART) */
void ESP_SendCommand(ESPCommandID id);

/* Parse a raw UCA0 frame into an ESPCommandEvent.
   Returns 1 on success (valid command + PIN matched), 0 otherwise. */
uint8_t ESP_ParseIPDFrame(const char *frame, ESPCommandEvent *out);

/* Translate a validated event into robot chain commands and schedule them */
void ESP_ScheduleEvent(const ESPCommandEvent *evt);

/* Single-slot pending-event buffer (used by main loop and menu display) */
void                   ESP_SetPendingEvent(const ESPCommandEvent *evt);
unsigned char          ESP_HasPendingEvent(void);
const ESPCommandEvent *ESP_GetPendingEvent(void);
void                   ESP_ConsumePendingEvent(void);

#define ESP_CMD_QUEUE_SIZE  8U          /* max commands buffered at once    */

uint8_t ESP_EnqueueFromFrame(const char *frame);   /* returns # enqueued   */
uint8_t ESP_HasQueuedCommand(void);
uint8_t ESP_DequeueCommand(ESPCommandEvent *out);  /* 1 = got one, 0 = empty */
void    ESP_FlushQueue(void);                      /* discard all queued commands */

const char *ESP_GetIPString(void);
void        ESP_IPPollUpdate(unsigned long tick);
void        ESP_StopIPPolling(void);

#endif // ESP_H
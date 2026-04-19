#include "msp430.h"
#include <string.h>
#include "include/menu.h"
#include "include/functions.h"
#include "include/adc.h"
#include "include/detector.h"
#include "include/esp.h"
#include "include/otos.h"

#define LINE_LEN (10U)
#define ROOT_ITEM_COUNT (5U)


#define IR_CAL_EXIT_THRESHOLD (100)

#define BATTERY_FULL_V (4.20f)
#define BATTERY_EMPTY_V (3.60f)
#define BATTERY_CAPACITY_MAH (5000U)
#define BATTERY_EST_DRAW_MA (1000U)

typedef enum
{
    PAGE_MAIN = 0,
    PAGE_IR_CAL,
    PAGE_RUN_MISSION,
    PAGE_BATTERY,
    PAGE_ESP_CMD,
    PAGE_POSITION
} MenuPage;

typedef struct
{
    const char *label;
    MenuPage target;
} RootItem;

static const RootItem root_items[ROOT_ITEM_COUNT] = {
    {"IR Cal",     PAGE_IR_CAL},
    {"Run Mission",PAGE_RUN_MISSION},
    {"Battery",    PAGE_BATTERY},
    {"ESP Cmds",   PAGE_ESP_CMD},
    {"Position",   PAGE_POSITION}
};

static const char root_icons[ROOT_ITEM_COUNT] = {
    'I', 'M', 'B', 'E', 'P'
};

static MenuPage current_page = PAGE_MAIN;
static unsigned int root_index = 0U;

static unsigned int ir_cal_last_raw = 0U;
static unsigned char ir_cal_raw_valid = 0U;

static unsigned char mission_run_request = 0U;
static unsigned char mission_cancel_request = 0U;
static unsigned char mission_is_running = 0U;
static unsigned char lcd_big_mode = 0U;


static void setLine(unsigned int line, const char *text)
{
    char out[11] = "          ";
    unsigned int i = 0U;

    while ((i < LINE_LEN) && (text[i] != '\0'))
    {
        out[i] = text[i];
        i++;
    }

    out[10] = '\0';
    Display_WriteLineIfChanged(line, out);
}

static void setLineNumber3(unsigned int line, char prefix, int value)
{
    char out[11] = "          ";
    unsigned int v;

    if (value < 0)
    {
        value = 0;
    }
    if (value > 999)
    {
        value = 999;
    }

    v = (unsigned int)value;
    out[0] = prefix;
    out[1] = ':';
    out[2] = (char)('0' + ((v / 100U) % 10U));
    out[3] = (char)('0' + ((v / 10U) % 10U));
    out[4] = (char)('0' + (v % 10U));
    out[10] = '\0';
    Display_WriteLineIfChanged(line, out);
}

static void setLcdMode(unsigned char big_mode)
{
    if (big_mode)
    {
        if (!lcd_big_mode)
        {
            lcd_BIG_mid();
            lcd_big_mode = 1U;
        }
    }
    else if (lcd_big_mode)
    {
        lcd_4line();
        lcd_big_mode = 0U;
    }
}

static void renderMain(void)
{
    char row[11] = "          ";
    char name[11] = "          ";
    unsigned int i = 0U;

    setLine(0U, "   MENU   ");

    row[3] = '{';
    row[4] = root_icons[root_index];
    row[5] = '}';

    if (root_index > 0U)
    {
        row[0] = '[';
        row[1] = root_icons[root_index - 1U];
        row[2] = ']';
    }

    if ((root_index + 1U) < ROOT_ITEM_COUNT)
    {
        row[6] = '[';
        row[7] = root_icons[root_index + 1U];
        row[8] = ']';
    }

    row[10] = '\0';
    Display_WriteLineIfChanged(1U, row);

    while ((i < LINE_LEN) && (root_items[root_index].label[i] != '\0'))
    {
        name[i] = root_items[root_index].label[i];
        i++;
    }
    name[10] = '\0';
    Display_WriteLineIfChanged(2U, name);

    setLine(3U, "O        X");
}

static int batteryPercent(float voltage)
{
    float percent;

    if (voltage <= BATTERY_EMPTY_V)
    {
        return 0;
    }
    if (voltage >= BATTERY_FULL_V)
    {
        return 100;
    }

    percent = ((voltage - BATTERY_EMPTY_V) * 100.0f) / (BATTERY_FULL_V - BATTERY_EMPTY_V);
    if (percent < 0.0f)
    {
        percent = 0.0f;
    }
    if (percent > 100.0f)
    {
        percent = 100.0f;
    }

    return (int)(percent + 0.5f);
}

static void renderBattery(void)
{
    float voltage;
    int percent;
    unsigned long remaining_mah;
    unsigned long eta_tenths;
    unsigned int centivolts;
    char v_line[11] = "V: 0.00V  ";
    char p_line[11] = "P:000%    ";
    char e_line[11] = "ETA:00.0h ";

    voltage = getBatteryVoltage();
    percent = batteryPercent(voltage);

    centivolts = (unsigned int)(voltage * 100.0f + 0.5f);
    if (centivolts > 999U)
    {
        centivolts = 999U;
    }

    v_line[3] = (char)('0' + ((centivolts / 100U) % 10U));
    v_line[5] = (char)('0' + ((centivolts / 10U) % 10U));
    v_line[6] = (char)('0' + (centivolts % 10U));

    p_line[2] = (char)('0' + ((percent / 100) % 10));
    p_line[3] = (char)('0' + ((percent / 10) % 10));
    p_line[4] = (char)('0' + (percent % 10));

    remaining_mah = ((unsigned long)BATTERY_CAPACITY_MAH * (unsigned long)percent) / 100UL;
    eta_tenths = (remaining_mah * 10UL) / (unsigned long)BATTERY_EST_DRAW_MA;
    if (eta_tenths > 999UL)
    {
        eta_tenths = 999UL;
    }

    e_line[4] = (char)('0' + ((eta_tenths / 100UL) % 10UL));
    e_line[5] = (char)('0' + ((eta_tenths / 10UL) % 10UL));
    e_line[7] = (char)('0' + (eta_tenths % 10UL));

    setLine(0U, "Battery");
    Display_WriteLineIfChanged(1U, v_line);
    Display_WriteLineIfChanged(2U, p_line);
    Display_WriteLineIfChanged(3U, e_line);
}

static void renderIrCalibrate(void)
{
    setLine(0U, "IR Calibrate");
    setLineNumber3(1U, 'L', getDetectorValue(DETECTOR_LEFT));
    setLineNumber3(2U, 'R', getDetectorValue(DETECTOR_RIGHT));
    setLine(3U, "SW1W SW2B");
}

static void renderRunMission(void)
{
    setLine(0U, "Run Mission");
    if (mission_is_running)
    {
        setLine(1U, "Running");
        setLine(2U, "Wait...");
    }
    else
    {
        setLine(1U, "Ready");
        setLine(2U, "SW1 Start");
    }
    setLine(3U, "SW2 Back");
}

static void renderESPCmd(void)
{
    ESPStartupState st = ESP_GetStartupState();

    setLcdMode(0U);
    setLine(0U, "ESP Cmds");

    if (st == ESP_STARTUP_DONE)
    {
        const char  *ip = ESP_GetIPString();
        char         ip1[11] = "          ";
        char         ip2[11] = "          ";
        unsigned int len = 0U;
        unsigned int j;

        while (ip[len] != '\0') { len++; }

        for (j = 0U; j < 10U && j < len; j++)
        {
            ip1[j] = ip[j];
        }
        ip1[10] = '\0';

        for (j = 0U; j < 10U && (j + 10U) < len; j++)
        {
            ip2[j] = ip[j + 10U];
        }
        ip2[10] = '\0';

        Display_WriteLineIfChanged(1U, ip1);
        Display_WriteLineIfChanged(2U, ip2);
    }
    else
    {
        switch (st)
        {
            case ESP_STARTUP_WAIT_READY:
                setLine(1U, "Booting.. ");
                break;
            case ESP_STARTUP_WAIT_MAC_OK:
                setLine(1U, "MAC...    ");
                break;
            case ESP_STARTUP_WAIT_WIFI:
                setLine(1U, "WiFi? No  ");
                break;
            case ESP_STARTUP_WAIT_CWJAP:
                setLine(1U, "Joining...");
                break;
            case ESP_STARTUP_WAIT_CIPMUX_OK:
                setLine(1U, "MUX...    ");
                break;
            case ESP_STARTUP_WAIT_SERVER_OK:
                setLine(1U, "Srv?...   ");
                break;
            default:
                setLine(1U, "          ");
                break;
        }
        setLine(2U, "          ");
    }

    {
        float        heading_f = getHeading();
        int          heading_i;
        char         sign;
        unsigned int ha;
        char         h_line[11] = "H:+000 deg";

        if (heading_f < 0.0f)
        {
            heading_i = (int)(-heading_f + 0.5f);
            sign = '-';
        }
        else
        {
            heading_i = (int)(heading_f + 0.5f);
            sign = '+';
        }

        if (heading_i > 999) { heading_i = 999; }
        ha = (unsigned int)heading_i;

        h_line[2]  = sign;
        h_line[3]  = (char)('0' + ((ha / 100U) % 10U));
        h_line[4]  = (char)('0' + ((ha /  10U) % 10U));
        h_line[5]  = (char)('0' +  (ha         % 10U));
        h_line[10] = '\0';
        Display_WriteLineIfChanged(3U, h_line);
    }
}

static void renderPosition(void)
{
    float xf = getPositionX();
    float yf = getPositionY();
    char x_line[11] = "X:+00.0in ";
    char y_line[11] = "Y:+00.0in ";
    int xt, yt;
    char xs, ys;
    unsigned int xa, ya;

    if (xf < 0.0f) { xs = '-'; xt = (int)(-xf * 10.0f + 0.5f); }
    else            { xs = '+'; xt = (int)( xf * 10.0f + 0.5f); }
    if (xt > 999) { xt = 999; }
    xa = (unsigned int)xt;

    if (yf < 0.0f) { ys = '-'; yt = (int)(-yf * 10.0f + 0.5f); }
    else            { ys = '+'; yt = (int)( yf * 10.0f + 0.5f); }
    if (yt > 999) { yt = 999; }
    ya = (unsigned int)yt;

    x_line[2]  = xs;
    x_line[3]  = (char)('0' + ((xa / 100U) % 10U));
    x_line[4]  = (char)('0' + ((xa /  10U) % 10U));
    x_line[6]  = (char)('0' +  (xa         % 10U));
    x_line[10] = '\0';

    y_line[2]  = ys;
    y_line[3]  = (char)('0' + ((ya / 100U) % 10U));
    y_line[4]  = (char)('0' + ((ya /  10U) % 10U));
    y_line[6]  = (char)('0' +  (ya         % 10U));
    y_line[10] = '\0';

    setLine(0U, "Position");
    Display_WriteLineIfChanged(1U, x_line);
    Display_WriteLineIfChanged(2U, y_line);
    setLine(3U, "SW2 Back");
}

void Menu_Init(void)
{
    current_page = PAGE_MAIN;
    root_index = 0U;
    ir_cal_last_raw = 0U;
    ir_cal_raw_valid = 0U;
    mission_run_request = 0U;
    mission_cancel_request = 0U;
    mission_is_running = 0U;
    lcd_big_mode = 0U;

    setThumbWheelMenuCount(ROOT_ITEM_COUNT);
    lcd_4line();
}

void Menu_Update(void)
{
    int thumb;
    int delta;
    unsigned int raw;

    switch (current_page)
    {
    case PAGE_MAIN:
        setThumbWheelMenuCount(ROOT_ITEM_COUNT);
        thumb = getThumbWheel();
        if (thumb < 0)
        {
            thumb = 0;
        }
        if (thumb >= (int)ROOT_ITEM_COUNT)
        {
            thumb = (int)ROOT_ITEM_COUNT - 1;
        }
        root_index = (unsigned int)thumb;
        break;

    case PAGE_IR_CAL:
        setThumbWheelMenuCount(1U);
        raw = adc_thumb_raw;
        if (!ir_cal_raw_valid)
        {
            ir_cal_last_raw = raw;
            ir_cal_raw_valid = 1U;
        }
        else
        {
            delta = (int)raw - (int)ir_cal_last_raw;
            if (delta < 0)
            {
                delta = -delta;
            }
            if (delta > (int)IR_CAL_EXIT_THRESHOLD)
            {
                current_page = PAGE_MAIN;
                ir_cal_raw_valid = 0U;
                setThumbWheelMenuCount(ROOT_ITEM_COUNT);
            }
        }
        break;

    case PAGE_RUN_MISSION:
    case PAGE_BATTERY:
    case PAGE_ESP_CMD:
    case PAGE_POSITION:
        setThumbWheelMenuCount(1U);
        break;

    default:
        setThumbWheelMenuCount(1U);
        break;
    }
}

void Menu_Render(void)
{
    switch (current_page)
    {
    case PAGE_MAIN:
        setLcdMode(0U);
        renderMain();
        break;

    case PAGE_IR_CAL:
        setLcdMode(0U);
        renderIrCalibrate();
        break;

    case PAGE_RUN_MISSION:
        setLcdMode(0U);
        renderRunMission();
        break;

    case PAGE_BATTERY:
        setLcdMode(0U);
        renderBattery();
        break;

    case PAGE_ESP_CMD:
        renderESPCmd();
        break;

    case PAGE_POSITION:
        setLcdMode(0U);
        renderPosition();
        break;

    default:
        setLcdMode(0U);
        renderBattery();
        break;
    }
}

void Menu_OnSW1(void)
{
    switch (current_page)
    {
    case PAGE_MAIN:
        current_page = root_items[root_index].target;
        if (current_page == PAGE_IR_CAL)
        {
            ir_cal_raw_valid = 0U;
        }
        break;

    case PAGE_IR_CAL:
        detectorSetWhiteRangeFromCurrent();
        break;

    case PAGE_RUN_MISSION:
        if (!mission_is_running)
        {
            mission_run_request = 1U;
        }
        break;

    default:
        break;
    }
}

void Menu_OnSW2(void)
{
    if (current_page == PAGE_IR_CAL)
    {
        detectorSetBlackRangeFromCurrent();
        return;
    }

    if (current_page == PAGE_RUN_MISSION && mission_is_running)
    {
        mission_cancel_request = 1U;
    }

    if (current_page != PAGE_MAIN)
    {
        current_page = PAGE_MAIN;
        setThumbWheelMenuCount(ROOT_ITEM_COUNT);
    }
}

unsigned char Menu_ConsumeRunMissionRequest(void)
{
    if (mission_run_request)
    {
        mission_run_request = 0U;
        return 1U;
    }

    return 0U;
}

unsigned char Menu_ConsumeCancelMissionRequest(void)
{
    if (mission_cancel_request)
    {
        mission_cancel_request = 0U;
        return 1U;
    }

    return 0U;
}

void Menu_SetMissionRunning(unsigned char running)
{
    mission_is_running = running;
}

void Menu_NotifyESPCommandReceived(void)
{
}

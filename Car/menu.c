#include "msp430.h"
#include <string.h>
#include "include/menu.h"
#include "include/functions.h"
#include "include/otos.h"
#include "include/adc.h"
#include "include/detector.h"
#include "include/esp.h"
#include "include/macros.h"

#define LINE_LEN (10U)
#define ROOT_ITEM_COUNT (6U)


#define IR_CAL_EXIT_THRESHOLD (100)

#define BATTERY_FULL_V (4.20f)
#define BATTERY_EMPTY_V (3.50f)
#define BATTERY_CAPACITY_MAH (6500U)
#define BATTERY_EST_DRAW_MA (200U)

#define REMOTE_TICKS_PER_SEC (50UL)

typedef enum
{
    PAGE_BOOT_IP = 0,
    PAGE_IR_CAL,
    PAGE_WAITING,
    PAGE_REMOTE,
    PAGE_BL_STOP,
    PAGE_MAIN,
    PAGE_RUN_MISSION,
    PAGE_BATTERY,
    PAGE_ESP_CMD,
    PAGE_POSITION,
    PAGE_FINAL_DEMO
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
    {"Position",   PAGE_POSITION},
    {"Final Demo", PAGE_FINAL_DEMO}
};

static const char root_icons[ROOT_ITEM_COUNT] = {
    'I', 'M', 'B', 'E', 'P', 'F'
};

static MenuPage current_page = PAGE_BOOT_IP;
static unsigned int root_index = 0U;

static unsigned int ir_cal_last_raw = 0U;
static unsigned char ir_cal_raw_valid = 0U;

static unsigned char mission_run_request = 0U;
static unsigned char mission_cancel_request = 0U;
static unsigned char mission_is_running = 0U;
static unsigned char lcd_big_mode = 0U;
static int s_pad_number = 0;

static char s_last_cmd5[6] = "     ";
static unsigned char s_last_cmd_valid = 0U;
static BLState s_bl_state = BL_NONE;
static unsigned long s_remote_start_tick = 0UL;
static unsigned long s_remote_stop_tick = 0UL;
static unsigned char s_remote_started = 0U;
static unsigned char s_remote_stopped = 0U;
extern volatile unsigned int black_line_left;
extern volatile unsigned int black_line_right;

static void renderSignedFloat1(unsigned int line, char prefix, float value);


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

    percent = ((voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V)) * 100.0f;
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
        renderSignedFloat1(1U, 'X', getPositionX());
        renderSignedFloat1(2U, 'Y', getPositionY());
        setLine(3U, "SW2 Stop");
    }
    else
    {
        setLine(1U, "Ready");
        setLine(2U, "SW1 Start");
        setLine(3U, "SW2 Back");
    }
}

static void splitIPLines(char *ip1, char *ip2)
{
    const char  *ip = ESP_GetIPString();
    unsigned int len = 0U;
    unsigned int j;

    while (ip[len] != '\0') { len++; }

    for (j = 0U; j < 10U; j++) { ip1[j] = ' '; ip2[j] = ' '; }
    ip1[10] = '\0';
    ip2[10] = '\0';

    for (j = 0U; j < 10U && j < len; j++)
    {
        ip1[j] = ip[j];
    }
    for (j = 0U; j < 10U && (j + 10U) < len; j++)
    {
        ip2[j] = ip[j + 10U];
    }
}

static void renderBootIP(void)
{
    ESPStartupState st = ESP_GetStartupState();

    setLine(0U, "SSID: ncsu");

    if (st == ESP_STARTUP_DONE)
    {
        char ip1[11];
        char ip2[11];
        splitIPLines(ip1, ip2);
        Display_WriteLineIfChanged(1U, ip1);
        Display_WriteLineIfChanged(2U, ip2);
    }
    else
    {
        switch (st)
        {
            case ESP_STARTUP_WAIT_READY:    setLine(1U, "Booting.. "); break;
            case ESP_STARTUP_WAIT_MAC_OK:   setLine(1U, "MAC...    "); break;
            case ESP_STARTUP_WAIT_WIFI:     setLine(1U, "WiFi? No  "); break;
            case ESP_STARTUP_WAIT_CWJAP:    setLine(1U, "Joining...");  break;
            case ESP_STARTUP_WAIT_CIPMUX_OK:setLine(1U, "MUX...    "); break;
            case ESP_STARTUP_WAIT_SERVER_OK:setLine(1U, "Srv?...   "); break;
            default:                        setLine(1U, "          "); break;
        }
        setLine(2U, "          ");
    }
}

static void renderWaiting(void)
{
    char ip1[11];
    char ip2[11];

    setLine(0U, "Waiting   ");
    setLine(1U, "for input ");

    if (ESP_GetStartupState() == ESP_STARTUP_DONE)
    {
        splitIPLines(ip1, ip2);
        Display_WriteLineIfChanged(2U, ip1);
        Display_WriteLineIfChanged(3U, ip2);
    }
    else
    {
        setLine(2U, "          ");
        setLine(3U, "          ");
    }
}

static void renderTopBL(void)
{
    switch (s_bl_state)
    {
        case BL_START:     setLine(0U, "BL Start  "); break;
        case BL_INTERCEPT: setLine(0U, "Intercept "); break;
        case BL_TURN:      setLine(0U, "BL Turn   "); break;
        case BL_TRAVEL:    setLine(0U, "BL Travel "); break;
        case BL_CIRCLE:    setLine(0U, "BL Circle "); break;
        case BL_EXIT:      setLine(0U, "BL Exit   "); break;
        case BL_STOP:      setLine(0U, "BL Stop   "); break;
        case BL_PAD_8:     setLine(0U, "Arrived 08"); break;
        default:           setLine(0U, "          "); break;
    }
}

static void renderTopArrived(void)
{
    char line0[11] = "Arrived 0X";

    if (s_pad_number >= 1 && s_pad_number <= 8)
    {
        line0[9] = (char)('0' + (unsigned int)s_pad_number);
        line0[10] = '\0';
        Display_WriteLineIfChanged(0U, line0);
    }
    else
    {
        setLine(0U, "          ");
    }
}

static void renderBottomCmdSecs(void)
{
    char b_line[11] = "          ";
    unsigned long secs = 0UL;
    unsigned int s;

    if (s_remote_started)
    {
        secs = (one_second_timer - s_remote_start_tick) / REMOTE_TICKS_PER_SEC;
    }
    if (secs > 999UL) { secs = 999UL; }
    s = (unsigned int)secs;

    b_line[0] = s_last_cmd5[0];
    b_line[1] = s_last_cmd5[1];
    b_line[2] = s_last_cmd5[2];
    b_line[3] = s_last_cmd5[3];
    b_line[4] = s_last_cmd5[4];
    b_line[5] = ' ';
    b_line[6] = (char)('0' + ((s / 100U) % 10U));
    b_line[7] = (char)('0' + ((s / 10U)  % 10U));
    b_line[8] = (char)('0' +  (s         % 10U));
    b_line[9] = 's';
    b_line[10] = '\0';
    Display_WriteLineIfChanged(3U, b_line);
}

static void renderRemote(void)
{
    if (s_bl_state != BL_NONE)
    {
        renderTopBL();
    }
    else
    {
        renderTopArrived();
    }

    setLine(1U, "Saranga   ");
    setLine(2U, "Rajagopala");
    renderBottomCmdSecs();
}

static void renderBLStop(void)
{
    char t_line[11] = "Time: 000s";
    unsigned long secs = 0UL;
    unsigned int s;

    if (s_remote_started)
    {
        secs = (s_remote_stop_tick - s_remote_start_tick) / REMOTE_TICKS_PER_SEC;
    }
    if (secs > 999UL) { secs = 999UL; }
    s = (unsigned int)secs;

    t_line[6] = (char)('0' + ((s / 100U) % 10U));
    t_line[7] = (char)('0' + ((s / 10U)  % 10U));
    t_line[8] = (char)('0' +  (s         % 10U));
    t_line[10] = '\0';

    setLine(0U, "BL Stop   ");
    setLine(1U, "That was  ");
    setLine(2U, "easy!! ;-)");
    Display_WriteLineIfChanged(3U, t_line);
}

static void renderESPCmd(void)
{
    ESPStartupState st = ESP_GetStartupState();

    setLcdMode(0U);
    setLine(0U, "SSID: ncsu");

    if (st == ESP_STARTUP_DONE)
    {
        char ip1[11];
        char ip2[11];
        splitIPLines(ip1, ip2);
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

static void renderSignedFloat1(unsigned int line, char prefix, float value)
{
    char out[11] = "X:+000.0  ";
    char sign;
    unsigned int av;

    out[0] = prefix;

    if (value < 0.0f)
    {
        sign = '-';
        value = -value;
    }
    else
    {
        sign = '+';
    }

    if (value > 999.9f) { value = 999.9f; }
    av = (unsigned int)(value * 10.0f + 0.5f);

    out[2] = sign;
    out[3] = (char)('0' + ((av / 1000U) % 10U));
    out[4] = (char)('0' + ((av /  100U) % 10U));
    out[5] = (char)('0' + ((av /   10U) % 10U));
    out[6] = '.';
    out[7] = (char)('0' + (av % 10U));
    out[10] = '\0';
    Display_WriteLineIfChanged(line, out);
}

static void renderPosition(void)
{
    setLcdMode(0U);
    setLine(0U, "Position");
    renderSignedFloat1(1U, 'X', getPositionX());
    renderSignedFloat1(2U, 'Y', getPositionY());
    setLine(3U, "SW2 Back");
}

static void formatLastCmd(const ESPCommandEvent *evt)
{
    unsigned int v;
    int sv;

    s_last_cmd5[0] = ' ';
    s_last_cmd5[1] = ' ';
    s_last_cmd5[2] = ' ';
    s_last_cmd5[3] = ' ';
    s_last_cmd5[4] = ' ';
    s_last_cmd5[5] = '\0';

    switch (evt->direction)
    {
        case ESP_DIR_FORWARD:
        case ESP_DIR_REVERSE:
        case ESP_DIR_RIGHT:
        case ESP_DIR_LEFT:
            v = evt->time_units;
            if (v > 9999U) v = 9999U;
            s_last_cmd5[0] = (char)evt->direction;
            s_last_cmd5[1] = (char)('0' + ((v / 1000U) % 10U));
            s_last_cmd5[2] = (char)('0' + ((v /  100U) % 10U));
            s_last_cmd5[3] = (char)('0' + ((v /   10U) % 10U));
            s_last_cmd5[4] = (char)('0' +  (v          % 10U));
            break;

        case ESP_DIR_CURVATURE:
            sv = (int)evt->fwd_percent;
            if (sv < 0) sv = -sv;
            if (sv > 99) sv = 99;
            s_last_cmd5[0] = 'C';
            s_last_cmd5[1] = (evt->fwd_percent < 0.0f) ? '-' : '+';
            s_last_cmd5[2] = (char)('0' + ((sv / 10) % 10));
            s_last_cmd5[3] = (char)('0' + (sv % 10));
            sv = (int)evt->turn_percent;
            if (sv < 0) sv = -sv;
            if (sv > 9) sv = 9;
            s_last_cmd5[4] = (char)('0' + (sv % 10));
            break;

        case ESP_DIR_DRIVE_DISTANCE:
        case ESP_DIR_TURN_ABSOLUTE:
            sv = (int)evt->float_value;
            if (sv < 0) sv = -sv;
            if (sv > 9999) sv = 9999;
            s_last_cmd5[0] = (evt->direction == ESP_DIR_DRIVE_DISTANCE) ? 'D' : 'A';
            s_last_cmd5[1] = (char)('0' + ((sv / 1000) % 10));
            s_last_cmd5[2] = (char)('0' + ((sv /  100) % 10));
            s_last_cmd5[3] = (char)('0' + ((sv /   10) % 10));
            s_last_cmd5[4] = (char)('0' + (sv % 10));
            break;

        case ESP_DIR_FOLLOW_LINE:  s_last_cmd5[0] = 'P'; break;
        case ESP_DIR_ROUTE:        s_last_cmd5[0] = 'T'; break;
        case ESP_DIR_ENTER_CIRCLE: s_last_cmd5[0] = 'E'; break;
        case ESP_DIR_EXIT_CIRCLE:  s_last_cmd5[0] = 'X'; break;
        case ESP_DIR_ZERO_OTOS:    s_last_cmd5[0] = 'Z'; break;
        case ESP_DIR_PAD_DISPLAY:
            s_last_cmd5[0] = 'N';
            s_last_cmd5[1] = (char)('0' + (evt->pad_number % 10U));
            break;

        default:
            s_last_cmd5[0] = '?';
            break;
    }

    s_last_cmd_valid = 1U;
}

static unsigned char isStartCommand(const ESPCommandEvent *evt)
{
    if (!evt || !evt->valid) { return 0U; }

    if (evt->direction == ESP_DIR_CURVATURE)
    {
        if ((evt->fwd_percent == 0.0f) && (evt->turn_percent == 0.0f))
        {
            return 0U;
        }
    }
    return 1U;
}

void Menu_SetPadArrival(int pad_number)
{
    s_pad_number = pad_number;
}

void Menu_SetBLState(BLState state)
{
    s_bl_state = state;
    if (state == BL_STOP)
    {
        s_remote_stop_tick = one_second_timer;
        s_remote_stopped = 1U;
        current_page = PAGE_BL_STOP;
    }
}

unsigned char Menu_IsInIRCal(void)
{
    return (current_page == PAGE_IR_CAL) ? 1U : 0U;
}

void Menu_Init(void)
{
    current_page = PAGE_BOOT_IP;
    root_index = 0U;
    ir_cal_last_raw = 0U;
    ir_cal_raw_valid = 0U;
    mission_run_request = 0U;
    mission_cancel_request = 0U;
    mission_is_running = 0U;
    lcd_big_mode = 0U;
    s_pad_number = 0;
    s_bl_state = BL_NONE;
    s_remote_started = 0U;
    s_remote_stopped = 0U;
    s_last_cmd_valid = 0U;
    s_last_cmd5[0] = ' ';
    s_last_cmd5[1] = ' ';
    s_last_cmd5[2] = ' ';
    s_last_cmd5[3] = ' ';
    s_last_cmd5[4] = ' ';
    s_last_cmd5[5] = '\0';

    setThumbWheelMenuCount(1U);
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
                current_page = PAGE_WAITING;
                ir_cal_raw_valid = 0U;
                setThumbWheelMenuCount(1U);
            }
        }
        break;

    case PAGE_REMOTE:
        setThumbWheelMenuCount(1U);
        break;

    case PAGE_BOOT_IP:
    case PAGE_WAITING:
    case PAGE_BL_STOP:
    case PAGE_RUN_MISSION:
    case PAGE_BATTERY:
    case PAGE_ESP_CMD:
    case PAGE_POSITION:
    case PAGE_FINAL_DEMO:
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
    case PAGE_BOOT_IP:
        setLcdMode(0U);
        renderBootIP();
        break;

    case PAGE_IR_CAL:
        setLcdMode(0U);
        renderIrCalibrate();
        break;

    case PAGE_WAITING:
        setLcdMode(0U);
        renderWaiting();
        break;

    case PAGE_REMOTE:
        setLcdMode(0U);
        renderRemote();
        break;

    case PAGE_BL_STOP:
        setLcdMode(0U);
        renderBLStop();
        break;

    case PAGE_MAIN:
        setLcdMode(0U);
        renderMain();
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
        renderPosition();
        break;

    case PAGE_FINAL_DEMO:
        setLcdMode(0U);
        renderRemote();
        break;

    default:
        setLcdMode(0U);
        renderBattery();
        break;
    }
}

static void enter_ir_cal(void)
{
    current_page = PAGE_IR_CAL;
    ir_cal_raw_valid = 0U;
    /* Stop CIFSR polling — no need to update IP during calibration */
    ESP_StopIPPolling();
    /* Re-calibrate IMU now that the board is at operating temperature
       and the robot is known to be stationary. */
    OTOS_CalibrateImu(255U, 1U);
}

void Menu_OnSW1(void)
{
    switch (current_page)
    {
    case PAGE_BOOT_IP:
        enter_ir_cal();
        break;

    case PAGE_MAIN:
        current_page = root_items[root_index].target;
        if (current_page == PAGE_IR_CAL)
        {
            ir_cal_raw_valid = 0U;
            (void)getThumbWheel();
            OTOS_CalibrateImu(255U, 1U);
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
    if (current_page == PAGE_BOOT_IP)
    {
        enter_ir_cal();
        return;
    }

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
        s_pad_number = 0;
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

void Menu_NotifyESPCommandReceived(const ESPCommandEvent *evt)
{
    if (!evt || !evt->valid) { return; }

    if (!isStartCommand(evt))
    {
        if (current_page == PAGE_WAITING) { return; }
    }

    if ((current_page == PAGE_WAITING) && isStartCommand(evt))
    {
        current_page = PAGE_REMOTE;
        s_remote_started = 1U;
        s_remote_start_tick = one_second_timer;
        s_bl_state = BL_NONE;
    }

    formatLastCmd(evt);

    switch (evt->direction)
    {
        case ESP_DIR_FOLLOW_LINE:
            s_bl_state = BL_START;
            break;
        case ESP_DIR_ENTER_CIRCLE:
            s_bl_state = BL_CIRCLE;
            break;
        case ESP_DIR_EXIT_CIRCLE:
            s_bl_state = BL_EXIT;
            break;
        default:
            break;
    }
}

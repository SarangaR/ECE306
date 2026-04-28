#include "msp430.h"
#include <string.h>
#include "include/functions.h"
#include "include/LCD.h"
#include "include/ports.h"
#include "include/macros.h"
#include "include/adc.h"
#include "include/robot.h"
#include "include/menu.h"
#include "include/serial.h"
#include "include/esp.h"
#include "include/debug_pc.h"
#include "include/detector.h"
#include "include/dac.h"

// Global Variables
volatile unsigned int black_line_left = 0;
volatile unsigned int black_line_right = 0;
volatile unsigned int black_all = 0;
unsigned long project7count = 0;
unsigned int project7lock = 0;
volatile unsigned int project7flag = 0;

volatile uint8_t baud_hi = 0;

static Robot g_robot;

void Display_WriteLineIfChanged(unsigned int line, const char *text10)
{
    if ((line > 3U) || (text10 == 0)) return;
    if (strncmp(display_line[line], text10, 10) != 0)
    {
        memcpy(display_line[line], text10, 10);
        display_line[line][10] = '\0';
        display_changed = TRUE;
    }
}

static void ScheduleMissionCommands(void)
{   
    chainWait(1.0).andThenDriveToXY(0.0f, 5.0f).schedule();
}

void main(void)
{
    PM5CTL0 &= ~LOCKLPM5;

    Init_Ports();
    Init_Clocks();
    Init_Conditions();
    Init_Timers();
    Init_LCD();
    Init_ADC();
    Init_Switches();

    Init_DAC();
    initRobot(&g_robot);
    Init_IMU();

    uart_init();
    ESP_Init(); 
    // Debug_PC_Init();
    Menu_Init();

    P6OUT |= LCD_BACKLITE;
    P2OUT |= IR_LED;

    while (ALWAYS)
    {
        {
            static char esp_frame[BUF_LEN];
            ESPCommandEvent evt;

            while (uart_read_frame(esp_frame))
            {
                ESP_ProcessStartup(esp_frame);
                if (ESP_ParseIPDFrame(esp_frame, &evt))
                {
                    /*
                     * Curvature commands are always accepted (they call
                     * applySpeedSet which handles its own preemption).
                     *
                     * All other commands are also always accepted: calling
                     * .schedule() replaces robot->active_command directly,
                     * so any incoming command naturally interrupts whatever
                     * is currently running (e.g. line following) without
                     * needing a STOP first.
                     */
                    ESP_SetPendingEvent(&evt);
                    Menu_NotifyESPCommandReceived(&evt);
                    ESP_ScheduleEvent(&evt);
                }
            }

            ESP_IPPollUpdate(one_second_timer);
        }

        Switches_Process();

        if (Switch1_ConsumePress())
        {
            Menu_OnSW1();
        }

        if (Switch2_ConsumePress())
        {
            Menu_OnSW2();
        }

        if (Menu_ConsumeRunMissionRequest())
        {
            ScheduleMissionCommands();
        }

        if (Menu_ConsumeCancelMissionRequest())
        {
            resetRobot(&g_robot);
        }

        {
            static unsigned long s_imu_tick = 0UL;
            unsigned long t = one_second_timer;
            if (t != s_imu_tick)
            {
                s_imu_tick = t;
                IMU_Process();
                robotCurvatureWatchdog(t);
            }
        }

        {
            /* Blink onboard red LED at 1 Hz (500 ms on, 500 ms off).
               one_second_timer ticks every ~20 ms, so 25 ticks = 500 ms. */
            static unsigned long s_blink_tick = 0UL;
            static unsigned char s_blink_state = 0U;
            unsigned long t = one_second_timer;
            if ((t - s_blink_tick) >= 25UL)
            {
                s_blink_tick = t;
                s_blink_state ^= 1U;
                if (s_blink_state) 
                {
                    P1OUT |=  RED_LED;
                    P6OUT &= ~GRN_LED;
                }
                else 
                { 
                    P1OUT &= ~RED_LED;
                    P6OUT |= GRN_LED;
                }
            }
        }

        updateRobot(&g_robot, Time_Sequence);
        Motors_Service();

        Menu_SetMissionRunning(isRobotBusy());
        Menu_Update();
        Menu_Render();

        if (getDetectedColor(DETECTOR_LEFT) == COLOR_BLACK)
        {
            black_line_left = 1;
        }
        else {
            black_line_left = 0;
        }

        if (getDetectedColor(DETECTOR_RIGHT) == COLOR_BLACK)
        {
            black_line_right = 1;
        }
        else {
            black_line_right = 0;
        }

        if (black_line_left || black_line_right) black_all = 1;
        else black_all = 0;

        Display_Process();
        P3OUT ^= TEST_PROBE;
    }
}

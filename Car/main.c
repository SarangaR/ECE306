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
// #include "include/otos.h"

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

    initRobot(&g_robot);
    Init_IMU();

    uart_init();
    ESP_Init(); 
    // Debug_PC_Init();
    Menu_Init();

    P6OUT |= LCD_BACKLITE;

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
                    /* Curvature (C) commands are streaming speed-sets and must
                       always be accepted — they preempt any active command
                       themselves.  All other commands respect isRobotBusy(). */
                    if ((evt.direction == ESP_DIR_CURVATURE) || !isRobotBusy())
                    {
                        ESP_SetPendingEvent(&evt);
                        Menu_NotifyESPCommandReceived();
                        ESP_ScheduleEvent(&evt);
                    }
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

        updateRobot(&g_robot, Time_Sequence);
        Motors_Service();

        Menu_SetMissionRunning(isRobotBusy());
        Menu_Update();
        Menu_Render();

        Display_Process();
        P3OUT ^= TEST_PROBE;
    }
}

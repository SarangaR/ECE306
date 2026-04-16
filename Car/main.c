<<<<<<< HEAD
=======
//------------------------------------------------------------------------------
//
//  Description: This file contains the Main Routine - "While" Operating System
//
//  Jim Carlson
//  Jan 2023
//  Built with Code Composer Version: CCS12.4.0.00007_win64
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
>>>>>>> main
#include "msp430.h"
#include <string.h>
#include "include/functions.h"
#include "include/LCD.h"
#include "include/ports.h"
#include "include/macros.h"
<<<<<<< HEAD
#include "include/adc.h"
#include "include/robot.h"
#include "include/menu.h"
#include "include/serial.h"
#include "include/esp.h"
#include "include/debug_pc.h"
// #include "include/otos.h"

// Global Variables
=======
#include "include/motors.h"
#include "include/robot.h"
#include "include/otos.h"
#include "include/timers.h"
#include "include/adc.h"
#include "include/detector.h"

>>>>>>> main
volatile unsigned int black_line_left = 0;
volatile unsigned int black_line_right = 0;
volatile unsigned int black_all = 0;
unsigned long project7count = 0;
unsigned int project7lock = 0;
volatile unsigned int project7flag = 0;

<<<<<<< HEAD
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
    // Update this chain to change mission behavior.
    chainForward(2)
        .andThenWait(1)
        .andThenSpinCW(1)
        .schedule();
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
=======
void main(void)
{
  unsigned char otos_ready = 0;
  const unsigned long otos_retry_period_ticks = (2UL * (unsigned long)COMMAND_TICKS_PER_SECOND); // 2 seconds
  unsigned long otos_attempt_start_tick = 0;
  unsigned int otos_packet_start = 0;
  unsigned int packet_delta = 0;
  unsigned long ticks_elapsed = 0;

  //    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

  //------------------------------------------------------------------------------
  // Main Program
  // This is the main routine for the program. Execution of code starts here.
  // The operating system is Back Ground Fore Ground.
  //
  //------------------------------------------------------------------------------
  PM5CTL0 &= ~LOCKLPM5;
  // Disable the GPIO power-on default high-impedance mode to activate
  // previously configured port settings

  Init_Ports();      // Initialize Ports
  Init_Clocks();     // Initialize Clock System
  Init_Conditions(); // Initialize Variables and Initial Conditions
  Init_Timers();     // Initialize Timers
  Init_LCD();        // Initialize LCD
  Init_ADC();        // Initialize ADC (thumbwheel + IR detectors)
  Init_Switches();   // Initialize switch interrupts

  P2OUT |= IR_LED;   // Turn on IR emitter so detectors can read reflected light
  P6OUT |= LCD_BACKLITE;

  Init_IMU();
  OTOS_CalibrateImu(255, TRUE);
  __delay_cycles(10000);

  Robot robot;

  initRobot(&robot);           // Initialize Robot (initial states and time variables)
  setRobotShape(&robot, NONE); // Set Initial Robot Shape to NONE

  detectorSetWhiteRangeFromCurrent();
  // Command chain is scheduled by SW1 press (see switches.c)

  //------------------------------------------------------------------------------
  // Beginning of the "While" Operating System
  //------------------------------------------------------------------------------
  while (ALWAYS)
  { // Can the Operating system run

    Motors_Service();
    IMU_Process();

    if (!otos_ready)
    {
      packet_delta = (unsigned int)(IMU_GetPacketCount() - otos_packet_start);
      ticks_elapsed = one_second_timer - otos_attempt_start_tick;

      if (packet_delta >= 5U)
      {
        otos_ready = 1U;
        P6OUT &= ~LCD_BACKLITE;
        zeroHeading();
      }
      else if (ticks_elapsed >= otos_retry_period_ticks)
      {
        Init_IMU();
        otos_packet_start = IMU_GetPacketCount();
        otos_attempt_start_tick = one_second_timer;
      }
    }
    else
    {
      updateRobot(&robot, Time_Sequence);
    }
    char heading_line[11];
    char xy_line[11];
    char thumb_line[11];
    char line_3[11];

    format_emitter_line(getEmitterState(), getThumbWheel(), thumb_line);
    if (strncmp(display_line[0], thumb_line, 10) != 0)
    {
      memcpy(display_line[0], thumb_line, 11);
      display_changed = 1;
    }

    {
      format_detector_line('L', getDetectorValue(DETECTOR_LEFT),
                           (int)getDetectedColor(DETECTOR_LEFT), heading_line);
      if (strncmp(display_line[1], heading_line, 10) != 0)
      {
        memcpy(display_line[1], heading_line, 11);
        display_changed = 1;
      }

      format_detector_line('R', getDetectorValue(DETECTOR_RIGHT),
                           (int)getDetectedColor(DETECTOR_RIGHT), xy_line);
      if (strncmp(display_line[2], xy_line, 10) != 0)
      {
        memcpy(display_line[2], xy_line, 11);
        display_changed = 1;
      }

      if (!isRobotBusy())
      {
        Color left_color = getDetectedColor(DETECTOR_LEFT);
        Color right_color = getDetectedColor(DETECTOR_RIGHT);
        if ((left_color == COLOR_BLACK) && (right_color == COLOR_BLACK))
        {
          strcpy(line_3, " BLACK ALL");
        }
        else if (left_color == COLOR_BLACK)
        {
          strcpy(line_3, "L: BLACK  ");
        }
        else if (right_color == COLOR_BLACK)
        {
          strcpy(line_3, "R: BLACK  ");
        }
        else
        {
          strcpy(line_3, "  IDLE    ");
        }
        if (strncmp(display_line[3], line_3, 10) != 0)
        {
          memcpy(display_line[3], line_3, 11);
          display_changed = 1;
        }
      }
    }

    if (getDetectedColor(DETECTOR_LEFT) == COLOR_BLACK)
    {
      black_line_left = 1;
    }

    if (getDetectedColor(DETECTOR_RIGHT) == COLOR_BLACK)
    {
      black_line_right = 1;
    }

    if (black_line_left && black_line_right) {
      black_all = 1;
    }

    Switches_Process();

    Display_Process();   // Update Display
    P3OUT ^= TEST_PROBE; // Change State of TEST_PROBE OFF
  }
  //------------------------------------------------------------------------------
>>>>>>> main
}

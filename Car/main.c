//------------------------------------------------------------------------------
//
//  Description: This file contains the Main Routine - "While" Operating System
//
//  Jim Carlson
//  Jan 2023
//  Built with Code Composer Version: CCS12.4.0.00007_win64
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
#include "msp430.h"
#include <string.h>
#include "include/functions.h"
#include "include/LCD.h"
#include "include/ports.h"
#include "include/macros.h"
#include "include/motors.h"
#include "include/robot.h"
#include "include/otos.h"
#include "include/timers.h"
#include "include/adc.h"
#include "include/detector.h"

volatile unsigned int black_line_left = 0;
volatile unsigned int black_line_right = 0;

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

  strcpy(display_line[0], "OTOS INIT ");
  strcpy(display_line[1], "CALIBRATE ");
  strcpy(display_line[2], "          ");
  display_changed = TRUE;
  P6OUT |= LCD_BACKLITE;
  Display_Process();

  Init_IMU();

  strcpy(display_line[1], " WAIT DATA");
  display_changed = TRUE;
  otos_packet_start = IMU_GetPacketCount();
  otos_attempt_start_tick = one_second_timer;

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
      updateRobot(&robot, Time_Sequence); // Update Robot State Machine (for shapes)
    }
    char heading_line[11];
    char xy_line[11];
    char thumb_line[11];
    char line_3[11];

    // Line 0: Emitter state + thumbwheel value
    format_emitter_line(getEmitterState(), getThumbWheel(), thumb_line);
    if (strncmp(display_line[0], thumb_line, 10) != 0)
    {
      memcpy(display_line[0], thumb_line, 11);
      display_changed = 1;
    }

    // Lines 1-3
    {
      // Line 1: Left detector raw + color
      format_detector_line('L', getRawDetectorValue(DETECTOR_LEFT),
                           (int)getDetectedColor(DETECTOR_LEFT), heading_line);
      if (strncmp(display_line[1], heading_line, 10) != 0)
      {
        memcpy(display_line[1], heading_line, 11);
        display_changed = 1;
      }

      // Line 2: Right detector raw + color
      format_detector_line('R', getRawDetectorValue(DETECTOR_RIGHT),
                           (int)getDetectedColor(DETECTOR_RIGHT), xy_line);
      if (strncmp(display_line[2], xy_line, 10) != 0)
      {
        memcpy(display_line[2], xy_line, 11);
        display_changed = 1;
      }

      // Line 3: Status (managed by command system when busy, idle detection when not)
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

    // Only SET flags to 1 when black detected.
    // The command system clears them to 0 at start;
    // once set they stay latched until the next command clears them.
    if (getDetectedColor(DETECTOR_LEFT) == COLOR_BLACK)
    {
      black_line_left = 1;
    }

    if (getDetectedColor(DETECTOR_RIGHT) == COLOR_BLACK)
    {
      black_line_right = 1;
    }

    Switches_Process();

    Display_Process();   // Update Display
    P3OUT ^= TEST_PROBE; // Change State of TEST_PROBE OFF
  }
  //------------------------------------------------------------------------------
}

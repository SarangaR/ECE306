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
#include "Include\functions.h"
#include "Include\LCD.h"
#include "Include\ports.h"
#include "Include\macros.h"
#include "Include\motors.h"
#include "Include\robot.h"
#include "Include\otos.h"
#include "Include\timers.h"

void main(void)
{
  unsigned char otos_ready = 0;
  const unsigned long otos_retry_period_ticks = 10UL; // 2 seconds at 5 Hz tick
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

  initRobot(&robot);       // Initialize Robot (initial states and time variables)
  setRobotShape(&robot, NONE); // Set Initial Robot Shape to NONE

  // turnSysId();
  // autoTuneTurnPID();
  // chainTurnSysId().schedule();
  // chainWait(5).andThenForward(5).schedule();
  // chainWait(2).andThenTurnToAngle(90.0f).andThenTurnToAngle(-90.0).andThenTurnToAngle(0.0).andThenTurnToAngle(0.0f).andThenWait(2).schedule();
  // chainWait(5).andThenTurnSysId().schedule();
  // autoTuneTurnPID();
  chainWait(5).andThenDriveToXY(0.0f, 15.0f).schedule();

  //------------------------------------------------------------------------------
  // Beginning of the "While" Operating System
  //------------------------------------------------------------------------------
  while (ALWAYS)
  { // Can the Operating system run
    // if ((!movement_armed) && (!(P4IN & SW1)))
    // { 
      
    //   // spin(2);
    //   movement_armed = 1;
    // }

    Motors_Service();
    // Motors_PWM_Test();
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
      updateRobot(&robot, Time_Sequence); //Update Robot State Machine (for shapes)
    }
    Display_Process();   // Update Display
    P3OUT ^= TEST_PROBE; // Change State of TEST_PROBE OFF
  }
  //------------------------------------------------------------------------------
}

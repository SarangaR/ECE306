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
#include "Include\imu.h"
#include "Include\timers.h"

void main(void)
{
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
  Init_IMU();        // Initialize IMU

  Robot robot;
  initRobot(&robot);       // Initialize Robot (initial states and time variables)
  setRobotShape(&robot, NONE); // Set Initial Robot Shape to NONE

  //------------------------------------------------------------------------------
  // Beginning of the "While" Operating System
  //------------------------------------------------------------------------------
  while (ALWAYS)
  { // Can the Operating system run
    // Run a Time Based State Machine
    if (Last_Time_Sequence != Time_Sequence)
    {
      Last_Time_Sequence = Time_Sequence;
      cycle_time++;
      time_change = 1;
    }

    updateRobot(&robot, Time_Sequence); //Update Robot State Machine (for shapes)
    Display_Process();   // Update Display
    P3OUT ^= TEST_PROBE; // Change State of TEST_PROBE OFF
  }
  //------------------------------------------------------------------------------
}

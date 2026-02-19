//------------------------------------------------------------------------------
//
//  Description: This file contains the definitions for the functions that control the motors.
//
//  Saranga Rajagopalan
//  Feb 2026
//  Built with Code Composer Version: CCS12.4.0.00007_win64
//------------------------------------------------------------------------------

#include "msp430.h"
#include "Include\functions.h"
#include "Include\ports.h"

void allMotorsOff(void)
{
    P6OUT &= ~L_FORWARD; // Set Port pin Low [Wheel Off]
    P6OUT &= ~R_FORWARD; // Set Port pin Low [Wheel Off]
    P6OUT &= ~L_REVERSE; // Set Port pin Low [Wheel Off]
    P6OUT &= ~R_REVERSE; // Set Port pin Low [Wheel Off]
}

void allMotorsOn(void)
{
    P6OUT |= L_FORWARD;  // Set Port pin High [Wheel On]
    P6OUT |= R_FORWARD;  // Set Port pin High [Wheel On]
    P6OUT &= ~L_REVERSE; // Set Port pin Low [Wheel Off]
    P6OUT &= ~R_REVERSE; // Set Port pin Low [Wheel Off]
}

void leftMotorOff(void)
{
    P6OUT &= ~L_FORWARD; // Set Port pin Low [Wheel Off]
    P6OUT &= ~L_REVERSE; // Set Port pin Low [Wheel Off]
}

void rightMotorOff(void)
{
    P6OUT &= ~R_FORWARD; // Set Port pin Low [Wheel Off]
    P6OUT &= ~R_REVERSE; // Set Port pin Low [Wheel Off]
}

void leftMotorOn(void)
{
    P6OUT |= L_FORWARD;  // Set Port pin Low [Wheel Off]
    P6OUT &= ~L_REVERSE; // Set Port pin Low [Wheel Off]
}

void rightMotorOn(void)
{
    P6OUT != R_FORWARD;  // Set Port pin Low [Wheel Off]
    P6OUT &= ~R_REVERSE; // Set Port pin Low [Wheel Off]
}

#include "msp430.h"
#include <string.h>
#include "include/functions.h"
#include "include/ports.h"
#include "include/macros.h"
#include "include/timers.h"
#include "include/detector.h"
#include "include/robot.h"
#include "include/adc.h"

static volatile unsigned char sw1_event = 0;
static volatile unsigned char sw2_event = 0;
static volatile unsigned char emitter_on = 1; // starts ON (set in main init)


void enable_switch_SW1(void)
{
    P4IFG &= ~SW1;
    P4IE |= SW1;
}

void enable_switch_SW2(void)
{
    P2IFG &= ~SW2;
    P2IE |= SW2;
}

void disable_switch_SW1(void)
{
    P4IE &= ~SW1;
}

void disable_switch_SW2(void)
{
    P2IE &= ~SW2;
}

void Init_Switches(void)
{
    P4IES |= SW1; // High-to-low edge (button press with pull-up)
    P2IES |= SW2; // High-to-low edge (button press with pull-up)

    sw1_event = FALSE;
    sw2_event = FALSE;
    sw1_debounce_count = RESET_STATE;
    sw2_debounce_count = RESET_STATE;
    sw1_debounce_active = FALSE;
    sw2_debounce_active = FALSE;
    backlight_blink_enable = FALSE;

    enable_switch_SW1();
    enable_switch_SW2();
}

void Init_Switch(void)
{
    Init_Switches();
}

void switch_control(void)
{
    Switches_Process();
}

void Switches_Process(void)
{
    Switch_Process();
}

void Switch_Process(void)
{
    // Events are set by the port ISR; the timer ISR re-enables the port
    // interrupt after the debounce window, so no polling is needed here.
    if (sw1_event)
    {
        sw1_event = FALSE;
        Switch1_Process();
    }

    if (sw2_event)
    {
        sw2_event = FALSE;
        Switch2_Process();
    }
}

//------------------------------------------------------------------------------
//  Switch1_Process
//  Description: Handles SW1 press action. When thumbwheel == 0,
//               performs black calibration. Otherwise runs drive sequence.
//  Globals:     display_line, display_changed, black_line_left, black_line_right
//  Locals:      thumb
//------------------------------------------------------------------------------
void Switch1_Process(void)
{
    int thumb = getThumbWheel();

    if (thumb == THUMBWHEEL_CALIBRATE)
    {
        // Calibration mode: set black threshold from current detector readings
        detectorSetBlackRangeFromCurrent();
        chainWait(1).withDisplay("BLACK CAL ").schedule();
    }
    else
    {
        // Movement mode: clear flags then schedule the drive sequence
        black_line_left = 0;
        black_line_right = 0;
 
        chainWait(1)
            .andThenDriveStraight(30).untilSelective(&black_line_left, &black_line_right).withDisplay("TO LINE   ")
            .andThenWait(1)
            .andThenAlignLeftToLine().withDisplay("ALIGN EDGE")
            .andThenWait(1)
            .andThenFollowLine(30).withDisplay("LF PID    ")
            .andThenTurnToAngle(-90.0f)
            .andThenDriveStraight(2)
            .schedule();
    }
}

void Switch2_Process(void)
{
    int thumb = getThumbWheel();

    if (thumb == THUMBWHEEL_CALIBRATE)
    {
        // Calibration mode: set white threshold from current detector readings
        detectorSetWhiteRangeFromCurrent();
        chainWait(1).withDisplay("WHITE CAL ").schedule();
    }
    else
    {
        chainWait(1)
            .andThenTurnToAngle(-90.0f)
            .andThenTurnToAngle(90.0)
            .schedule();
    }
}

unsigned char getEmitterState(void)
{
    return emitter_on;
}

void menu_act(void)
{
}

void menu_select(void)
{
}

#pragma vector=PORT4_VECTOR
__interrupt void PORT4_ISR(void)
{
    if (P4IFG & SW1)
    {
        unsigned int new_ccr;

        P4IFG &= ~SW1;  // Clear SW1 interrupt flag
        P4IE  &= ~SW1;  // Disable SW1 port interrupt

        // Arm CCR1 ~25 ms from now; Timer0_B1_ISR re-enables SW1 when it fires
        new_ccr = TB0R + TB0_SW_DEBOUNCE_COUNTS;
        if (new_ccr >= TB0CCR0_INTERVAL)
        {
            new_ccr -= TB0CCR0_INTERVAL;
        }
        TB0CCR1  = new_ccr;
        TB0CCTL1 = CCIE; // arm – clear any stale flag and enable interrupt

        sw1_event = TRUE; // Signal main loop to process switch action
    }
}

#pragma vector=PORT2_VECTOR
__interrupt void PORT2_ISR(void)
{
    if (P2IFG & SW2)
    {
        unsigned int new_ccr;

        P2IFG &= ~SW2;  // Clear SW2 interrupt flag
        P2IE  &= ~SW2;  // Disable SW2 port interrupt

        // Arm CCR2 ~25 ms from now; Timer0_B1_ISR re-enables SW2 when it fires
        new_ccr = TB0R + TB0_SW_DEBOUNCE_COUNTS;
        if (new_ccr >= TB0CCR0_INTERVAL)
        {
            new_ccr -= TB0CCR0_INTERVAL;
        }
        TB0CCR2  = new_ccr;
        TB0CCTL2 = CCIE; // arm – clear any stale flag and enable interrupt

        sw2_event = TRUE; // Signal main loop to process switch action
    }
}

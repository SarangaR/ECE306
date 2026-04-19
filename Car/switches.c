#include "msp430.h"
#include "include/functions.h"
#include "include/ports.h"
#include "include/macros.h"
#include "include/timers.h"

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
    P4IES |= SW1;
    P2IES |= SW2;

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
}

void Switch1_Process(void)
{
}

void Switch2_Process(void)
{
}

unsigned char Switch1_ConsumePress(void)
{
    if (sw1_event)
    {
        sw1_event = FALSE;
        return TRUE;
    }
    return FALSE;
}

unsigned char Switch2_ConsumePress(void)
{
    if (sw2_event)
    {
        sw2_event = FALSE;
        return TRUE;
    }
    return FALSE;
}

unsigned char getEmitterState(void)
{
    return emitter_on;
}

void menu_act(void) {}
void menu_select(void) {}

#pragma vector=PORT4_VECTOR
__interrupt void PORT4_ISR(void)
{
    if (P4IFG & SW1)
    {
        unsigned int new_ccr;
        P4IFG &= ~SW1;
        P4IE  &= ~SW1;

        new_ccr = TB0R + TB0_SW_DEBOUNCE_COUNTS;
        if (new_ccr >= TB0CCR0_INTERVAL)
        {
            new_ccr -= TB0CCR0_INTERVAL;
        }
        TB0CCR1  = new_ccr;
        TB0CCTL1 = CCIE;

        sw1_event = TRUE;
    }
}

#pragma vector=PORT2_VECTOR
__interrupt void PORT2_ISR(void)
{
    if (P2IFG & SW2)
    {
        unsigned int new_ccr;
        P2IFG &= ~SW2;
        P2IE  &= ~SW2;

        new_ccr = TB0R + TB0_SW_DEBOUNCE_COUNTS;
        if (new_ccr >= TB0CCR0_INTERVAL)
        {
            new_ccr -= TB0CCR0_INTERVAL;
        }
        TB0CCR2  = new_ccr;
        TB0CCTL2 = CCIE;

        sw2_event = TRUE;
    }
}

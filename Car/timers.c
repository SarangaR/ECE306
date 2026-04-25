//------------------------------------------------------------------------------
//
//  Description: Timer configuration and interrupt services (DriverLib version).
//
//------------------------------------------------------------------------------

#include "msp430.h"
#include <driverlib.h>
#include "include/functions.h"
#include "include/timers.h"
#include "include/macros.h"
#include "include/ports.h"
#include "include/dac.h"
#include <string.h>

void Init_Timers(void)
{
    Init_Timer_B0();
    Init_Timer_B1();
    Init_Timer_B3();
}

void Init_Timer_B0(void)
{
    Timer_B_initUpModeParam tb0UpConfig = {0};

    tb0UpConfig.clockSource = TIMER_B_CLOCKSOURCE_SMCLK;
    tb0UpConfig.clockSourceDivider = TIMER_B_CLOCKSOURCE_DIVIDER_64;
    tb0UpConfig.timerPeriod = TB0CCR0_INTERVAL;
    tb0UpConfig.timerInterruptEnable_TBIE = TIMER_B_TBIE_INTERRUPT_DISABLE;
    tb0UpConfig.captureCompareInterruptEnable_CCR0_CCIE = TIMER_B_CCIE_CCR0_INTERRUPT_ENABLE;
    tb0UpConfig.timerClear = TIMER_B_DO_CLEAR;
    tb0UpConfig.startTimer = false;

    Timer_B_initUpMode(TIMER_B0_BASE, &tb0UpConfig);

    // CCR1: SW1 debounce – compare value is set dynamically in the port ISR
    TB0CCR1  = 0U;
    TB0CCTL1 = 0U; // CCIE disabled until a switch press arms it

    // CCR2: SW2 debounce – compare value is set dynamically in the port ISR
    TB0CCR2  = 0U;
    TB0CCTL2 = 0U; // CCIE disabled until a switch press arms it

    Timer_B_startCounter(TIMER_B0_BASE, TIMER_B_UP_MODE);
}

void Init_Timer_B1(void)
{

    Timer_B_initUpModeParam tb1UpConfig = {0};

    tb1UpConfig.clockSource = TIMER_B_CLOCKSOURCE_ACLK;
    tb1UpConfig.clockSourceDivider = TIMER_B_CLOCKSOURCE_DIVIDER_1;
    tb1UpConfig.timerPeriod = TB1CCR0_INTERVAL;
    tb1UpConfig.timerInterruptEnable_TBIE = TIMER_B_TBIE_INTERRUPT_DISABLE;
    tb1UpConfig.captureCompareInterruptEnable_CCR0_CCIE = TIMER_B_CCIE_CCR0_INTERRUPT_ENABLE;
    tb1UpConfig.timerClear = TIMER_B_DO_CLEAR;
    tb1UpConfig.startTimer = false;

    Timer_B_initUpMode(TIMER_B1_BASE, &tb1UpConfig);

    Timer_B_startCounter(TIMER_B1_BASE, TIMER_B_UP_MODE);
}

void Init_Timer_B3(void)
{
    TB3CTL = MC__STOP | TBCLR;
    TB3EX0 = TBIDEX__1;
    TB3CCR0 = TB3CCR0_INTERVAL;

    TB3CCTL2 = OUTMOD_7;
    TB3CCR2 = 0;

    TB3CCTL3 = OUTMOD_7;
    TB3CCR3 = 0;

    TB3CCTL4 = OUTMOD_7;
    TB3CCR4 = 0;

    TB3CCTL5 = OUTMOD_7;
    TB3CCR5 = 0;

    TB3CTL = TBSSEL__SMCLK | ID__1 | MC__UP | TBCLR;
}

#pragma vector = TIMER0_B0_VECTOR
__interrupt void Timer0_B0_ISR(void)
{
    update_display = TRUE;
    if (update_display_count >= DISPLAY_COUNT_MAX) update_display_count = RESET_STATE;
    update_display_count++;
    if (Time_Sequence >= TIME_SEQUENCE_MAX) Time_Sequence = RESET_STATE;
    Time_Sequence++;
    one_time = TRUE;
}

#pragma vector = TIMER1_B0_VECTOR
__interrupt void Timer1_B0_ISR(void)
{
    one_second_timer++;
}

#pragma vector = TIMER0_B1_VECTOR
__interrupt void Timer0_B1_ISR(void)
{
    switch (__even_in_range(TB0IV, TB0IV_MAX))
    {
    case TB0IV_CCR1: // CCR1 – SW1 debounce window elapsed
        TB0CCTL1 &= ~CCIE;
        P4IFG &= ~SW1;  // Clear any edge that arrived during debounce window
        P4IE  |=  SW1;  // Re-enable SW1 port interrupt
        break;

    case TB0IV_CCR2: // CCR2 – SW2 debounce window elapsed
        TB0CCTL2 &= ~CCIE;
        P2IFG &= ~SW2;  // Clear any edge that arrived during debounce window
        P2IE  |=  SW2;  // Re-enable SW2 port interrupt
        break;
    default:
        break;
    }
}

#pragma vector = TIMER1_B1_VECTOR
__interrupt void Timer1_B1_ISR(void)
{
    (void)TB1IV;
}

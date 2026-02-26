//------------------------------------------------------------------------------
//
//  Description: Timer configuration and interrupt services (DriverLib version).
//
//------------------------------------------------------------------------------

#include "msp430.h"
#include <driverlib.h>
#include "Include\functions.h"
#include "Include\timers.h"
#include "Include/macros.h"

void Init_Timers(void)
{
    Init_Timer_B0();
    Init_Timer_B1();
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

#pragma vector = TIMER0_B0_VECTOR
__interrupt void Timer0_B0_ISR(void)
{
    update_display = 1;
    update_display_count++;
    Time_Sequence++;
    one_time = 1;
}

#pragma vector = TIMER1_B0_VECTOR
__interrupt void Timer1_B0_ISR(void)
{
    one_second_timer++;
}
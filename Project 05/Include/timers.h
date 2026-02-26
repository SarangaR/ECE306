#ifndef TIMERS_H
#define TIMERS_H

#include "msp430.h"

#define TB0CCR0_INTERVAL (25000) // 8,000,000 / 8 / 8 / (1 / 200msec)
#define TB1CCR0_INTERVAL (32768) // 32,768 / 1 / (1 / 1000msec)

void Init_Timers(void);
void Init_Timer_B0(void);
void Init_Timer_B1(void);

#endif


#ifndef TIMERS_H
#define TIMERS_H

#include "msp430.h"

static inline unsigned int Timer_CCR_Interval_From_Frequency(unsigned long clock_hz, unsigned int divider, unsigned int frequency_hz)
{
	unsigned long ticks;

	if ((divider == 0U) || (frequency_hz == 0U))
	{
		return 0U;
	}

	ticks = clock_hz / ((unsigned long)divider * (unsigned long)frequency_hz);

	if (ticks > 65535UL)
	{
		return 65535U;
	}

	return (unsigned int)ticks;
}

#define TB0CCR0_INTERVAL (Timer_CCR_Interval_From_Frequency(8000000UL, 64U, 5U)) // 8,000,000 / 8 / 8 / (1 / 200msec)
#define TB1CCR0_INTERVAL (Timer_CCR_Interval_From_Frequency(32768UL, 1U, 5U)) // 32,768 / 1 / (1 / 5Hz)
#define TB3_PWM_TARGET_HZ (25000U)
#define TB3CCR0_INTERVAL (Timer_CCR_Interval_From_Frequency(8000000UL, 1U, TB3_PWM_TARGET_HZ))

void Init_Timers(void);
void Init_Timer_B0(void);
void Init_Timer_B1(void);
void Init_Timer_B3(void);

#endif


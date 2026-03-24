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

#define TB0CCR0_INTERVAL (Timer_CCR_Interval_From_Frequency(8000000UL, 64U, 5U)) // 8,000,000 / 64 / 5Hz = 200ms (display/time sequence)
#define TB1CCR0_INTERVAL (Timer_CCR_Interval_From_Frequency(32768UL, 1U, 50U)) // 32,768 / 1 / 50Hz = 20ms command tick
#define TB3_PWM_TARGET_HZ (25000U)
#define TB3CCR0_INTERVAL (Timer_CCR_Interval_From_Frequency(8000000UL, 1U, TB3_PWM_TARGET_HZ))

// TB0 CCR vector offsets (used by Timer0_B1_ISR switch)
#define TB0IV_CCR1 (2U)
#define TB0IV_CCR2 (4U)
#define TB0IV_MAX  (14U)

// Debounce window: 25 ms at SMCLK/64 = 125 kHz  =>  3125 counts
#define TB0_SW_DEBOUNCE_COUNTS (3125U)

void Init_Timers(void);
void Init_Timer_B0(void);
void Init_Timer_B1(void);
void Init_Timer_B3(void);

#endif


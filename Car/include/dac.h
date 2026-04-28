#ifndef DAC_H
#define DAC_H

#include <msp430.h>
#include <driverlib.h>
#include "macros.h"

/* --- DAC soft-start ramp constants --------------------------------- *
 * Adjust DAC_Begin and DAC_Limit to suit your supply / target.       *
 * DAC_Adjust = 1505 is empirically calibrated to 4.00 V.            */
#define DAC_Begin   (0x0800U)   /* starting count (~2 V); calibrate as needed */
#define DAC_Limit   (0x05F0U)   /* ramp stop threshold; calibrate as needed   */
#define DAC_Adjust  (1505U)     /* 4.00 V, empirically calibrated             */

/* --- Red LED convenience macros ------------------------------------ */
#define RED_LED_ON   (P1OUT |=  RED_LED)
#define RED_LED_OFF  (P1OUT &= ~RED_LED)

/* --- Shared DAC data word (written by Init_DAC, updated in ISR) --- */
extern unsigned int DAC_data;

void Init_DAC(void);

#endif /* DAC_H */
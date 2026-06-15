#include "include/dac.h"

unsigned int DAC_data = 0U;

void Init_DAC(void)
{
    SAC3DAC  = DACSREF_0;   /* Select VCC as DAC reference                */
    SAC3DAC |= DACLSEL_0;   /* DAC latch loads when DACDAT written        */

    SAC3OA   = NMUXEN;      /* SAC Negative input MUX control             */
    SAC3OA  |= PMUXEN;      /* SAC Positive input MUX control             */
    SAC3OA  |= PSEL_1;      /* 12-bit reference DAC source selected       */
    SAC3OA  |= NSEL_1;      /* Select negative pin input                  */
    SAC3OA  |= OAPM;        /* Select low speed and low power mode        */

    SAC3PGA  = MSEL_1;      /* Set OA as buffer mode                      */

    SAC3OA  |= SACEN;       /* Enable SAC                                 */
    SAC3OA  |= OAEN;        /* Enable OA                                  */

    DAC_data  = DAC_Begin;  /* Starting low value [~2 V]                  */
    SAC3DAT   = DAC_data;   /* Initial DAC data                           */

    TB0CTL  |= TBIE;        /* Timer B0 overflow interrupt enable          */
    RED_LED_ON;             /* Indicates OVERFLOW timer start             */

    SAC3DAC |= DACEN;       /* Enable DAC (last)                          */
}
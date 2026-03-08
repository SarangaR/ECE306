#include "msp430.h"
#include <driverlib.h>
#include "include/adc.h"

volatile unsigned int adc_thumb_raw     = 0;
volatile unsigned int adc_left_det_raw  = 0;
volatile unsigned int adc_right_det_raw = 0;

static volatile unsigned int adc_channel_index = ADC_THUMB;

#define ADC_RIGHT_DETECTOR_OFFSET_RAW (0U)

static const unsigned int adc_input_map[ADC_NUM_CHANNELS] = {
    ADC_INPUT_A5,   // V_THUMB
    ADC_INPUT_A2,   // V_DETECT_L
    ADC_INPUT_A3    // V_DETECT_R
};

static void ADC_SelectAndStart(unsigned int ch_index)
{
    ADC_disableConversions(ADC_BASE, ADC_PREEMPTCONVERSION);

    ADC_configureMemory(ADC_BASE,
                        adc_input_map[ch_index],
                        ADC_VREFPOS_AVCC,       // V+
                        ADC_VREFNEG_AVSS);      // V-

    ADC_startConversion(ADC_BASE, ADC_SINGLECHANNEL);
}

void Init_ADC(void)
{
    ADC_init(ADC_BASE,
             ADC_SAMPLEHOLDSOURCE_SC,
             ADC_CLOCKSOURCE_SMCLK,
             ADC_CLOCKDIVIDER_8);
    ADC_enable(ADC_BASE);
    ADC_setupSamplingTimer(ADC_BASE,
                           ADC_CYCLEHOLD_96_CYCLES,
                           ADC_MULTIPLESAMPLESDISABLE);
    ADC_setResolution(ADC_BASE, ADC_RESOLUTION_10BIT);
    ADC_setDataReadBackFormat(ADC_BASE, ADC_UNSIGNED_BINARY);

    ADC_clearInterrupt(ADC_BASE, ADC_COMPLETED_INTERRUPT_FLAG);
    ADC_enableInterrupt(ADC_BASE, ADC_COMPLETED_INTERRUPT);

    adc_channel_index = ADC_THUMB;
    ADC_SelectAndStart(adc_channel_index);
}

int getThumbWheel(void)
{
    unsigned int raw = adc_thumb_raw;

    int scaled = (int)(((long)raw * 10L + 512L) / 1023L);

    if (scaled > 10) scaled = 10;
    if (scaled < 0)  scaled = 0;

    return scaled;
}

#pragma vector = ADC_VECTOR
__interrupt void ADC_ISR(void)
{
    switch (__even_in_range(ADCIV, ADCIV_ADCIFG))
    {
    case ADCIV_ADCIFG:
        switch (adc_channel_index)
        {
        case ADC_THUMB:
            adc_thumb_raw = (unsigned int)ADC_getResults(ADC_BASE);
            break;
        case ADC_LEFT_DET:
            adc_left_det_raw = (unsigned int)ADC_getResults(ADC_BASE);
            break;
        case ADC_RIGHT_DET:
        {
            unsigned int raw = (unsigned int)ADC_getResults(ADC_BASE);
            if (raw > ADC_RIGHT_DETECTOR_OFFSET_RAW)
            {
                adc_right_det_raw = raw - ADC_RIGHT_DETECTOR_OFFSET_RAW;
            }
            else
            {
                adc_right_det_raw = 0U;
            }
            break;
        }
        default:
            break;
        }

        adc_channel_index++;
        if (adc_channel_index >= ADC_NUM_CHANNELS)
        {
            adc_channel_index = ADC_THUMB;
        }

        ADC_SelectAndStart(adc_channel_index);
        break;

    default:
        break;
    }
}

#include "msp430.h"
#include <driverlib.h>
#include "include/adc.h"
#include "include/ports.h"

volatile uint16_t adc_thumb_raw     = 0;
volatile uint16_t adc_left_det_raw  = 0;
volatile uint16_t adc_right_det_raw = 0;
volatile uint16_t adc_battery_raw   = 0;

static volatile unsigned int adc_channel_index = ADC_THUMB;
static volatile unsigned int thumbwheel_menu_count = 1U;
static volatile int last_thumb = 0;

#define ADC_RIGHT_DETECTOR_OFFSET_RAW (0U)
#define ADC_REF_VOLTAGE (3.3f)
#define BATTERY_R_TOP_OHMS (20000.0f)
#define BATTERY_R_BOTTOM_OHMS (10000.0f)
#define BATTERY_DIVIDER_GAIN ((BATTERY_R_TOP_OHMS + BATTERY_R_BOTTOM_OHMS) / BATTERY_R_BOTTOM_OHMS)

static const unsigned int adc_input_map[ADC_NUM_CHANNELS] = {
    ADC_INPUT_A5,   // V_THUMB
    ADC_INPUT_A2,   // V_DETECT_L
    ADC_INPUT_A3,   // V_DETECT_R
    ADC_INPUT_A8    // V_BAT
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
    P2OUT |= CHECK_BAT;

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

void setThumbWheelMenuCount(unsigned int item_count)
{
    if (item_count == 0U)
    {
        thumbwheel_menu_count = 1U;
    }
    else
    {
        thumbwheel_menu_count = item_count;
    }
}

int getThumbWheel(void)
{
    unsigned int raw = adc_thumb_raw;
    unsigned int count = thumbwheel_menu_count;
    unsigned int scaled;

    if (count <= 1U)
    {
        return 0;
    }

    scaled = (unsigned int)(((unsigned long)raw * (unsigned long)count) / ((unsigned long)ADC_MAX_VALUE + 1UL));
    if (scaled >= count)
    {
        scaled = count - 1U;
    }

    last_thumb = (int)scaled;

    return (int)scaled;
}

int getThumbWheelMoved(void) {
    int thumb = getThumbWheel();
    if (thumb != last_thumb) 
    {
        return 1;
    }
    return 0;
}

float getBatteryVoltage(void)
{
    float adc_voltage;

    adc_voltage = ((float)adc_battery_raw * ADC_REF_VOLTAGE) / (float)ADC_MAX_VALUE;
    return adc_voltage * BATTERY_DIVIDER_GAIN;
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
            adc_thumb_raw = ADCMEM0;
            break;
        case ADC_LEFT_DET:
            adc_left_det_raw = ADCMEM0;
            break;
        case ADC_RIGHT_DET:
            adc_right_det_raw = ADCMEM0;
            break;
        case ADC_BATTERY:
            adc_battery_raw = ADCMEM0;
            break;
        default:
            break;
        }

        adc_channel_index++;
        if (adc_channel_index >= ADC_NUM_CHANNELS)
        {
            adc_channel_index = ADC_THUMB;
        }

        /* Switch channel and trigger next single conversion directly.
           AVCC/AVSS reference bits are 0 so ADCMCTL0 = ADCINCH_x only. */
        ADCCTL0 &= ~ADCENC;
        ADCMCTL0  = adc_input_map[adc_channel_index];
        ADCCTL0  |= ADCENC | ADCSC;
        break;

    default:
        break;
    }
}

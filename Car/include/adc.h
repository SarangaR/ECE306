// P1.5  V_THUMB = A5
// P1.2  V_DETECT_L = A2
// P1.3  V_DETECT_R = A3
// P5.0  V_BAT = A8

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define ADC_THUMB (0)
#define ADC_LEFT_DET (1)
#define ADC_RIGHT_DET (2)
#define ADC_BATTERY (3)
#define ADC_NUM_CHANNELS (4)

#define ADC_MAX_VALUE (1023) // 10 bit

extern volatile uint16_t adc_thumb_raw;
extern volatile uint16_t adc_left_det_raw;
extern volatile uint16_t adc_right_det_raw;
extern volatile uint16_t adc_battery_raw;

void Init_ADC(void);
void setThumbWheelMenuCount(unsigned int item_count);
int getThumbWheel(void);
float getBatteryVoltage(void);

#endif // ADC_H

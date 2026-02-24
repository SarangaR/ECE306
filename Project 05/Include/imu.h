#ifndef IMU_H_
#define IMU_H_

#include "msp430.h"
#include <stdint.h>

void Init_IMU(void);
void IMU_Process(void);
float getHeading(void);
void zeroHeading(void);
uint8_t IMU_HasValidStartupAngle(void);

#endif

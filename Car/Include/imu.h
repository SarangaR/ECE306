#ifndef IMU_H_
#define IMU_H_

#include "msp430.h"
#include <stdint.h>

void Init_IMU(void);
void IMU_Process(void);
float getHeading(void);
void zeroHeading(void);
uint8_t IMU_HasValidStartupAngle(void);
uint16_t IMU_GetPacketCount(void);
int16_t IMU_GetRawChannel(uint8_t index);
int16_t IMU_GetAccelXRaw(void);
int16_t IMU_GetAccelYRaw(void);
int16_t IMU_GetAccelZRaw(void);

#endif

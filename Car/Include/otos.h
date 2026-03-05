#ifndef OTOS_H_
#define OTOS_H_

#include "msp430.h"
#include <stdint.h>

typedef struct
{
  float x;
  float y;
  float h;
} sfe_otos_pose2d_t;

typedef enum
{
  kSfeOtosLinearUnitMeters = 0,
  kSfeOtosLinearUnitInches = 1
} sfe_otos_linear_unit_t;

typedef enum
{
  kSfeOtosAngularUnitRadians = 0,
  kSfeOtosAngularUnitDegrees = 1
} sfe_otos_angular_unit_t;

typedef union {
  struct
  {
    uint8_t minor : 4;
    uint8_t major : 4;
  };
  uint8_t value;
} sfe_otos_version_t;

typedef union {
  struct
  {
    uint8_t enLut : 1;
    uint8_t enAcc : 1;
    uint8_t enRot : 1;
    uint8_t enVar : 1;
    uint8_t reserved : 4;
  };
  uint8_t value;
} sfe_otos_signal_process_config_t;

typedef union {
  struct
  {
    uint8_t start : 1;
    uint8_t inProgress : 1;
    uint8_t pass : 1;
    uint8_t fail : 1;
    uint8_t reserved : 4;
  };
  uint8_t value;
} sfe_otos_self_test_config_t;

typedef union {
  struct
  {
    uint8_t warnTiltAngle : 1;
    uint8_t warnOpticalTracking : 1;
    uint8_t reserved : 4;
    uint8_t errorPaa : 1;
    uint8_t errorLsm : 1;
  };
  uint8_t value;
} sfe_otos_status_t;

typedef enum
{
  OTOS_ERR_OK = 0,
  OTOS_ERR_FAIL = -1
} otos_err_t;

uint8_t OTOS_Begin(void);
otos_err_t OTOS_IsConnected(void);
otos_err_t OTOS_GetVersionInfo(sfe_otos_version_t *hwVersion, sfe_otos_version_t *fwVersion);
otos_err_t OTOS_SelfTest(void);
otos_err_t OTOS_CalibrateImu(uint8_t numSamples, uint8_t waitUntilDone);
otos_err_t OTOS_GetImuCalibrationProgress(uint8_t *numSamples);

sfe_otos_linear_unit_t OTOS_GetLinearUnit(void);
void OTOS_SetLinearUnit(sfe_otos_linear_unit_t unit);
sfe_otos_angular_unit_t OTOS_GetAngularUnit(void);
void OTOS_SetAngularUnit(sfe_otos_angular_unit_t unit);

otos_err_t OTOS_GetLinearScalar(float *scalar);
otos_err_t OTOS_SetLinearScalar(float scalar);
otos_err_t OTOS_GetAngularScalar(float *scalar);
otos_err_t OTOS_SetAngularScalar(float scalar);
otos_err_t OTOS_ResetTracking(void);
otos_err_t OTOS_GetSignalProcessConfig(sfe_otos_signal_process_config_t *config);
otos_err_t OTOS_SetSignalProcessConfig(const sfe_otos_signal_process_config_t *config);
otos_err_t OTOS_GetStatus(sfe_otos_status_t *status);
otos_err_t OTOS_GetOffset(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_SetOffset(const sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetPosition(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_SetPosition(const sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetVelocity(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetAcceleration(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetPositionStdDev(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetVelocityStdDev(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetAccelerationStdDev(sfe_otos_pose2d_t *pose);
otos_err_t OTOS_GetPosVelAcc(sfe_otos_pose2d_t *pos, sfe_otos_pose2d_t *vel, sfe_otos_pose2d_t *acc);
otos_err_t OTOS_GetPosVelAccStdDev(sfe_otos_pose2d_t *pos, sfe_otos_pose2d_t *vel, sfe_otos_pose2d_t *acc);
otos_err_t OTOS_GetPosVelAccAndStdDev(sfe_otos_pose2d_t *pos, sfe_otos_pose2d_t *vel, sfe_otos_pose2d_t *acc,
                                       sfe_otos_pose2d_t *posStdDev, sfe_otos_pose2d_t *velStdDev,
                                       sfe_otos_pose2d_t *accStdDev);

void Init_IMU(void);
void IMU_Process(void);
float getHeading(void);
float getPositionX(void);
float getPositionY(void);
void zeroHeading(void);
uint8_t IMU_HasValidStartupAngle(void);
uint16_t IMU_GetPacketCount(void);
int16_t IMU_GetRawChannel(uint8_t index);
int16_t IMU_GetAccelXRaw(void);
int16_t IMU_GetAccelYRaw(void);
int16_t IMU_GetAccelZRaw(void);

#endif

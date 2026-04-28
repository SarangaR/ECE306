#ifndef OTOS_H_
#define OTOS_H_

#include "msp430.h"
#include <stdint.h>

#define OTOS_PI (3.14159265358979323846f)

#define OTOS_I2C_PORT (GPIO_PORT_P4)
#define OTOS_SDA_PIN (GPIO_PIN2)   /* P4.2 = SDA */
#define OTOS_SCL_PIN (GPIO_PIN3)   /* P4.3 = SCL */

#define OTOS_I2C_DELAY_CYCLES (16U)
#define OTOS_I2C_TIMEOUT_CYCLES (5000U)

#define OTOS_DEFAULT_ADDRESS (0x17U)
#define OTOS_PRODUCT_ID (0x5FU)

#define OTOS_REG_PRODUCT_ID (0x00U)
#define OTOS_REG_HW_VERSION (0x01U)
#define OTOS_REG_FW_VERSION (0x02U)
#define OTOS_REG_SCALAR_LINEAR (0x04U)
#define OTOS_REG_SCALAR_ANGULAR (0x05U)
#define OTOS_REG_IMU_CALIB (0x06U)
#define OTOS_REG_RESET (0x07U)
#define OTOS_REG_SIGNAL_PROCESS (0x0EU)
#define OTOS_REG_SELF_TEST (0x0FU)
#define OTOS_REG_OFF_XL (0x10U)
#define OTOS_REG_STATUS (0x1FU)
#define OTOS_REG_POS_XL (0x20U)
#define OTOS_REG_VEL_XL (0x26U)
#define OTOS_REG_ACC_XL (0x2CU)
#define OTOS_REG_POS_STD_XL (0x32U)
#define OTOS_REG_VEL_STD_XL (0x38U)
#define OTOS_REG_ACC_STD_XL (0x3EU)

#define OTOS_MIN_SCALAR (0.872f)
#define OTOS_MAX_SCALAR (1.127f)

#define OTOS_METER_TO_INCH (39.37f)
#define OTOS_RADIAN_TO_DEGREE (180.0f / OTOS_PI)
#define OTOS_DEGREE_TO_RADIAN (OTOS_PI / 180.0f)

#define OTOS_METER_TO_INT16 (32768.0f / 10.0f)
#define OTOS_INT16_TO_METER (1.0f / OTOS_METER_TO_INT16)
#define OTOS_MPS_TO_INT16 (32768.0f / 5.0f)
#define OTOS_INT16_TO_MPS (1.0f / OTOS_MPS_TO_INT16)
#define OTOS_MPSS_TO_INT16 (32768.0f / (16.0f * 9.80665f))
#define OTOS_INT16_TO_MPSS (1.0f / OTOS_MPSS_TO_INT16)
#define OTOS_RAD_TO_INT16 (32768.0f / OTOS_PI)
#define OTOS_INT16_TO_RAD (1.0f / OTOS_RAD_TO_INT16)
#define OTOS_RPS_TO_INT16 (32768.0f / (2000.0f * OTOS_DEGREE_TO_RADIAN))
#define OTOS_INT16_TO_RPS (1.0f / OTOS_RPS_TO_INT16)
#define OTOS_RPSS_TO_INT16 (32768.0f / (OTOS_PI * 1000.0f))
#define OTOS_INT16_TO_RPSS (1.0f / OTOS_RPSS_TO_INT16)

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
otos_err_t OTOS_FullReset(void);
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

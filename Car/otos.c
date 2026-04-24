#include "include/otos.h"
#include <driverlib.h>

#pragma diag_suppress 1527
#pragma diag_suppress 1530
#pragma diag_suppress 1531
#pragma diag_suppress 1544
#pragma diag_suppress 1546

typedef struct
{
  uint8_t initialized;
  sfe_otos_linear_unit_t linearUnit;
  sfe_otos_angular_unit_t angularUnit;
  float meterToUnit;
  float radToUnit;
  uint16_t packetCount;
  int16_t rawChannels[6];
  float headingZeroOffsetDeg;
} otos_state_t;

static otos_state_t g_otos = {
    0U,
    kSfeOtosLinearUnitInches,
    kSfeOtosAngularUnitDegrees,
    OTOS_METER_TO_INCH,
    OTOS_RADIAN_TO_DEGREE,
    0U,
    {0, 0, 0, 0, 0, 0},
    0.0f};

static float wrap_heading(float angle)
{
  while (angle > 180.0f)
  {
    angle -= 360.0f;
  }

  while (angle < -180.0f)
  {
    angle += 360.0f;
  }

  return angle;
}

static void otos_delay(void)
{
  __delay_cycles(OTOS_I2C_DELAY_CYCLES);
}

static void sda_high(void)
{
  GPIO_setAsInputPinWithPullUpResistor(OTOS_I2C_PORT, OTOS_SDA_PIN);
}

static void sda_low(void)
{
  GPIO_setOutputLowOnPin(OTOS_I2C_PORT, OTOS_SDA_PIN);
  GPIO_setAsOutputPin(OTOS_I2C_PORT, OTOS_SDA_PIN);
}

static void scl_high(void)
{
  GPIO_setAsInputPinWithPullUpResistor(OTOS_I2C_PORT, OTOS_SCL_PIN);
}

static void scl_low(void)
{
  GPIO_setOutputLowOnPin(OTOS_I2C_PORT, OTOS_SCL_PIN);
  GPIO_setAsOutputPin(OTOS_I2C_PORT, OTOS_SCL_PIN);
}

static uint8_t read_sda(void)
{
  return (GPIO_getInputPinValue(OTOS_I2C_PORT, OTOS_SDA_PIN) != 0U) ? 1U : 0U;
}

static uint8_t wait_scl_high(void)
{
  uint16_t timeout = OTOS_I2C_TIMEOUT_CYCLES;

  while ((GPIO_getInputPinValue(OTOS_I2C_PORT, OTOS_SCL_PIN) == 0U) && (timeout > 0U))
  {
    timeout--;
    otos_delay();
  }

  return (timeout > 0U) ? 1U : 0U;
}

static void i2c_start(void)
{
  sda_high();
  scl_high();
  otos_delay();
  sda_low();
  otos_delay();
  scl_low();
}

static void i2c_stop(void)
{
  sda_low();
  otos_delay();
  scl_high();
  wait_scl_high();
  otos_delay();
  sda_high();
  otos_delay();
}

static uint8_t i2c_write_byte(uint8_t byte)
{
  uint8_t bit;
  uint8_t ack;

  for (bit = 0U; bit < 8U; bit++)
  {
    if ((byte & 0x80U) != 0U)
    {
      sda_high();
    }
    else
    {
      sda_low();
    }

    otos_delay();
    scl_high();
    if (!wait_scl_high())
    {
      scl_low();
      return 0U;
    }
    otos_delay();
    scl_low();
    byte <<= 1;
  }

  sda_high();
  otos_delay();
  scl_high();
  if (!wait_scl_high())
  {
    scl_low();
    return 0U;
  }
  ack = (read_sda() == 0U) ? 1U : 0U;
  otos_delay();
  scl_low();

  return ack;
}

static uint8_t i2c_read_byte(uint8_t sendAck)
{
  uint8_t bit;
  uint8_t byte = 0U;

  sda_high();

  for (bit = 0U; bit < 8U; bit++)
  {
    byte <<= 1;
    otos_delay();
    scl_high();
    if (!wait_scl_high())
    {
      scl_low();
      return 0xFFU;
    }

    if (read_sda())
    {
      byte |= 0x01U;
    }

    otos_delay();
    scl_low();
  }

  if (sendAck)
  {
    sda_low();
  }
  else
  {
    sda_high();
  }

  otos_delay();
  scl_high();
  wait_scl_high();
  otos_delay();
  scl_low();
  sda_high();

  return byte;
}

static otos_err_t otos_write_register_bytes(uint8_t reg, const uint8_t *data, uint8_t length)
{
  uint8_t i;

  i2c_start();
  if (!i2c_write_byte((uint8_t)((OTOS_DEFAULT_ADDRESS << 1) | 0U)))
  {
    i2c_stop();
    return OTOS_ERR_FAIL;
  }

  if (!i2c_write_byte(reg))
  {
    i2c_stop();
    return OTOS_ERR_FAIL;
  }

  for (i = 0U; i < length; i++)
  {
    if (!i2c_write_byte(data[i]))
    {
      i2c_stop();
      return OTOS_ERR_FAIL;
    }
  }

  i2c_stop();
  return OTOS_ERR_OK;
}

static otos_err_t otos_read_register_bytes(uint8_t reg, uint8_t *data, uint8_t length)
{
  uint8_t i;

  i2c_start();
  if (!i2c_write_byte((uint8_t)((OTOS_DEFAULT_ADDRESS << 1) | 0U)))
  {
    i2c_stop();
    return OTOS_ERR_FAIL;
  }

  if (!i2c_write_byte(reg))
  {
    i2c_stop();
    return OTOS_ERR_FAIL;
  }

  i2c_start();
  if (!i2c_write_byte((uint8_t)((OTOS_DEFAULT_ADDRESS << 1) | 1U)))
  {
    i2c_stop();
    return OTOS_ERR_FAIL;
  }

  for (i = 0U; i < length; i++)
  {
    data[i] = i2c_read_byte((i + 1U) < length);
  }

  i2c_stop();
  return OTOS_ERR_OK;
}

static int16_t bytes_to_int16(const uint8_t *raw)
{
  return (int16_t)(((int16_t)raw[1] << 8) | raw[0]);
}

static void regs_to_pose(const uint8_t *rawData, sfe_otos_pose2d_t *pose, float rawToXY, float rawToH)
{
  int16_t rawX;
  int16_t rawY;
  int16_t rawH;

  rawX = bytes_to_int16(rawData);
  rawY = bytes_to_int16(rawData + 2);
  rawH = bytes_to_int16(rawData + 4);

  pose->x = (float)rawX * rawToXY * g_otos.meterToUnit;
  pose->y = (float)rawY * rawToXY * g_otos.meterToUnit;
  pose->h = (float)rawH * rawToH * g_otos.radToUnit;
}

static void pose_to_regs(uint8_t *rawData, const sfe_otos_pose2d_t *pose, float xyToRaw, float hToRaw)
{
  int16_t rawX;
  int16_t rawY;
  int16_t rawH;

  rawX = (int16_t)(pose->x * xyToRaw / g_otos.meterToUnit);
  rawY = (int16_t)(pose->y * xyToRaw / g_otos.meterToUnit);
  rawH = (int16_t)(pose->h * hToRaw / g_otos.radToUnit);

  rawData[0] = (uint8_t)(rawX & 0xFF);
  rawData[1] = (uint8_t)((rawX >> 8) & 0xFF);
  rawData[2] = (uint8_t)(rawY & 0xFF);
  rawData[3] = (uint8_t)((rawY >> 8) & 0xFF);
  rawData[4] = (uint8_t)(rawH & 0xFF);
  rawData[5] = (uint8_t)((rawH >> 8) & 0xFF);
}

static otos_err_t read_pose_regs(uint8_t reg, sfe_otos_pose2d_t *pose, float rawToXY, float rawToH)
{
  uint8_t rawData[6];

  if (otos_read_register_bytes(reg, rawData, 6U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  regs_to_pose(rawData, pose, rawToXY, rawToH);
  return OTOS_ERR_OK;
}

static otos_err_t write_pose_regs(uint8_t reg, const sfe_otos_pose2d_t *pose, float xyToRaw, float hToRaw)
{
  uint8_t rawData[6];

  pose_to_regs(rawData, pose, xyToRaw, hToRaw);
  return otos_write_register_bytes(reg, rawData, 6U);
}

static otos_err_t otos_poll_measurement(void)
{
  uint8_t rawData[18];
  int16_t rawPosH;
  otos_err_t result;

  result = otos_read_register_bytes(OTOS_REG_POS_XL, rawData, 18U);

  if (result != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  rawPosH = bytes_to_int16(rawData + 4);

  g_otos.rawChannels[0] = (int16_t)(((float)rawPosH * OTOS_INT16_TO_RAD * OTOS_RADIAN_TO_DEGREE) * 100.0f);
  g_otos.rawChannels[1] = bytes_to_int16(rawData);
  g_otos.rawChannels[2] = bytes_to_int16(rawData + 2);
  g_otos.rawChannels[3] = bytes_to_int16(rawData + 12);
  g_otos.rawChannels[4] = bytes_to_int16(rawData + 14);
  g_otos.rawChannels[5] = bytes_to_int16(rawData + 16);
  g_otos.packetCount++;

  return OTOS_ERR_OK;
}

uint8_t OTOS_Begin(void)
{
  uint8_t productId;

  sda_high();
  scl_high();

  g_otos.initialized = 1U;
  g_otos.packetCount = 0U;
  g_otos.rawChannels[0] = 0;
  g_otos.rawChannels[1] = 0;
  g_otos.rawChannels[2] = 0;
  g_otos.rawChannels[3] = 0;
  g_otos.rawChannels[4] = 0;
  g_otos.rawChannels[5] = 0;
  g_otos.headingZeroOffsetDeg = 0.0f;
  g_otos.linearUnit = kSfeOtosLinearUnitInches;
  g_otos.angularUnit = kSfeOtosAngularUnitDegrees;
  g_otos.meterToUnit = OTOS_METER_TO_INCH;
  g_otos.radToUnit = OTOS_RADIAN_TO_DEGREE;

  if (otos_read_register_bytes(OTOS_REG_PRODUCT_ID, &productId, 1U) != OTOS_ERR_OK)
  {
    return 0U;
  }

  return (productId == OTOS_PRODUCT_ID) ? 1U : 0U;
}

otos_err_t OTOS_IsConnected(void)
{
  uint8_t productId;

  if (otos_read_register_bytes(OTOS_REG_PRODUCT_ID, &productId, 1U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  return (productId == OTOS_PRODUCT_ID) ? OTOS_ERR_OK : OTOS_ERR_FAIL;
}

otos_err_t OTOS_GetVersionInfo(sfe_otos_version_t *hwVersion, sfe_otos_version_t *fwVersion)
{
  uint8_t rawData[2];

  if ((hwVersion == 0) || (fwVersion == 0))
  {
    return OTOS_ERR_FAIL;
  }

  if (otos_read_register_bytes(OTOS_REG_HW_VERSION, rawData, 2U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  hwVersion->value = rawData[0];
  fwVersion->value = rawData[1];

  return OTOS_ERR_OK;
}

otos_err_t OTOS_SelfTest(void)
{
  sfe_otos_self_test_config_t selfTest;
  uint8_t i;

  selfTest.value = 0U;
  selfTest.start = 1U;

  if (otos_write_register_bytes(OTOS_REG_SELF_TEST, &selfTest.value, 1U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  for (i = 0U; i < 10U; i++)
  {
    __delay_cycles(40000U);

    if (otos_read_register_bytes(OTOS_REG_SELF_TEST, &selfTest.value, 1U) != OTOS_ERR_OK)
    {
      return OTOS_ERR_FAIL;
    }

    if (selfTest.inProgress == 0U)
    {
      break;
    }
  }

  return (selfTest.pass == 1U) ? OTOS_ERR_OK : OTOS_ERR_FAIL;
}

otos_err_t OTOS_CalibrateImu(uint8_t numSamples, uint8_t waitUntilDone)
{
  uint8_t attempts;
  uint8_t calibrationValue;

  if (otos_write_register_bytes(OTOS_REG_IMU_CALIB, &numSamples, 1U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  __delay_cycles(24000U);

  if (waitUntilDone == 0U)
  {
    return OTOS_ERR_OK;
  }

  attempts = numSamples;
  while (attempts > 0U)
  {
    if (otos_read_register_bytes(OTOS_REG_IMU_CALIB, &calibrationValue, 1U) != OTOS_ERR_OK)
    {
      return OTOS_ERR_FAIL;
    }

    if (calibrationValue == 0U)
    {
      return OTOS_ERR_OK;
    }

    __delay_cycles(24000U);
    attempts--;
  }

  return OTOS_ERR_FAIL;
}

otos_err_t OTOS_GetImuCalibrationProgress(uint8_t *numSamples)
{
  if (numSamples == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return otos_read_register_bytes(OTOS_REG_IMU_CALIB, numSamples, 1U);
}

sfe_otos_linear_unit_t OTOS_GetLinearUnit(void)
{
  return g_otos.linearUnit;
}

void OTOS_SetLinearUnit(sfe_otos_linear_unit_t unit)
{
  g_otos.linearUnit = unit;
  g_otos.meterToUnit = (unit == kSfeOtosLinearUnitMeters) ? 1.0f : OTOS_METER_TO_INCH;
}

sfe_otos_angular_unit_t OTOS_GetAngularUnit(void)
{
  return g_otos.angularUnit;
}

void OTOS_SetAngularUnit(sfe_otos_angular_unit_t unit)
{
  g_otos.angularUnit = unit;
  g_otos.radToUnit = (unit == kSfeOtosAngularUnitRadians) ? 1.0f : OTOS_RADIAN_TO_DEGREE;
}

otos_err_t OTOS_GetLinearScalar(float *scalar)
{
  int8_t rawScalar;
  uint8_t raw;

  if (scalar == 0)
  {
    return OTOS_ERR_FAIL;
  }

  if (otos_read_register_bytes(OTOS_REG_SCALAR_LINEAR, &raw, 1U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  rawScalar = (int8_t)raw;
  *scalar = ((float)rawScalar * 0.001f) + 1.0f;
  return OTOS_ERR_OK;
}

otos_err_t OTOS_SetLinearScalar(float scalar)
{
  int16_t scaled;
  int8_t rawScalar;
  uint8_t raw;

  if ((scalar < OTOS_MIN_SCALAR) || (scalar > OTOS_MAX_SCALAR))
  {
    return OTOS_ERR_FAIL;
  }

  scaled = (int16_t)((scalar - 1.0f) * 1000.0f + 0.5f);
  rawScalar = (int8_t)scaled;
  raw = (uint8_t)rawScalar;

  return otos_write_register_bytes(OTOS_REG_SCALAR_LINEAR, &raw, 1U);
}

otos_err_t OTOS_GetAngularScalar(float *scalar)
{
  int8_t rawScalar;
  uint8_t raw;

  if (scalar == 0)
  {
    return OTOS_ERR_FAIL;
  }

  if (otos_read_register_bytes(OTOS_REG_SCALAR_ANGULAR, &raw, 1U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  rawScalar = (int8_t)raw;
  *scalar = ((float)rawScalar * 0.001f) + 1.0f;
  return OTOS_ERR_OK;
}

otos_err_t OTOS_SetAngularScalar(float scalar)
{
  int16_t scaled;
  int8_t rawScalar;
  uint8_t raw;

  if ((scalar < OTOS_MIN_SCALAR) || (scalar > OTOS_MAX_SCALAR))
  {
    return OTOS_ERR_FAIL;
  }

  scaled = (int16_t)((scalar - 1.0f) * 1000.0f + 0.5f);
  rawScalar = (int8_t)scaled;
  raw = (uint8_t)rawScalar;

  return otos_write_register_bytes(OTOS_REG_SCALAR_ANGULAR, &raw, 1U);
}

otos_err_t OTOS_ResetTracking(void)
{
  uint8_t value = 0x01U;
  return otos_write_register_bytes(OTOS_REG_RESET, &value, 1U);
}

otos_err_t OTOS_GetSignalProcessConfig(sfe_otos_signal_process_config_t *config)
{
  if (config == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return otos_read_register_bytes(OTOS_REG_SIGNAL_PROCESS, &config->value, 1U);
}

otos_err_t OTOS_SetSignalProcessConfig(const sfe_otos_signal_process_config_t *config)
{
  if (config == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return otos_write_register_bytes(OTOS_REG_SIGNAL_PROCESS, &config->value, 1U);
}

otos_err_t OTOS_GetStatus(sfe_otos_status_t *status)
{
  if (status == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return otos_read_register_bytes(OTOS_REG_STATUS, &status->value, 1U);
}

otos_err_t OTOS_GetOffset(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_OFF_XL, pose, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
}

otos_err_t OTOS_SetOffset(const sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return write_pose_regs(OTOS_REG_OFF_XL, pose, OTOS_METER_TO_INT16, OTOS_RAD_TO_INT16);
}

otos_err_t OTOS_GetPosition(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_POS_XL, pose, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
}

otos_err_t OTOS_SetPosition(const sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return write_pose_regs(OTOS_REG_POS_XL, pose, OTOS_METER_TO_INT16, OTOS_RAD_TO_INT16);
}

otos_err_t OTOS_GetVelocity(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_VEL_XL, pose, OTOS_INT16_TO_MPS, OTOS_INT16_TO_RPS);
}

otos_err_t OTOS_GetAcceleration(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_ACC_XL, pose, OTOS_INT16_TO_MPSS, OTOS_INT16_TO_RPSS);
}

otos_err_t OTOS_GetPositionStdDev(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_POS_STD_XL, pose, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
}

otos_err_t OTOS_GetVelocityStdDev(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_VEL_STD_XL, pose, OTOS_INT16_TO_MPS, OTOS_INT16_TO_RPS);
}

otos_err_t OTOS_GetAccelerationStdDev(sfe_otos_pose2d_t *pose)
{
  if (pose == 0)
  {
    return OTOS_ERR_FAIL;
  }

  return read_pose_regs(OTOS_REG_ACC_STD_XL, pose, OTOS_INT16_TO_MPSS, OTOS_INT16_TO_RPSS);
}

otos_err_t OTOS_GetPosVelAcc(sfe_otos_pose2d_t *pos, sfe_otos_pose2d_t *vel, sfe_otos_pose2d_t *acc)
{
  uint8_t rawData[18];

  if ((pos == 0) || (vel == 0) || (acc == 0))
  {
    return OTOS_ERR_FAIL;
  }

  if (otos_read_register_bytes(OTOS_REG_POS_XL, rawData, 18U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  regs_to_pose(rawData, pos, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
  regs_to_pose(rawData + 6, vel, OTOS_INT16_TO_MPS, OTOS_INT16_TO_RPS);
  regs_to_pose(rawData + 12, acc, OTOS_INT16_TO_MPSS, OTOS_INT16_TO_RPSS);

  return OTOS_ERR_OK;
}

otos_err_t OTOS_GetPosVelAccStdDev(sfe_otos_pose2d_t *pos, sfe_otos_pose2d_t *vel, sfe_otos_pose2d_t *acc)
{
  uint8_t rawData[18];

  if ((pos == 0) || (vel == 0) || (acc == 0))
  {
    return OTOS_ERR_FAIL;
  }

  if (otos_read_register_bytes(OTOS_REG_POS_STD_XL, rawData, 18U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  regs_to_pose(rawData, pos, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
  regs_to_pose(rawData + 6, vel, OTOS_INT16_TO_MPS, OTOS_INT16_TO_RPS);
  regs_to_pose(rawData + 12, acc, OTOS_INT16_TO_MPSS, OTOS_INT16_TO_RPSS);

  return OTOS_ERR_OK;
}

otos_err_t OTOS_GetPosVelAccAndStdDev(sfe_otos_pose2d_t *pos, sfe_otos_pose2d_t *vel, sfe_otos_pose2d_t *acc,
                                       sfe_otos_pose2d_t *posStdDev, sfe_otos_pose2d_t *velStdDev,
                                       sfe_otos_pose2d_t *accStdDev)
{
  uint8_t rawData[36];

  if ((pos == 0) || (vel == 0) || (acc == 0) || (posStdDev == 0) || (velStdDev == 0) || (accStdDev == 0))
  {
    return OTOS_ERR_FAIL;
  }

  if (otos_read_register_bytes(OTOS_REG_POS_XL, rawData, 36U) != OTOS_ERR_OK)
  {
    return OTOS_ERR_FAIL;
  }

  regs_to_pose(rawData, pos, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
  regs_to_pose(rawData + 6, vel, OTOS_INT16_TO_MPS, OTOS_INT16_TO_RPS);
  regs_to_pose(rawData + 12, acc, OTOS_INT16_TO_MPSS, OTOS_INT16_TO_RPSS);
  regs_to_pose(rawData + 18, posStdDev, OTOS_INT16_TO_METER, OTOS_INT16_TO_RAD);
  regs_to_pose(rawData + 24, velStdDev, OTOS_INT16_TO_MPS, OTOS_INT16_TO_RPS);
  regs_to_pose(rawData + 30, accStdDev, OTOS_INT16_TO_MPSS, OTOS_INT16_TO_RPSS);

  return OTOS_ERR_OK;
}

void Init_IMU(void)
{
  if (OTOS_Begin())
  {
    OTOS_CalibrateImu(255U, 1U);
  }
}

void IMU_Process(void)
{
  if (!g_otos.initialized)
  {
    return;
  }

  (void)otos_poll_measurement();
}

float getHeading(void)
{
  float heading = ((float)g_otos.rawChannels[0] * 0.01f) - g_otos.headingZeroOffsetDeg;
  return wrap_heading(heading);
}

void zeroHeading(void)
{
  IMU_Process();
  g_otos.headingZeroOffsetDeg = (float)g_otos.rawChannels[0] * 0.01f;
}

uint8_t IMU_HasValidStartupAngle(void)
{
  uint16_t i;
  uint16_t packetStart;

  packetStart = IMU_GetPacketCount();

  for (i = 0U; i < 3000U; i++)
  {
    IMU_Process();
    if ((uint16_t)(IMU_GetPacketCount() - packetStart) >= 5U)
    {
      return 1U;
    }
    __delay_cycles(5000U);
  }

  return 0U;
}

uint16_t IMU_GetPacketCount(void)
{
  return g_otos.packetCount;
}

int16_t IMU_GetRawChannel(uint8_t index)
{
  if (index >= 6U)
  {
    return 0;
  }

  return g_otos.rawChannels[index];
}

float getPositionX(void)
{
  /* rawChannels[1] holds raw X position; convert to inches */
  return (float)g_otos.rawChannels[1] * OTOS_INT16_TO_METER * OTOS_METER_TO_INCH;
}

float getPositionY(void)
{
  /* rawChannels[2] holds raw Y position; convert to inches */
  return (float)g_otos.rawChannels[2] * OTOS_INT16_TO_METER * OTOS_METER_TO_INCH;
}

int16_t IMU_GetAccelXRaw(void)
{
  return g_otos.rawChannels[3];
}

int16_t IMU_GetAccelYRaw(void)
{
  return g_otos.rawChannels[4];
}

int16_t IMU_GetAccelZRaw(void)
{
  return g_otos.rawChannels[5];
}

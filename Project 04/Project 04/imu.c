#include "Include\imu.h"

#define IMU_PACKET_HEADER (0xAA)
#define IMU_PACKET_BYTES (19)
#define IMU_PAYLOAD_BYTES (17)
#define DEGREE_SCALE (0.01f)

static uint8_t packet_buffer[IMU_PACKET_BYTES];
static uint8_t packet_index = 0;
static float yaw_raw = 0.0f;
static float yaw_zero_offset = 0.0f;

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

static void decode_packet(void)
{
  uint8_t sum = 0;
  uint8_t i;
  int16_t yaw_counts;

  for (i = 2; i < 18; i++)
  {
    sum += packet_buffer[i];
  }

  if (sum != packet_buffer[18])
  {
    return;
  }

  yaw_counts = (int16_t)packet_buffer[3];
  yaw_counts += ((int16_t)packet_buffer[4] << 8);
  yaw_raw = ((float)yaw_counts) * DEGREE_SCALE;
}

void Init_IMU(void)
{
  UCA1CTLW0 = UCSWRST;
  UCA1CTLW0 |= UCSSEL__SMCLK;

  UCA1BRW = 4;
  UCA1MCTLW = ((0x55 << 8) | (5 << 4) | UCOS16);

  UCA1CTLW0 &= ~UCSWRST;

  packet_index = 0;
  yaw_raw = 0.0f;
  yaw_zero_offset = 0.0f;
}

void IMU_Process(void)
{
  uint8_t incoming;

  while (UCA1IFG & UCRXIFG)
  {
    incoming = UCA1RXBUF;

    if ((packet_index == 0) && (incoming != IMU_PACKET_HEADER))
    {
      continue;
    }

    if ((packet_index == 1) && (incoming != IMU_PACKET_HEADER))
    {
      packet_index = 0;
      continue;
    }

    packet_buffer[packet_index++] = incoming;

    if (packet_index >= IMU_PACKET_BYTES)
    {
      decode_packet();
      packet_index = 0;
    }
  }
}

float getHeading(void)
{
  float heading;

  IMU_Process();
  heading = yaw_raw - yaw_zero_offset;

  return wrap_heading(heading);
}

void zeroHeading(void)
{
  IMU_Process();
  yaw_zero_offset = yaw_raw;
}

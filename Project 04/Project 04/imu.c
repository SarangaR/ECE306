#include "Include\imu.h"

#define IMU_PACKET_HEADER (0xAA)
#define IMU_PACKET_BYTES (19)
#define IMU_PAYLOAD_BYTES (17)
#define DEGREE_SCALE (0.01f)
#define IMU_RX_BUFFER_SIZE (128)

static volatile uint8_t rx_buffer[IMU_RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static float yaw_raw = 0.0f;
static float yaw_zero_offset = 0.0f;

static uint8_t rx_available(void)
{
  return (uint8_t)((rx_head - rx_tail) & (IMU_RX_BUFFER_SIZE - 1));
}

static uint8_t rx_peek(uint8_t *byte_out)
{
  if (rx_head == rx_tail)
  {
    return 0;
  }

  *byte_out = rx_buffer[rx_tail];
  return 1;
}

static uint8_t rx_read(uint8_t *byte_out)
{
  if (rx_head == rx_tail)
  {
    return 0;
  }

  *byte_out = rx_buffer[rx_tail];
  rx_tail = (uint8_t)((rx_tail + 1U) & (IMU_RX_BUFFER_SIZE - 1));
  return 1;
}

static uint8_t rx_read_bytes(uint8_t *buffer, uint8_t count)
{
  uint8_t i;

  if (rx_available() < count)
  {
    return 0;
  }

  for (i = 0; i < count; i++)
  {
    rx_read(&buffer[i]);
  }

  return 1;
}

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

static uint8_t imu_read_yaw(float *yaw_out)
{
  uint8_t buffer[IMU_PAYLOAD_BYTES];
  int16_t buffer_16[6];
  uint8_t sum;
  uint8_t i;
  uint8_t incoming;

  if (!yaw_out)
  {
    return 0;
  }

  if (!rx_available())
  {
    return 0;
  }

  if (!rx_peek(&incoming))
  {
    return 0;
  }

  if (incoming != IMU_PACKET_HEADER)
  {
    rx_read(&incoming);
    return 0;
  }

  if (rx_available() < IMU_PACKET_BYTES)
  {
    return 0;
  }

  if (!rx_read(&incoming) || (incoming != IMU_PACKET_HEADER))
  {
    return 0;
  }

  if (!rx_read(&incoming) || (incoming != IMU_PACKET_HEADER))
  {
    return 0;
  }

  if (!rx_read_bytes(buffer, IMU_PAYLOAD_BYTES))
  {
    return 0;
  }

  sum = 0;
  for (i = 0; i < 16; i++)
  {
    sum += buffer[i];
  }

  if (sum != buffer[16])
  {
    return 0;
  }

  for (i = 0; i < 6; i++)
  {
    buffer_16[i] = (int16_t)buffer[1 + (i * 2)];
    buffer_16[i] += ((int16_t)buffer[1 + (i * 2) + 1] << 8);
  }

  *yaw_out = ((float)buffer_16[0]) * DEGREE_SCALE;
  return 1;
}

void Init_IMU(void)
{
  UCA1CTLW0 = UCSWRST;
  UCA1CTLW0 |= UCSSEL__SMCLK;

  UCA1BRW = 4;
  UCA1MCTLW = ((0x55 << 8) | (5 << 4) | UCOS16);

  UCA1IE &= ~UCRXIE;
  UCA1IFG &= ~UCRXIFG;
  UCA1CTLW0 &= ~UCSWRST;
  UCA1IE |= UCRXIE;

  rx_head = 0;
  rx_tail = 0;
  yaw_raw = 0.0f;
  yaw_zero_offset = 0.0f;
}

void IMU_Process(void)
{
  float next_yaw;

  while (imu_read_yaw(&next_yaw))
  {
    yaw_raw = next_yaw;
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

#pragma vector=EUSCI_A1_VECTOR
__interrupt void EUSCI_A1_ISR(void)
{
  if (UCA1IFG & UCRXIFG)
  {
    uint8_t incoming = UCA1RXBUF;
    uint8_t next_head = (uint8_t)((rx_head + 1U) & (IMU_RX_BUFFER_SIZE - 1));

    if (next_head != rx_tail)
    {
      rx_buffer[rx_head] = incoming;
      rx_head = next_head;
    }
  }
}

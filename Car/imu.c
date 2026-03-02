#include "Include\imu.h"
#include <driverlib.h>

#define IMU_PACKET_HEADER (0xAAU)
#define IMU_PAYLOAD_BYTES (17U)
#define IMU_CHECKSUM_BYTES (16U)
#define IMU_UART_BASE (EUSCI_A1_BASE)
#define DEGREE_SCALE (0.01f)

typedef enum
{
  IMU_PARSE_WAIT_HEADER_1 = 0,
  IMU_PARSE_WAIT_HEADER_2,
  IMU_PARSE_PAYLOAD
} ImuParseState;

static volatile ImuParseState imu_parse_state = IMU_PARSE_WAIT_HEADER_1;
static volatile uint8_t imu_payload_buffer[IMU_PAYLOAD_BYTES];
static volatile uint8_t imu_payload_index = 0;
static volatile uint16_t imu_packet_count = 0;
static volatile int16_t imu_raw_channels[6];
static volatile int16_t yaw_raw_counts = 0;
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

static void imu_reset_parser(void)
{
  imu_parse_state = IMU_PARSE_WAIT_HEADER_1;
  imu_payload_index = 0;
}

static void imu_process_payload(void)
{
  uint8_t i;
  uint8_t checksum = 0;
  int16_t channel_value;

  for (i = 0; i < IMU_CHECKSUM_BYTES; i++)
  {
    checksum += imu_payload_buffer[i];
  }

  if (checksum != imu_payload_buffer[IMU_CHECKSUM_BYTES])
  {
    return;
  }

  for (i = 0; i < 6U; i++)
  {
    channel_value = (int16_t)imu_payload_buffer[1U + (i * 2U)];
    channel_value += ((int16_t)imu_payload_buffer[2U + (i * 2U)] << 8);
    imu_raw_channels[i] = channel_value;
  }

  yaw_raw_counts = imu_raw_channels[0];
  imu_packet_count++;
}

static void imu_consume_byte(uint8_t incoming)
{
  if (imu_parse_state == IMU_PARSE_WAIT_HEADER_1)
  {
    if (incoming == IMU_PACKET_HEADER)
    {
      imu_parse_state = IMU_PARSE_WAIT_HEADER_2;
    }
    return;
  }

  if (imu_parse_state == IMU_PARSE_WAIT_HEADER_2)
  {
    if (incoming == IMU_PACKET_HEADER)
    {
      imu_parse_state = IMU_PARSE_PAYLOAD;
      imu_payload_index = 0;
    }
    else
    {
      imu_parse_state = IMU_PARSE_WAIT_HEADER_1;
    }
    return;
  }

  imu_payload_buffer[imu_payload_index] = incoming;
  imu_payload_index++;

  if (imu_payload_index >= IMU_PAYLOAD_BYTES)
  {
    imu_process_payload();
    imu_reset_parser();
  }
}

void Init_IMU(void)
{
  EUSCI_A_UART_initParam uart_params = {0};
  uint8_t dummy;

  uart_params.selectClockSource = EUSCI_A_UART_CLOCKSOURCE_SMCLK;
  uart_params.clockPrescalar = 4U;
  uart_params.firstModReg = 5U;
  uart_params.secondModReg = 0x55U;
  uart_params.parity = EUSCI_A_UART_NO_PARITY;
  uart_params.msborLsbFirst = EUSCI_A_UART_LSB_FIRST;
  uart_params.numberofStopBits = EUSCI_A_UART_ONE_STOP_BIT;
  uart_params.uartMode = EUSCI_A_UART_MODE;
  uart_params.overSampling = EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION;

  EUSCI_A_UART_disable(IMU_UART_BASE);
  EUSCI_A_UART_disableInterrupt(IMU_UART_BASE,
                                EUSCI_A_UART_RECEIVE_INTERRUPT |
                                    EUSCI_A_UART_TRANSMIT_INTERRUPT |
                                    EUSCI_A_UART_RECEIVE_ERRONEOUSCHAR_INTERRUPT |
                                    EUSCI_A_UART_BREAKCHAR_INTERRUPT |
                                    EUSCI_A_UART_STARTBIT_INTERRUPT |
                                    EUSCI_A_UART_TRANSMIT_COMPLETE_INTERRUPT);

  EUSCI_A_UART_init(IMU_UART_BASE, &uart_params);
  EUSCI_A_UART_enable(IMU_UART_BASE);

  EUSCI_A_UART_clearInterrupt(IMU_UART_BASE,
                              EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG |
                                  EUSCI_A_UART_TRANSMIT_INTERRUPT_FLAG |
                                  EUSCI_A_UART_STARTBIT_INTERRUPT_FLAG |
                                  EUSCI_A_UART_TRANSMIT_COMPLETE_INTERRUPT_FLAG);

  while (EUSCI_A_UART_getInterruptStatus(IMU_UART_BASE,
                                         EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG))
  {
    dummy = EUSCI_A_UART_receiveData(IMU_UART_BASE);
    (void)dummy;
  }

  imu_packet_count = 0;
  imu_raw_channels[0] = 0;
  imu_raw_channels[1] = 0;
  imu_raw_channels[2] = 0;
  imu_raw_channels[3] = 0;
  imu_raw_channels[4] = 0;
  imu_raw_channels[5] = 0;
  yaw_raw_counts = 0;
  yaw_zero_offset = 0.0f;
  imu_reset_parser();

  EUSCI_A_UART_enableInterrupt(IMU_UART_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
}

void IMU_Process(void)
{
  /* Parsing happens in the UART RX ISR. */
}

float getHeading(void)
{
  float heading;

  IMU_Process();
  heading = ((float)yaw_raw_counts * DEGREE_SCALE) - yaw_zero_offset;
  return wrap_heading(heading);
}

void zeroHeading(void)
{
  IMU_Process();
  yaw_zero_offset = ((float)yaw_raw_counts) * DEGREE_SCALE;
}

uint8_t IMU_HasValidStartupAngle(void)
{
  uint16_t i;
  uint16_t packet_start;

  packet_start = IMU_GetPacketCount();

  for (i = 0; i < 3000U; i++)
  {
    IMU_Process();
    if ((uint16_t)(IMU_GetPacketCount() - packet_start) >= 5U)
    {
      return 1U;
    }
    __delay_cycles(5000);
  }

  return 0U;
}

uint16_t IMU_GetPacketCount(void)
{
  return imu_packet_count;
}

int16_t IMU_GetRawChannel(uint8_t index)
{
  if (index >= 6U)
  {
    return 0;
  }

  return imu_raw_channels[index];
}

int16_t IMU_GetAccelXRaw(void)
{
  return imu_raw_channels[3];
}

int16_t IMU_GetAccelYRaw(void)
{
  return imu_raw_channels[4];
}

int16_t IMU_GetAccelZRaw(void)
{
  return imu_raw_channels[5];
}

#pragma vector=EUSCI_A1_VECTOR
__interrupt void EUSCI_A1_ISR(void)
{
  if (EUSCI_A_UART_getInterruptStatus(IMU_UART_BASE,
                                      EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG))
  {
    imu_consume_byte(EUSCI_A_UART_receiveData(IMU_UART_BASE));
  }
}

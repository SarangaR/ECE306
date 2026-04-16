#include "include/serial.h"
#include "include/ports.h"
#include "include/macros.h"
#include <msp430.h>

static volatile char    s_rx_buf[BUF_LEN];   /* ISR writes bytes here        */
static volatile uint8_t s_rx_idx = 0U;       /* current write position       */
static char             s_frame[BUF_LEN];    /* last complete frame          */
static volatile uint8_t s_frame_ready = 0U;  /* 1 when frame is waiting      */

static void (*s_iot_rx_hook)(char) = 0;

void uart_set_iot_rx_hook(void (*hook)(char))
{
    s_iot_rx_hook = hook;
}

void uart_init(void)
{
    UCA0CTL1 |=  UCSWRST;
    UCA0CTL1  =  UCSSEL_2 | UCSWRST;   /* SMCLK = 8 MHz                     */
    UCA0CTL0  =  0;
    UCA0BRW   =  4;                     /* floor(8e6 / (16 * 115200)) = 4    */
    UCA0MCTLW =  0x5551;                /* UCBRSx=0x55 UCBRFx=5 UCOS16=1    */
    P1SEL0   |=  UCA0RXD | UCA0TXD;
    P1SEL1   &= ~(UCA0RXD | UCA0TXD);
    UCA0CTL1 &= ~UCSWRST;
    UCA0IE   |=  UCRXIE;
}

void uart_send_buf(const char *src)
{
    while (*src != '\0') {
        while (!(UCA0IFG & UCTXIFG));
        UCA0TXBUF = (uint8_t)*src++;
    }
    while (!(UCA0IFG & UCTXIFG)); UCA0TXBUF = '\r';
    while (!(UCA0IFG & UCTXIFG)); UCA0TXBUF = '\n';
}

uint8_t uart_read_frame(char *dst)
{
    uint8_t i = 0U;
    if (!s_frame_ready) { return 0U; }
    __disable_interrupt();
    while (i < (BUF_LEN - 1U) && s_frame[i] != '\0') {
        dst[i] = s_frame[i];
        i++;
    }
    dst[i]        = '\0';
    s_frame_ready = 0U;
    __enable_interrupt();
    return (i > 0U) ? 1U : 0U;
}

const char *uart_get_last_frame(void)
{
    return s_frame;
}

#pragma vector = EUSCI_A0_VECTOR
__interrupt void EUSCI_A0_RX_ISR(void)
{
    char    rx  = (char)UCA0RXBUF;
    uint8_t idx = s_rx_idx;

    if (s_iot_rx_hook) { s_iot_rx_hook(rx); }

    if (rx == '\n') {
        if (idx > 0U) {
            uint8_t j;
            for (j = 0U; j < idx; j++) {
                s_frame[j] = s_rx_buf[j];
            }
            s_frame[idx]  = '\0';
            s_frame_ready = 1U;
        }
        s_rx_idx = 0U;
    } else if (rx != '\r' && idx < (BUF_LEN - 1U)) {
        s_rx_buf[idx] = rx;
        s_rx_idx      = idx + 1U;
    }
}

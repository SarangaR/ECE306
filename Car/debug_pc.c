#include <msp430.h>
#include "include/debug_pc.h"
#include "include/serial.h"
#include "include/ports.h"

/*===========================================================================
 *  Internal state
 *===========================================================================*/

/* Set to 1 the first time a character is received from the PC.
   The FRAM must not transmit to the PC before this (project spec step 13). */
static volatile unsigned char s_pc_ready = 0U;

/* Set to 1 by Debug_PC_Init(); guards the ISR so it is inert if init was
   never called (e.g. after the debug include is removed from main.c).    */
static volatile unsigned char s_debug_enabled = 0U;

static void forward_to_pc(char c)
{
    if (!s_pc_ready)      { return; }
    if (!s_debug_enabled) { return; }
    if (!(UCA1IFG & UCTXIFG)) { return; }  /* TX busy — drop rather than block in ISR */
    UCA1TXBUF = (unsigned char)c;
}

/*---------------------------------------------------------------------------
 *  Debug_PC_Init
 *  -----------------------------------------------------------------------
 *  Configures UCA1 for 115,200 Baud, 8 data bits, no parity, 1 stop bit
 *  on P4.2 (RXD) / P4.3 (TXD), enables the UCA1 RX interrupt, and
 *  registers forward_to_pc() as the UCA0 receive hook so ESP32 responses
 *  are automatically forwarded to the PC terminal.
 *
 *  Call once from main().  Comment out that call to disable everything.
 *---------------------------------------------------------------------------*/
void Debug_PC_Init(void)
{
    s_pc_ready    = 0U;
    s_debug_enabled = 1U;

    /* --- Configure UCA1: 115,200 Baud, 8N1, SMCLK source (8 MHz) --- */
    UCA1CTL1 |=  UCSWRST;                          /* Hold in reset         */
    UCA1CTL1  =  UCSSEL_2 | UCSWRST;              /* SMCLK                 */
    UCA1CTL0  =  0;                                /* 8-bit, no parity, 1 stop */
    UCA1BRW   =  4;                                /* Baud-rate integer     */
    UCA1MCTLW =  (5U << 8) | (3U << 4) | UCOS16; /* 115,200 @ 8 MHz       */

    /* Route P4.2/P4.3 to UCA1 function (SEL0=1, SEL1=0)                */
    P4SEL0   |=  UCA1RXD | UCA1TXD;
    P4SEL1   &= ~(UCA1RXD | UCA1TXD);

    UCA1CTL1 &= ~UCSWRST;                         /* Release from reset    */
    UCA1IE   |=  UCRXIE;                          /* Enable RX interrupt   */

    /* Register the IOT→PC forwarding hook in serial.c                   */
    uart_set_iot_rx_hook(forward_to_pc);
}

/*===========================================================================
 *  UCA1 Receive ISR  —  PC terminal → ESP32 (IOT) pass-through
 *  -----------------------------------------------------------------------
 *  Every character typed in the PC terminal arrives here.
 *  1. The first character sets s_pc_ready, unlocking the PC TX path.
 *  2. The character is forwarded straight to UCA0 TX (ESP32).
 *     Blocking wait on UCA0 TX is safe here because the ESP32 baud rate
 *     matches UCA1 and the wait is at most one character period (~9 µs).
 *===========================================================================*/
#pragma vector = EUSCI_A1_VECTOR
__interrupt void EUSCI_A1_RX_ISR(void)
{
    if (UCA1IFG & UCRXIFG)
    {
        unsigned char temp = UCA1RXBUF;   /* Read clears the interrupt flag */

        if (!s_debug_enabled) { return; }

        s_pc_ready = 1U;                  /* PC is now active — allow TX   */

        /* Forward to ESP32 via UCA0 TX */
        while (!(UCA0IFG & UCTXIFG));
        UCA0TXBUF = temp;
    }
}

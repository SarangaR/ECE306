#include  "msp430.h"
#include  "Include\functions.h"
#include  "Include\ports.h"

void Init_Ports(void){
    Init_Port1();
    Init_Port2();
    Init_Port3();
    Init_Port4();
    Init_Port5();
    Init_Port6();
}


void Init_Port1(void){

    P1OUT = 0x00; // P1 set Low
    P1DIR = 0x00; // Set P1 direction to output

    P1SEL0 &= ~RED_LED;
    P1SEL1 &= ~RED_LED;
    P1OUT &= ~RED_LED;
    P1DIR |= RED_LED;

    P1SEL0 |= A1_SEEED;
    P1SEL1 |= A1_SEEED;
    P1OUT &= ~A1_SEEED;
    P1DIR &= ~A1_SEEED;

    P1SEL0 |= V_DETECT_L;
    P1SEL1 |= V_DETECT_L;
    P1OUT &= ~V_DETECT_L;
    P1DIR &= ~V_DETECT_L;

    P1SEL0 |= V_DETECT_R;
    P1SEL1 |= V_DETECT_R;
    P1OUT &= ~V_DETECT_R;
    P1DIR &= ~V_DETECT_R;

    P1SEL0 |= A4_SEEED;
    P1SEL1 |= A4_SEEED;
    P1OUT &= ~A4_SEEED;
    P1DIR &= ~A4_SEEED;

    P1SEL0 |= V_THUMB;
    P1SEL1 |= V_THUMB;
    P1OUT &= ~V_THUMB;
    P1DIR &= ~V_THUMB;

    P1SEL0 |= UCA0RXD;
    P1SEL1 &= ~UCA0RXD;
    P1OUT &= ~UCA0RXD;
    P1DIR &= ~UCA0RXD;

    P1SEL0 |= UCA0TXD;
    P1SEL1 &= ~UCA0TXD;
    P1OUT &= ~UCA0TXD;
    P1DIR &= ~UCA0TXD;
}

 void Init_Port2(void){ // Configure Port 2
     //------------------------------------------------------------------------------
     P2OUT = 0x00; // P2 set Low
     P2DIR = 0x00; // Set P2 direction to output


     P2SEL0 &= ~SLOW_CLK; // SLOW_CLK GPIO operation
     P2SEL1 &= ~SLOW_CLK; // SLOW_CLK GPIO operation
     P2OUT &= ~SLOW_CLK; // Initial Value = Low / Off
     P2DIR |= SLOW_CLK; // Direction = output

     P2SEL0 &= ~CHECK_BAT; // CHECK_BAT GPIO operation
     P2SEL1 &= ~CHECK_BAT; // CHECK_BAT GPIO operation
     P2OUT &= ~CHECK_BAT; // Initial Value = Low / Off
     P2DIR |= CHECK_BAT; // Direction = output


     P2SEL0 &= ~IR_LED; // P2_2 GPIO operation
     P2SEL1 &= ~IR_LED; // P2_2 GPIO operation
     P2OUT &= ~IR_LED; // Initial Value = Low / Off
     P2DIR |= IR_LED; // Direction = output


     P2SEL0 &= ~SW2; // SW2 Operation
     P2SEL1 &= ~SW2; // SW2 Operation
     P2OUT |= SW2; // Configure pullup resistor
     P2DIR &= ~SW2; // Direction = input
     P2REN |= SW2; // Enable pullup resistor


     P2SEL0 &= ~IOT_RUN_RED; // IOT_RUN_CPU GPIO operation
     P2SEL1 &= ~IOT_RUN_RED; // IOT_RUN_CPU GPIO operation
     P2OUT &= ~IOT_RUN_RED; // Initial Value = Low / Off
     P2DIR |= IOT_RUN_RED; // Direction = output


     P2SEL0 &= ~DAC_ENB; // DAC_ENB GPIO operation
     P2SEL1 &= ~DAC_ENB; // DAC_ENB GPIO operation
     P2OUT |= DAC_ENB; // Initial Value = High
     P2DIR |= DAC_ENB; // Direction = output


     P2SEL0 &= ~LFXOUT; // LFXOUT Clock operation
     P2SEL1 |= LFXOUT; // LFXOUT Clock operation


     P2SEL0 &= ~LFXIN; // LFXIN Clock operation
     P2SEL1 |= LFXIN; // LFXIN Clock operation
     //------------------------------------------------------------------------------
 }


 void Init_Port3(void){ // Configure PORT 3
     //------------------------------------------------------------------------------
     P3OUT = 0x00; // P3 set Low
     P3DIR = 0x00; // Set P3 direction to output

     P3SEL0 &= ~TEST_PROBE;
     P3SEL1 &= ~TEST_PROBE;
     P3OUT &= ~TEST_PROBE;
     P3DIR &= ~TEST_PROBE;

     P3SEL0 |= OA2O;
     P3SEL1 |= OA2O;
     P3OUT &= ~OA2O;
     P3DIR |= OA2O;

     P3SEL0 |= OA2N;
     P3SEL1 |= OA2N;
     P3OUT &= ~OA2N;
     P3DIR &= ~OA2N;

     P3SEL0 |= OA2P;
     P3SEL1 |= OA2P;
     P3OUT &= ~OA2P;
     P3DIR &= ~OA2P;

     P3SEL0 &= ~SMCLK;
     P3SEL1 &= ~SMCLK;
     P3OUT &= ~SMCLK;
     P3DIR &= ~SMCLK;

     P3SEL0 |= DAC_CNTL;
     P3SEL1 |= DAC_CNTL;
     P3OUT &= ~DAC_CNTL;
     P3DIR &= ~DAC_CNTL;

     P3SEL0 |= IOT_LINK_GRN;
     P3SEL1 |= IOT_LINK_GRN;
     P3OUT &= ~IOT_LINK_GRN;
     P3DIR &= ~IOT_LINK_GRN;

     P3SEL0 |= IOT_EN;
     P3SEL1 |= IOT_EN;
     P3OUT &= ~IOT_EN;
     P3DIR &= ~IOT_EN;
 }




 void Init_Port4(void){ // Configure PORT 4
     //------------------------------------------------------------------------------
     P4OUT = 0x00; // P4 set Low
     P4DIR = 0x00; // Set P4 direction to output


     P4SEL0 &= ~RESET_LCD; // RESET_LCD GPIO operation
     P4SEL1 &= ~RESET_LCD; // RESET_LCD GPIO operation
     P4OUT &= ~RESET_LCD; // Initial Value = Low / Off
     P4DIR |= RESET_LCD; // Direction = output


     P4SEL0 &= ~SW1; // SW1 GPIO operation
     P4SEL1 &= ~SW1; // SW1 GPIO operation
     P4OUT |= SW1; // Configure pullup resistor
     P4DIR &= ~SW1; // Direction = input
     P4REN |= SW1; // Enable pullup resistor


     P4SEL0 |= UCA1TXD; // USCI_A1 UART operation
     P4SEL1 &= ~UCA1TXD; // USCI_A1 UART operation

     P4SEL0 |= UCA1RXD; // USCI_A1 UART operation
     P4SEL1 &= ~UCA1RXD; // USCI_A1 UART operation


     P4SEL0 &= ~UCB1_CS_LCD; // UCB1_CS_LCD GPIO operation
     P4SEL1 &= ~UCB1_CS_LCD; // UCB1_CS_LCD GPIO operation
     P4OUT |= UCB1_CS_LCD; // Set SPI_CS_LCD Off [High]
     P4DIR |= UCB1_CS_LCD; // Set SPI_CS_LCD direction to output


     P4SEL0 |= UCB1CLK; // UCB1CLK SPI BUS operation
     P4SEL1 &= ~UCB1CLK; // UCB1CLK SPI BUS operation

     P4SEL0 |= UCB1SIMO; // UCB1SIMO SPI BUS operation
     P4SEL1 &= ~UCB1SIMO; // UCB1SIMO SPI BUS operation

     P4SEL0 |= UCB1SOMI; // UCB1SOMI SPI BUS operation
     P4SEL1 &= ~UCB1SOMI; // UCB1SOMI SPI BUS operation
     //------------------------------------------------------------------------------
 }


 void Init_Port5(void){ // Configure PORT 5
     //------------------------------------------------------------------------------
     P5OUT = 0x00; // P5 set Low
     P5DIR = 0x00; // Set P5 direction to output

     P5SEL0 |= V_BAT;
     P5SEL1 |= V_BAT;
     P5OUT &= ~V_BAT;
     P5DIR &= ~V_BAT;

     P5SEL0 |= V_5_0;
     P5SEL1 |= V_5_0;
     P5OUT &= ~V_5_0;
     P5DIR &= ~V_5_0;

     P5SEL0 |= V_DAC;
     P5SEL1 |= V_DAC;
     P5OUT &= ~V_DAC;
     P5DIR &= ~V_DAC;

     P5SEL0 |= V_3_3;
     P5SEL1 |= V_3_3;
     P5OUT &= ~V_3_3;
     P5DIR &= ~V_3_3;

     P5SEL0 &= ~IOT_BOOT_CPU;
     P5SEL1 &= ~IOT_BOOT_CPU;
     P5OUT &= ~IOT_BOOT_CPU;
     P5DIR &= ~IOT_BOOT_CPU;
 }


 void Init_Port6(void){ // Configure PORT 6
    //------------------------------------------------------------------------------
    P6OUT = 0x00; // P6 set Low
    P6DIR = 0x00; // Set P6 direction to output

    P6SEL0 &= ~LCD_BACKLITE;
    P6SEL1 &= ~LCD_BACKLITE;
    P6OUT &= ~LCD_BACKLITE;
    P6DIR |= LCD_BACKLITE;

    P6SEL0 &= ~R_FORWARD;
    P6SEL1 &= ~R_FORWARD;
    P6OUT &= ~R_FORWARD;
    P6DIR |= R_FORWARD;

    P6SEL0 &= ~L_FORWARD;
    P6SEL1 &= ~L_FORWARD;
    P6OUT &= ~L_FORWARD;
    P6DIR |= L_FORWARD;

    P6SEL0 &= ~R_REVERSE;
    P6SEL1 &= ~R_REVERSE;
    P6OUT &= ~R_REVERSE;
    P6DIR &= ~R_REVERSE;

    P6SEL0 &= ~L_REVERSE;
    P6SEL1 &= ~L_REVERSE;
    P6OUT &= ~L_REVERSE;
    P6DIR &= ~L_REVERSE;

    P6SEL0 &= ~P6_5;
    P6SEL1 &= ~P6_5;
    P6OUT &= ~P6_5;
    P6DIR &= ~P6_5;

    P6SEL0 &= ~GRN_LED;
    P6SEL1 &= ~GRN_LED;
    P6OUT |= GRN_LED;
    P6DIR |= GRN_LED;
}

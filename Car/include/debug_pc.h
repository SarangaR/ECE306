#ifndef DEBUG_PC_H
#define DEBUG_PC_H

/*===========================================================================
 *  PC Debug Pass-Through  (TEMPORARY — Project 09 setup only)
 *  -----------------------------------------------------------------------
 *  Enables bidirectional pass-through between the PC terminal (UCA1) and
 *  the ESP32 IOT module (UCA0) so you can type AT commands from PuTTY /
 *  Termite to retrieve your module's MAC address and IP address.
 *
 *  Usage
 *  -----
 *  In main.c, call Debug_PC_Init() once during initialisation:
 *
 *      Debug_PC_Init();   // <-- comment this out when no longer needed
 *
 *  Data flow while enabled:
 *    PC terminal (UCA1 RX)  →  ESP32 (UCA0 TX)   [you type AT commands]
 *    ESP32       (UCA0 RX)  →  PC terminal (UCA1 TX) [responses appear]
 *
 *  The FRAM will NOT transmit to the PC until it receives at least one
 *  character from the PC first (per project spec step 13/14).
 *
 *  Removal
 *  -------
 *  Comment out the single Debug_PC_Init() call in main.c.
 *  The UCA1 ISR and hook function do nothing if init was never called.
 *===========================================================================*/
void Debug_PC_Init(void);

#endif /* DEBUG_PC_H */

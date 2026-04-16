#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define BUF_LEN  32    /* must hold a full +IPD frame + null terminator */

void         uart_init(void);
void         uart_send_buf(const char *src);
uint8_t      uart_read_frame(char *dst);
const char  *uart_get_last_frame(void);

/* Byte-level hook called inside the UCA0 RX ISR — used by debug_pc only */
void         uart_set_iot_rx_hook(void (*hook)(char));

#endif /* SERIAL_H */

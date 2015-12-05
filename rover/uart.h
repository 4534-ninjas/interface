#ifndef _ROVER_UART_H_
#define _ROVER_UART_H_

int uart_has_byte(void);
int uart_read_byte(void);
void uart_init(void);
void uart_interrupt_handler(void);

#endif

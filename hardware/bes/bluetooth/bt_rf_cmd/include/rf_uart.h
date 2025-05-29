#ifndef __RF_UART_H
#define __RF_UART_H

#include <stdint.h>

int init_uart(char *dev);
void close_uart(int fd);
void userial_print_raw_data(uint8_t* str, uint16_t length);

#endif /* __RF_UART_H */
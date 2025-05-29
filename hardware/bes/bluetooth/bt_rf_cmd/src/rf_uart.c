#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "rf_uart.h"

static int uart_speed(int s)
{
    switch (s) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
    case 2500000:
        return B2500000;
    case 3000000:
        return B3000000;
    case 3500000:
        return B3500000;
    case 4000000:
        return B4000000;
    default:
        return B57600;
    }
}

int set_speed(int fd, struct termios *ti, int speed)
{
    cfsetospeed(ti, uart_speed(speed));
    cfsetispeed(ti, uart_speed(speed));
    return tcsetattr(fd, TCSANOW, ti);
}

int init_uart(char *dev)
{
    struct termios ti;
    int fd;

    fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Can't open serial port");
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
    if (tcgetattr(fd, &ti) < 0) {
        perror("Can't get port settings");
        return -1;
    }
    cfmakeraw(&ti);
    ti.c_cflag |= CLOCAL;
    ti.c_cflag |= CRTSCTS;
    if (tcsetattr(fd, TCSANOW, &ti) < 0) {
        perror("Can't set port settings");
        return -1;
    }
    /* Set initial baudrate */
    if (set_speed(fd, &ti, 1500000) < 0) {
        perror("Can't set initial baud rate");
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
    return fd;
}

void close_uart(int fd)
{
    close(fd);
}

void userial_print_raw_data(uint8_t* str, uint16_t length)
{
    uint8_t *ch = str;
    int print_line = 0;

    printf("%s, length:%d \n", __func__, length);

    if(length == 0){
        length = 8;
    }

    print_line = length/8;

    for (int k = 0; k < print_line; k ++) {
        printf(" %02x %02x %02x %02x %02x %02x %02x %02x", ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6], ch[7]);
        ch += 8;
    }

    if (length%8) {
        uint8_t raw_data[8] = {0};
        memcpy(raw_data, ch, length%8);
        printf(" %02x %02x %02x %02x %02x %02x %02x %02x \n", raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7]);
    } else {
        printf("\n");
    }
}
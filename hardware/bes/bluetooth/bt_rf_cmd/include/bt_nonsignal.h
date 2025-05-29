#ifndef __BT_NOSIGNAL_H
#define __BT_NOSIGNAL_H

#include <stdint.h>
#define BT_UART "/dev/ttyS1"

struct nonsignal_t {
    int fd;                     /* fd to Bluetooth device */
    pthread_t read_thread;
    int bt_ts_quit;
};

#endif /* __BT_NOSIGNAL_H */
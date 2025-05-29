#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include "rf_uart.h"
#include "rf_cmd.h"

static struct nonsignal_t nonsignal;

struct thread_bt thread_bt;
uint8_t HciRxBuff[64];
int HciRxBuffLen;

static void *event_thread(void *param)
{
    int fd = *((int *)param);
    if (fd < 0)
        goto out;

    fcntl(fd, F_SETFL, O_NDELAY);
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
    while (!nonsignal.bt_ts_quit) {
        gettimeofday(&tv_end, NULL);
        if(tv_end.tv_sec - tv_start.tv_sec >= 2)
            nonsignal.bt_ts_quit = 1;
        HciRxBuffLen = read(fd, HciRxBuff, sizeof(HciRxBuff));
        if (HciRxBuffLen > 0) {
            userial_print_raw_data(HciRxBuff, HciRxBuffLen);
            nonsignal.bt_ts_quit = 1;
            // pthread_mutex_lock(&thread_bt.test_mut);
            pthread_cond_signal(&thread_bt.test_cond);
            // pthread_mutex_unlock(&thread_bt.test_mut);
        }
    }

out:
    pthread_exit(NULL);
}

int main(int argC, char *argV[])
{
    int err;
    nonsignal.fd = init_uart(BT_UART);

    if (nonsignal.fd < 0) {
        printf("open device failed \n");
        goto out;
    }
    nonsignal.bt_ts_quit = 0;
    err = pthread_create(&nonsignal.read_thread, NULL, event_thread, &nonsignal.fd);
    if (err < 0) {
        printf("pthread_create failed \n");
        goto out;
    }

    thread_init(&thread_bt);

    if(argC < 2) {
        printf("Bad segment!!!\n");
        goto out;
    }

    if(!cmd_select(argC, argV, nonsignal))
        printf("Bad segment!!!\n");
    pthread_join(nonsignal.read_thread, NULL);

out:
    if (nonsignal.fd > 0) {
        close(nonsignal.fd);
        nonsignal.fd = -1;
    }
    return 0;
}
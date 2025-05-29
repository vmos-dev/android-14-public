#ifndef __RF_CMD_H
#define __RF_CMD_H

#include <stdint.h>
#include <stdbool.h>
#include "bt_nonsignal.h"

#define bt_mp_start 0x01
#define bt_hci_reset 0x02
#define bt_bdr_tx_signal 0x03
#define bt_set_bdr_power 0x04
#define bt_edr_tx_signal 0x05
#define bt_set_edr_power 0x06
#define bt_le_transmitter_test 0x07
#define bt_le_test_end 0x08
#define bt_bdr_rx_signal 0x09
#define bt_stop_rx_signal 0x10
#define bt_edr_rx_signal 0x11
#define bt_le_receiver_test 0x12
#define bt_enter_signal_mode 0x13
#define bt_scan 0x14

typedef struct {
    const char* string;
    uint8_t idex;
} BT_PACKET_TYPE;

typedef struct {
    const char* string;
    uint8_t idex;
} BT_PAYLOAD_TYPE;

struct bt_test_cmd {
    const char *cmd;
    int (*handler) (int argc, char *argv[], int fd);
};

struct thread_bt {
    pthread_condattr_t test_conda;
    pthread_mutex_t test_mut;
    pthread_cond_t test_cond;
};

enum bt_power_sel {
    BDR_POWER = 0,
    EDR_POWER,
};

#define FACTORY_SIZE 4096
#define FACTORY_SECTION_WIFI_OFFSET 0x800
#define FACTORY_WIFI_DATA_OFFSET (0x8 + FACTORY_SECTION_WIFI_OFFSET)
#define FACTORY_BT_CRC_OFFSET (0x4 + FACTORY_SECTION_WIFI_OFFSET)
#define FACTORY_BT_POWER_OFFSET (0x74 + FACTORY_SECTION_WIFI_OFFSET)
#define FACTORY_CRC_LENGTH (192)
#define FACTORY_PATH "/lib/firmware/bes2600_factory.bin"

bool cmd_select(int argc, char *argv[], struct nonsignal_t nonsignal);
void thread_init(struct thread_bt *thread_bt);
int hci_rx_check_result(uint8_t RX[]);

#endif /* __RF_CMD_H */
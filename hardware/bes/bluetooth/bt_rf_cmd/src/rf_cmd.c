#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include  <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "rf_cmd.h"
#include "rf_uart.h"

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

struct tx_power_cali {
    uint32_t div;
    uint32_t power_level;
};
static int btrf_power_cali_save(struct tx_power_cali *data_cali, enum bt_power_sel power);

uint8_t HciBuff[32];
uint16_t paylaod_size;
static uint8_t dataBuff[24];
extern uint8_t HciRxBuff[64];
extern int HciRxBuffLen;
uint8_t HciTxBuff[32] = {0x01,0x87,0xFC,0x1C,0x00,0xe8,0x03,0x00,0x00,0x00,0x00,0x03,0x55,0x55,0x55,0x55,0x00,0x00,
                        0x01,0x00,0x0F,0x03,0x53,0x01,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF};
struct thread_bt *thread_bt_bes;

void thread_init(struct thread_bt *thread_bt)
{
    pthread_mutex_init (&thread_bt->test_mut, NULL);

    pthread_condattr_init(&thread_bt->test_conda);

    pthread_condattr_setclock(&thread_bt->test_conda, CLOCK_MONOTONIC);

    pthread_cond_init(&thread_bt->test_cond, &thread_bt->test_conda);

    thread_bt_bes = thread_bt;
}

const BT_PACKET_TYPE  bt_packet_type_cfg[] = {
    {"ID_NUL",0x0},
    {"POLL",  0x1},
    {"FHS",   0x2},
    {"DM1",   0x3},
    {"DH1",   0x4},
    {"2-DH1", 0x4},
    {"3-DH1", 0x8},
    {"HV1",   0x5},
    {"HV2",   0x6},
    {"2-EV3", 0x6},
    {"HV3",   0x7},
    {"EV3",   0x7},
    {"3-EV3", 0x7},
    {"DV",    0x8},
    {"AUX1",  0x9},
    {"DM3",   0xa},
    {"DH3",   0xb},
    {"2-DH3", 0xa},
    {"3-DH3", 0xb},
    {"EV4",   0xc},
    {"2-EV5", 0xc},
    {"EV5",   0xd},
    {"3-EV5", 0xd},
    {"DM5",   0xe},
    {"DH5",   0xf},
    {"2-DH5", 0xe},
    {"3-DH5", 0xf},
};

const BT_PAYLOAD_TYPE  bt_payload_type_cfg[] = {
    {"0x00",  0x0},
    {"0xFF",  0x1},
    {"0x55",  0x2},
    {"0x0F",  0x3},
    {"PRBS9", 0x4},
};

const BT_PAYLOAD_TYPE  ble_payload_type_cfg[] = {
    {"PRBS9", 0x0},
    {"0x0F",  0x1},
    {"0x55",  0x2},
    {"PRBS15",0x3},
    {"0xFF",  0x4},
    {"0x00",  0x5},
    {"0xF0",  0x6},
    {"0xAA",  0x7},
};

/**
 * Support hexadecimal, decimal, binary, Octal.
 * Negative numbers are not supported.
 */
static bool bt_atoi(char s[], uint8_t *sum)
{
    int i;
    int n = 0;
    int len = strlen(s);
    int base;

    /* hex offset*/
    if (len > 2 && (s[0] == '0' && s[1] == 'x')) {
        s += 2;
        len -= 2;
        base = 16;
    /* binary offset*/
    } else if (len > 2 && (s[0] == '0' && s[1] == 'b')) {
        s += 2;
        len -= 2;
        base = 2;
    /* Octal offset*/
    } else if (len > 1 && s[0] == '0') {
        s += 1;
        len -= 1;
        base = 8;
    } else {
        base = 10;
    }

    for (i = 0; i < len; ++i) {
        if (base == 2 && (s[i] < '0' || s[i] > '1'))
            return false;
        if (base == 8 && (s[i] < '0' || s[i] > '7'))
            return false;
        if (base == 10 && (s[i] < '0' || s[i] > '9'))
            return false;
        if (base == 16) {
            if (s[i] >= '0' && s[i] <= '9') {
                n = base * n + (s[i] - '0');
            } else if (s[i] >= 'a' && s[i] <= 'f') {
                n = base * n + (s[i] - 'a' + 10);
            } else if (s[i] >= 'A' && s[i] <= 'F') {
                n = base * n + (s[i] - 'A' + 10);
            } else {
                return false;
            }
        } else {
            n = base * n + (s[i] - '0');
        }
    }

    if (n > 255)
        return false;
    sum[0] = n;
    return true;
}

static void lower2upper(char *str)
{
    if (!str)
        return ;
    int i;
    int len = strlen(str);

    /* hex offset*/
    if (len > 2 && (str[0] == '0' && str[1] == 'x')) {
        str += 2;
        len -= 2;
    }

    for (i = 0; i < len; i++) {
        if (str[i] >= 'a' && str[i] <= 'z')
            str[i] -= 32;
    }
}

static uint8_t packet2index(char *Packet)
{
    lower2upper(Packet);
    for (int i = 0; i < (sizeof(bt_packet_type_cfg) / sizeof(BT_PACKET_TYPE)); i++) {
        if (strncmp(Packet, bt_packet_type_cfg[i].string, strlen(Packet)) == 0) {
            return bt_packet_type_cfg[i].idex;
        }
    }

    printf("Packet2Index packet cmd unknown!\n");

    return 0xff;
}

static uint8_t payload2index(char *Payload)
{
    lower2upper(Payload);
    for (int i = 0; i < (sizeof(bt_payload_type_cfg) / sizeof(BT_PACKET_TYPE)); i++) {
        if(strncmp(Payload, bt_payload_type_cfg[i].string, strlen(Payload)) == 0) {
            return bt_payload_type_cfg[i].idex;
        }
    }

    printf("Payload2Index payload cmd unknown!\n");

    return 0xff;
}

uint16_t check_paylaod_size(uint8_t edr,uint8_t type)
{
    uint16_t paylaod_size = 0;
    switch(type)
    {
        case 0x00:    //ID_NUL_TYPE
            paylaod_size = 0;
        break;
        case 0x01:   //POLL_TYPE
            paylaod_size = 0;
        break;
        case 0x02:    //FHS_TYPE
            paylaod_size = 18;
        break;
        case 0x03:
            paylaod_size = 17;
        break;
        case 0x04:
            if(edr)
                paylaod_size = 54;
            else
                paylaod_size = 27;
        break;
        case 0x05:
            paylaod_size = 10;
        break;
        case 0x06:
            if(edr)
                paylaod_size = 60;
            else
                paylaod_size = 20;
        break;
        case 0x07:
            if(edr)
                paylaod_size = 90;
            else
                paylaod_size = 30;
        break;
        case 0x08:
            if(edr)
                paylaod_size = 83;
            else
                paylaod_size = 10;
        break;
        case 0x09:
            paylaod_size = 29;
        break;
        case 0x0A:
            if(edr)
                paylaod_size = 367;
            else
                paylaod_size = 121;
        break;
        case 0x0B:
            if(edr)
                paylaod_size = 552;
            else
                paylaod_size = 183;
        break;
        case 0x0C:
            if(edr)
                paylaod_size = 360;
            else
                paylaod_size = 120;
        break;
        case 0x0D:
            if(edr)
                paylaod_size = 540;
            else
                paylaod_size = 180;
        break;
        case 0x0E:
            if(edr)
                paylaod_size = 679;
            else
                paylaod_size = 224;
        break;
        case 0x0F:
            if(edr)
                paylaod_size = 1021;
            else
                paylaod_size = 339;
        break;
        default :
        break;
    }
    return paylaod_size;
}


static uint8_t payload2index_ble(char *Payload)
{
    lower2upper(Payload);
    for (int i = 0; i < (sizeof(ble_payload_type_cfg) / sizeof(BT_PACKET_TYPE)); i++) {
        if(strncmp(Payload, ble_payload_type_cfg[i].string, strlen(Payload)) == 0) {
            return ble_payload_type_cfg[i].idex;
        }
    }

    printf("Payload2Index payload cmd unknown!\n");

    return 0xff;
}

static uint8_t hexChar2Dec(const char c)
{
  int r = 0;
  if ((c >= '0') && (c <= '9'))
    r = c - '0';
  else if ((c >= 'a') && (c <= 'f'))
    r = c - 'a' + 10;
  else if ((c >= 'A') && (c <= 'F'))
    r = c - 'A' + 10;
  else
    r = 16; /* invalid hex character */

  return (uint8_t)r;
}

static int hexString2CharBuf(const char *string, uint8_t *charBuf, uint32_t charBufLength)
{
  uint32_t i, k = 0;
  uint8_t hNibble, lNibble;
  /* sanity checks */
  if (string[0] == '\0') {
    return -1; /* invalid string size */
  }
  if (strlen(string) != 4) {
    return -1; /* invalid string size */
  }

  if (string[0] != '0' || string[1] != 'x') {
    return -4;  /* not hexadecimal characters */
  }

  string += 2;

  if (charBufLength <= 0){
    return -2; /* invalid buffer size */
  }
  /* convert to hex characters to corresponding 8bit value */
  for (i = 0; (string[i] != '\0') && ((i >> 1) < charBufLength); i += 2) {
    k = i >> 1;
    hNibble = hexChar2Dec(string[i]);
    lNibble = hexChar2Dec(string[i + 1]);
    if ((hNibble == 16) || (lNibble == 16)) {
      return -3; /* invalid character */
    }
    charBuf[k] = ((hNibble << 4) & 0xf0) + lNibble;
  }
  /* check if last character was string terminator */
  if ((string[i - 2] != 0) && (string[i] != 0)) {
    return -1; /* invalid string size */
  }
  /* fill charBuffer with zeros */
  for (i = k + 1; i < charBufLength; i++) {
    charBuf[i] = 0;
  }
  return 0;
}

void send_hci(int fd, uint8_t buff[], int length, int switch_id)
{
    uint8_t buff_send[50] = {0};
    for (int i = length-1; i >= 0; i--) {
        buff_send[i+2] = buff[i];
    }
    buff_send[0] = 0xFF;
    buff_send[1] = switch_id;

    write(fd, buff_send, length+2);
    if((switch_id != bt_set_bdr_power) && (switch_id != bt_set_edr_power))
    {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        tv.tv_sec += 2;
        // pthread_mutex_lock(&thread_bt_bes->test_mut);
        pthread_cond_timedwait(&thread_bt_bes->test_cond, &thread_bt_bes->test_mut, &tv);
        // pthread_mutex_unlock(&thread_bt_bes->test_mut);
    }
}

int hci_rx_check_result(uint8_t RX[])
{
    if((RX[0] == 0) && (RX[1] == 0))
        return 0;
    return 1;
}
static int bt_test_cmd_hci_reset(int argc, char *argv[], int fd)
{
    HciBuff[0] = 0x01;
    HciBuff[1] = 0x03;
    HciBuff[2] = 0x0C;
    HciBuff[3] = 0x00;

    printf("enter %s cmd \n", __func__);

    if (argc != 1) {
        return -EINVAL;
    }

    send_hci(fd, HciBuff, 4, bt_hci_reset);

    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_bdr_tx_signal(int argc, char *argv[], int fd)
{
    uint8_t dataBuff[24];
    uint16_t paylaod_size;

    printf("enter %s cmd \n", __func__);

    if (argc != 4) {
        return -EINVAL;
    }

    if (!bt_atoi(argv[1], dataBuff + 0)) {
        printf("%s format err\n", argv[1]);
        return -EINVAL;
    }
    if ((dataBuff[1] = packet2index(argv[2])) == 0xff) {
        return -EINVAL;
    }
    if ((dataBuff[2] = payload2index(argv[3])) == 0xff) {
        return -EINVAL;
    }

    HciTxBuff[4] = 0x00;
    HciTxBuff[19] = 0x00;
    HciTxBuff[9]  = dataBuff[0];
    HciTxBuff[20] = dataBuff[1];
    HciTxBuff[21] = dataBuff[2];
    paylaod_size = check_paylaod_size(HciTxBuff[19],HciTxBuff[20]);
    HciTxBuff[22] = paylaod_size%0x100;
    HciTxBuff[23] = paylaod_size/0x100;

    send_hci(fd, HciTxBuff, sizeof(HciTxBuff), bt_bdr_tx_signal);

    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_set_bdr_power(int argc, char *argv[], int fd)
{
    int err;
    struct tx_power_cali data_cali;

    printf("enter %s cmd \n", __func__);

    if (argc != 3) {
        return -EINVAL;
    }

    for (int i = 0; i < 2; i++) {
        err = hexString2CharBuf(argv[i + 1], &dataBuff[i], 1);
        if (err) {
            printf("hexString to hexNum fail, err = %d\n", err);
            return -EINVAL;
        }
    }

    send_hci(fd, dataBuff, sizeof(dataBuff), bt_set_bdr_power);

    printf("success !\n");

    data_cali.div = dataBuff[0];
    data_cali.power_level = dataBuff[1];
    if (btrf_power_cali_save(&data_cali, BDR_POWER))
        printf("bdr power save failed\n");
    else
        printf("bdr power save success\n");

    return 0;
}

static int bt_test_cmd_edr_tx_signal(int argc, char *argv[], int fd)
{
    uint16_t paylaod_size;

    printf("enter %s cmd \n", __func__);

    if (argc != 4) {
        return -EINVAL;
    }

    if (!bt_atoi(argv[1], dataBuff + 0)) {
        printf("%s format err\n", argv[1]);
        return -EINVAL;
    }
    if ((dataBuff[1] = packet2index(argv[2])) == 0xff) {
        return -EINVAL;
    }
    if ((dataBuff[2] = payload2index(argv[3])) == 0xff) {
        return -EINVAL;
    }


    HciTxBuff[4] = 0x00;
    HciTxBuff[19] = 0x01;
    HciTxBuff[9]  = dataBuff[0];
    HciTxBuff[20] = dataBuff[1];
    HciTxBuff[21] = dataBuff[2];
    paylaod_size = check_paylaod_size(HciTxBuff[19],HciTxBuff[20]);
    HciTxBuff[22] = paylaod_size%0x100;
    HciTxBuff[23] = paylaod_size/0x100;

    send_hci(fd, HciTxBuff, sizeof(HciTxBuff), bt_edr_tx_signal);
    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_set_edr_power(int argc, char *argv[], int fd)
{
    int err;
    struct tx_power_cali data_cali;

    printf("enter %s cmd \n", __func__);

    if (argc != 3) {
        return -EINVAL;
    }

    for (int i = 0; i < 2; i++) {
        err = hexString2CharBuf(argv[i + 1], &dataBuff[i], 1);
        if (err) {
            printf("hexString to hexNum fail, err = %d\n", err);
            return -EINVAL;
        }
    }

    send_hci(fd, dataBuff, sizeof(dataBuff), bt_set_edr_power);

    printf("success !\n");

    data_cali.div = dataBuff[0];
    data_cali.power_level = dataBuff[1];
    if (btrf_power_cali_save(&data_cali, EDR_POWER))
        printf("edr power save failed\n");
    else
        printf("edr power save success\n");

    return 0;
}

static int bt_test_cmd_transmitter_test(int argc, char *argv[], int fd)
{
    printf("enter %s cmd \n", __func__);

    if (argc != 5) {
        return -EINVAL;
    }

    HciBuff[0] = 0x01;
    HciBuff[1] = 0x34;
    HciBuff[2] = 0x20;
    HciBuff[3] = 0x04;

    if (!bt_atoi(argv[1], dataBuff + 0) ||
        !bt_atoi(argv[2], dataBuff + 1) ||
        !bt_atoi(argv[4], HciBuff + 7)) {
        printf("%s or %s or %s format err\n", argv[1], argv[2], argv[4]);
        return -EINVAL;
    }
    if ((dataBuff[2] = payload2index_ble(argv[3])) == 0xff) {
        return -EINVAL;
    }

    HciBuff[4] = dataBuff[0];
    HciBuff[5] = dataBuff[1];
    HciBuff[6] = dataBuff[2];

    send_hci(fd, HciBuff, 8, bt_le_transmitter_test);
    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_test_end(int argc, char *argv[], int fd)
{
    uint16_t length = 0;

    printf("enter %s cmd \n", __func__);

    if (argc != 1) {
        return -EINVAL;
    }

    HciBuff[0] = 0x01;
    HciBuff[1] = 0x1F;
    HciBuff[2] = 0x20;
    HciBuff[3] = 0x00;

    send_hci(fd, HciBuff, 4, bt_le_test_end);
    length = HciRxBuff[8]*0x100 + HciRxBuff[7];
    printf("success:%d receive packet num length:%d \n",hci_rx_check_result(HciRxBuff), length);

    return 0;
}

static int bt_test_cmd_bdr_rx_signal(int argc, char *argv[], int fd)
{
    printf("enter %s cmd \n", __func__);

    if (argc != 4) {
        return -EINVAL;
    }

    if (!bt_atoi(argv[1], dataBuff + 0)) {
        printf("%s format err\n", argv[1]);
        return -EINVAL;
    }
    if ((dataBuff[1] = packet2index(argv[2])) == 0xff) {
        return -EINVAL;
    }
    if ((dataBuff[2] = payload2index(argv[3])) == 0xff) {
        return -EINVAL;
    }

    HciTxBuff[4] = 0x01;
    HciTxBuff[19] = 0x00;
    HciTxBuff[10] = dataBuff[0];
    HciTxBuff[20] = dataBuff[1];
    HciTxBuff[21] = dataBuff[2];
    paylaod_size = check_paylaod_size(HciTxBuff[19],HciTxBuff[20]);
    HciTxBuff[22] = paylaod_size%0x100;
    HciTxBuff[23] = paylaod_size/0x100;

    send_hci(fd, HciTxBuff, sizeof(HciTxBuff), bt_bdr_rx_signal);
    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_stop_rx_signal(int argc, char *argv[], int fd)
{
    uint16_t length = 0, ret = 0;
    uint16_t head_error,payload_error,adv_estsw,adv_esttpl;
    uint32_t payload_bit_error;

    printf("enter %s cmd \n", __func__);

    if (argc != 1) {
        return -EINVAL;
    }

    memset(dataBuff,0,sizeof(dataBuff));
    HciTxBuff[4] = 0x02;
    send_hci(fd, HciTxBuff, sizeof(HciTxBuff), bt_stop_rx_signal);

    length = HciRxBuff[8]*0x100+HciRxBuff[7];
    head_error = HciRxBuff[10]*0x100+HciRxBuff[9];
    payload_error = HciRxBuff[12]*0x100+HciRxBuff[11];
    adv_estsw = HciRxBuff[14]*0x100+HciRxBuff[13];
    adv_esttpl = HciRxBuff[16]*0x100+HciRxBuff[15];
    payload_bit_error = HciRxBuff[20]*0x1000000+HciRxBuff[19]*0x10000+HciRxBuff[18]*0x100+HciRxBuff[17];

    printf("success:%d receive packet num length:%d \n", ret, length);
    printf("head_error:%d \n", head_error);
    printf("payload_error:%d \n", payload_error);
    printf("adv_estsw:%d \n", adv_estsw);
    printf("adv_esttpl:%d \n", adv_esttpl);
    printf("payload_bit_error:%d \n", payload_bit_error);

    return length;
}

static int bt_test_cmd_edr_rx_signal(int argc, char *argv[], int fd)
{
    printf("enter %s cmd \n", __func__);

    if (argc != 4) {
        return -EINVAL;
    }

    if (!bt_atoi(argv[1], dataBuff + 0)) {
        printf("%s format err\n", argv[1]);
        return -EINVAL;
    }
    if ((dataBuff[1] = packet2index(argv[2])) == 0xff) {
        return -EINVAL;
    }
    if ((dataBuff[2] = payload2index(argv[3])) == 0xff) {
        return -EINVAL;
    }

    HciTxBuff[4] = 0x01;
    HciTxBuff[19] = 0x01;
    HciTxBuff[10] = dataBuff[0];
    HciTxBuff[20] = dataBuff[1];
    HciTxBuff[21] = dataBuff[2];
    paylaod_size = check_paylaod_size(HciTxBuff[19],HciTxBuff[20]);
    HciTxBuff[22] = paylaod_size%0x100;
    HciTxBuff[23] = paylaod_size/0x100;

    send_hci(fd, HciTxBuff, sizeof(HciTxBuff), bt_edr_rx_signal);
    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_receiver_test(int argc, char *argv[], int fd)
{
    printf("enter %s cmd \n", __func__);

    if (argc != 3) {
        return -EINVAL;
    }

    HciBuff[0] = 0x01;
    HciBuff[1] = 0x33;
    HciBuff[2] = 0x20;
    HciBuff[3] = 0x03;
    if (!bt_atoi(argv[1], HciBuff + 4) ||
        !bt_atoi(argv[2], HciBuff + 5)) {
        printf("%s or %s format err\n", argv[1], argv[2]);
        return -EINVAL;
    }
    HciBuff[6] = 0x00;

    send_hci(fd, HciBuff, 7, bt_le_receiver_test);
    printf("success:%d length:%d \n",hci_rx_check_result(HciRxBuff), HciRxBuffLen);
    return 0;
}

static int bt_test_cmd_enter_signal_mode(int argc, char *argv[], int fd)
{
    printf("enter %s cmd \n", __func__);

    if (argc != 1) {
        return -EINVAL;
    }

    dataBuff[0] = 0x01;
    dataBuff[1] = 0x03;
    dataBuff[2] = 0x18;
    dataBuff[3] = 0x00;

    send_hci(fd, dataBuff, 4, bt_enter_signal_mode);

    return 0;
}

static int bt_test_cmd_scan(int argc, char *argv[], int fd)
{
    printf("enter %s cmd \n", __func__);

    if (argc != 2) {
        return -EINVAL;
    }

    HciBuff[0] = 0x01;
    HciBuff[1] = 0x1a;
    HciBuff[2] = 0x0c;
    HciBuff[3] = 0x01;
    if (strncmp(argv[1], "on", strlen(argv[1])) == 0) {
        HciBuff[4] = 0x03;
    } else if (strncmp(argv[1], "off", strlen(argv[1])) == 0) {
        HciBuff[4] = 0x00;
    }
    send_hci(fd, HciBuff, 5, bt_scan);

    return 0;
}

static void make_crc_table(uint32_t crc_table[], int len)
{
    uint32_t c;
    uint32_t n, k;

    for (n = 0; n < len; n++) {
        c = n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xEDB88320 ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
}

static uint32_t btrf_crc32(const uint8_t *buf, uint32_t len)
{
    uint32_t crc_table[256];
    uint32_t crc = 0xffffffff;

    make_crc_table(crc_table, ARRAY_SIZE(crc_table));

    while (len) {
        crc = crc_table[(crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
        len--;
    }

    return crc ^ 0xffffffff;
}

static int btrf_power_cali_save(struct tx_power_cali *data_cali, enum bt_power_sel power)
{
    int fp, ret;
    uint8_t *mempool = NULL;
    uint32_t crc, bt_power_offset;
    bool save_status = true;
    printf("enter %s \n", __func__);

    mempool = (uint8_t*)malloc(FACTORY_SIZE);
    if(mempool == NULL) {
        printf("%s : mempool zalloc error!\n", __func__);
        return -1;
    }

    if (power > EDR_POWER)
        save_status = false;

    if (data_cali->div > 0x05 || data_cali->div < 0x02)
        save_status = false;

    if (data_cali->power_level > 0x20 || data_cali->power_level == 0x0)
        save_status = false;

    if (!save_status) {
        printf("cali value inval, save failed\n");
        free(mempool);
        return -EPERM;
    }

    fp = open(FACTORY_PATH, O_RDWR, 0);
    if(fp < 0) {
        printf("power cali file open failed\n");
        free(mempool);
        return fp;
    }

    ret = read(fp, mempool, FACTORY_SIZE);
    if (ret < 0) {
        printf("power cali file read failed\n");
        free(mempool);
        return -EPERM;
    }

    bt_power_offset = power * sizeof(uint32_t) * 2;

    memcpy(mempool + FACTORY_BT_POWER_OFFSET + bt_power_offset, data_cali, sizeof(struct tx_power_cali));

    crc = btrf_crc32(mempool + FACTORY_WIFI_DATA_OFFSET, FACTORY_CRC_LENGTH);

    lseek(fp, FACTORY_BT_CRC_OFFSET, SEEK_SET);
    ret = write(fp, &crc, sizeof(crc));
    if(ret < 0)
        save_status = false;

    lseek(fp, FACTORY_BT_POWER_OFFSET + bt_power_offset, SEEK_SET);
    ret = write(fp, data_cali, sizeof(struct tx_power_cali));
    if(ret < 0)
        save_status = false;
    
    close(fp);

    if (!save_status) {
        printf("power cali write failed\n");
        free(mempool);
        return -ret;
    }

    printf("power cali write complete\n");
    free(mempool);
    return 0;
}

static struct bt_test_cmd bt_test_commands[] = {
    {"bt_hci_reset", bt_test_cmd_hci_reset},
    {"bt_bdr_tx_signal", bt_test_cmd_bdr_tx_signal},
    {"bt_set_bdr_power", bt_test_cmd_set_bdr_power},
    {"bt_edr_tx_signal", bt_test_cmd_edr_tx_signal},
    {"bt_set_edr_power", bt_test_cmd_set_edr_power},
    {"bt_le_transmitter_test", bt_test_cmd_transmitter_test},
    {"bt_le_test_end", bt_test_cmd_test_end},
    {"bt_bdr_rx_signal", bt_test_cmd_bdr_rx_signal},
    {"bt_stop_rx_signal", bt_test_cmd_stop_rx_signal},
    {"bt_edr_rx_signal", bt_test_cmd_edr_rx_signal},
    {"bt_le_receiver_test", bt_test_cmd_receiver_test},
    {"bt_enter_signal_mode", bt_test_cmd_enter_signal_mode},
    {"bt_scan", bt_test_cmd_scan},
};

bool cmd_select(int argc, char *argv[], struct nonsignal_t nonsignal)
{
    int err;
    memset(HciRxBuff, 0, sizeof(HciRxBuff));
    for (int i = 0;
            i <
            sizeof(bt_test_commands) /
            sizeof(struct bt_test_cmd); i++) {
        if (!memcmp
            (argv[1], bt_test_commands[i].cmd,
                strlen(bt_test_commands[i].cmd))) {
            err = bt_test_commands[i].handler(argc - 1, &argv[1], nonsignal.fd);
            if (err)
                return false;
            return true;
        }
    }
    return false;
}
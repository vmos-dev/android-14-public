/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Filename:      userial_vendor.c
 *
 *  Description:   Contains vendor-specific userial functions
 *
 ******************************************************************************/

#define LOG_TAG "bt_userial_vendor"

#include <utils/Log.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bes_hcidefs.h"
#include "bt_vendor_bes.h"
#include "userial.h"
#include "userial_vendor.h"
#include <unistd.h>
#include "bes_h5.h"
#ifdef CONFIG_SCO_OVER_HCI
#include "sbc.h"
#include "bes_mmap.h"
#include "bes_cqueue.h"
#endif

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef VNDUSERIAL_DBG
#define VNDUSERIAL_DBG FALSE
#endif

#if (VNDUSERIAL_DBG == TRUE)
#define VNDUSERIALDBG(param, ...) {ALOGD("BES_ "param, ## __VA_ARGS__);}
#else
#define VNDUSERIALDBG(param, ...) {}
#endif

#define VND_PORT_NAME_MAXLEN    256

#define SCO_PACKET_HEAD_LEN (4) //1-type 2-handle 1-len
#define SCO_PACKET_LEN (60)
#define SCO_PACKET_PCM_LEN (240)
/******************************************************************************
**  Local type definitions
******************************************************************************/
/*******************
 * SCO Codec Types
 *******************/
typedef enum {
  SCO_CODEC_NONE = 0x0000,
  SCO_CODEC_CVSD = 0x0001,
  SCO_CODEC_MSBC = 0x0002,
} sco_codec_t;

/* vendor serial control block */
typedef struct
{
    int fd;                     /* fd to Bluetooth device */
    int uart_fd[2];        /* fd for socket host <--> controller*/
    int epoll_host_fd;  /* bind with host socket*/
    int signal_fd[2];     /* event with different thread*/
    struct termios termios;     /* serial terminal of BT port */
    char port_name[VND_PORT_NAME_MAXLEN];
    pthread_t thread_socket_id;
    pthread_t thread_uart_id;
    bool thread_running;
    bool h5_enable;
    volatile bool  btdriver_state;
} vnd_userial_cb_t;

#ifdef CONFIG_SCO_OVER_HCI
#define SCO_DOWNLINK_QUEUE_SIZE (4096)
uint16_t btui_msbc_h2[] = {0x0801,0x3801,0xc801,0xf801};
typedef struct
{
    sco_codec_t codec_type;
    pthread_mutex_t sco_mutex;
    pthread_cond_t  sco_cond;
    uint16_t        sco_thread_event;
    bool thread_sco_running;
    pthread_t thread_send_sco_id;
    uint16_t  sco_handle;
    uint16_t voice_settings;
    unsigned char dec_data[240];
    unsigned int current_pos;
    uint16_t sco_packet_len;
    sbc_t sbc_dec, sbc_enc;
    uint32_t pcm_enc_seq;
}sco_cb_t;
int bes_mmap_sco_read_cb(unsigned char * str, unsigned short length);
#endif

/******************************************************************************
**  Static variables
******************************************************************************/
#ifdef CONFIG_SCO_OVER_HCI
static sco_cb_t sco_cb;
static CQueue sco_downlink_queue;
static unsigned char sco_downlink_data_buffer[SCO_DOWNLINK_QUEUE_SIZE];
bes_mmap_context_st bes_mmap_read_context = {0};
bes_mmap_context_st bes_mmap_write_context = {0};
#endif
static vnd_userial_cb_t vnd_userial;
static struct bt_object_t bt_socket_object = {0};
static int packet_recv_state = BESBT_PACKET_IDLE;
static unsigned int packet_bytes_need = 0;
static serial_data_type_t current_type = 0;
static unsigned char h4_read_buffer[2048] = {0};
static int h4_read_length = 0;

#ifdef BES_HANDLE_EVENT
static int received_packet_state = BESBT_PACKET_IDLE;
static int received_packet_bytes_need = 0;
static serial_data_type_t recv_packet_current_type = 0;
static unsigned char received_resvered_header[2048] = {0};
static int received_resvered_length = 0;
#endif

static const uint8_t hci_preamble_sizes[] = {
    COMMAND_PREAMBLE_SIZE,
    ACL_PREAMBLE_SIZE,
    SCO_PREAMBLE_SIZE,
    EVENT_PREAMBLE_SIZE
};

/*****************************************************************************
**   Helper Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        userial_to_tcio_baud
**
** Description     helper function converts USERIAL baud rates into TCIO
**                  conforming baud rates
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t userial_to_tcio_baud(uint8_t cfg_baud, uint32_t *baud)
{
    if (cfg_baud == USERIAL_BAUD_115200)
        *baud = B115200;
    else if (cfg_baud == USERIAL_BAUD_4M)
        *baud = B4000000;
    else if (cfg_baud == USERIAL_BAUD_3M)
        *baud = B3000000;
    else if (cfg_baud == USERIAL_BAUD_2M)
        *baud = B2000000;
    else if (cfg_baud == USERIAL_BAUD_1_5M)
        *baud = B1500000;
    else if (cfg_baud == USERIAL_BAUD_1M)
        *baud = B1000000;
    else if (cfg_baud == USERIAL_BAUD_921600)
        *baud = B921600;
    else if (cfg_baud == USERIAL_BAUD_460800)
        *baud = B460800;
    else if (cfg_baud == USERIAL_BAUD_230400)
        *baud = B230400;
    else if (cfg_baud == USERIAL_BAUD_57600)
        *baud = B57600;
    else if (cfg_baud == USERIAL_BAUD_19200)
        *baud = B19200;
    else if (cfg_baud == USERIAL_BAUD_9600)
        *baud = B9600;
    else if (cfg_baud == USERIAL_BAUD_1200)
        *baud = B1200;
    else if (cfg_baud == USERIAL_BAUD_600)
        *baud = B600;
    else
    {
        ALOGE( "userial_to_tcio_baud: unsupported baud idx %i", cfg_baud);
        *baud = B115200;
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function        cfg_to_uart_baud
**
** Description     helper function converts  cfg
**                      conforming baud rates to USERIAL baud rates
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t cfg_to_uart_baud(uint8_t * cfg_baud, uint32_t baud)
{
    if (baud == 115200)
        *cfg_baud = USERIAL_BAUD_115200;
    else if (baud == 4000000)
        *cfg_baud = USERIAL_BAUD_4M;
    else if (baud == 3000000)
        *cfg_baud = USERIAL_BAUD_3M;
    else if (baud == 2000000)
        *cfg_baud = USERIAL_BAUD_2M;
    else if (baud == 1500000)
        *cfg_baud = USERIAL_BAUD_1_5M;
    else if (baud == 1000000)
        *cfg_baud = USERIAL_BAUD_1M;
    else if (baud == 921600)
        *cfg_baud = USERIAL_BAUD_921600;
    else if (baud == 460800)
        *cfg_baud = USERIAL_BAUD_460800;
    else if (baud == 230400)
        *cfg_baud = USERIAL_BAUD_230400;
    else if (baud == 57600)
        *cfg_baud = USERIAL_BAUD_57600;
    else if (baud == 19200)
        *cfg_baud = USERIAL_BAUD_19200;
    else if (baud == 9600)
        *cfg_baud = USERIAL_BAUD_9600;
    else if (baud == 1200)
        *cfg_baud = USERIAL_BAUD_1200;
    else if (baud == 600)
        *cfg_baud = USERIAL_BAUD_600;
    else
    {
        ALOGE( "tcio_to_uart_baud: unsupported baud: %d", baud);
        *cfg_baud = USERIAL_BAUD_921600;
        return FALSE;
    }

    return TRUE;
}

void userial_print_raw_data(uint8_t* str, uint16_t length)
{
    uint8_t *ch = str;
    int print_line = 0;
    if(length == 0){
        length = 8;
    }

    print_line = length/8;

    for(int k = 0; k < print_line; k ++){
        ALOGD(" %02x %02x %02x %02x %02x %02x %02x %02x", ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6], ch[7]);
        ch += 8;
    }

    if(length%8){
        uint8_t raw_data[8] = {0};
        memcpy(raw_data, ch, length%8);
        ALOGD(" %02x %02x %02x %02x %02x %02x %02x %02x", raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7]);
    }
}


#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
/*******************************************************************************
**
** Function        userial_ioctl_init_bt_wake
**
** Description     helper function to set the open state of the bt_wake if ioctl
**                  is used. it should not hurt in the rfkill case but it might
**                  be better to compile it out.
**
** Returns         none
**
*******************************************************************************/
void userial_ioctl_init_bt_wake(int fd)
{
    uint32_t bt_wake_state;

#if (BT_WAKE_USERIAL_LDISC==TRUE)
    int ldisc = N_BRCM_HCI; /* brcm sleep mode support line discipline */

    /* attempt to load enable discipline driver */
    if (ioctl(vnd_userial.fd, TIOCSETD, &ldisc) < 0)
    {
        VNDUSERIALDBG("USERIAL_Open():fd %d, TIOCSETD failed: error %d for ldisc: %d",
                      fd, errno, ldisc);
    }
#endif



    /* assert BT_WAKE through ioctl */
    ioctl(fd, USERIAL_IOCTL_BT_WAKE_ASSERT, NULL);
    ioctl(fd, USERIAL_IOCTL_BT_WAKE_GET_ST, &bt_wake_state);
    VNDUSERIALDBG("userial_ioctl_init_bt_wake read back BT_WAKE state=%i", \
               bt_wake_state);
}
#endif // (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)


/*****************************************************************************
**   Userial Vendor API Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        userial_vendor_init
**
** Description     Initialize userial vendor-specific control block
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_init(void)
{
#ifdef BES_HANDLE_EVENT
    //reset handle event gloable variables
    received_packet_state = BESBT_PACKET_IDLE;
    received_packet_bytes_need = 0;
    recv_packet_current_type = 0;
    received_resvered_length = 0;
#endif

    vnd_userial.fd = -1;
    snprintf(vnd_userial.port_name, VND_PORT_NAME_MAXLEN, "%s", \
            BLUETOOTH_UART_DEVICE_PORT);
#ifdef CONFIG_SCO_OVER_HCI
    sco_cb.thread_sco_running = false;
    pthread_mutex_init(&sco_cb.sco_mutex, NULL);
    pthread_cond_init(&sco_cb.sco_cond, NULL);
    sco_cb.sco_thread_event = 0;
    memset(&sco_cb.sbc_enc, 0, sizeof(sbc_t));
    sbc_init_msbc(&sco_cb.sbc_enc, 0L);
    sco_cb.sbc_enc.endian = SBC_LE;
    memset(&sco_cb.sbc_dec, 0, sizeof(sbc_t));
    sbc_init_msbc(&sco_cb.sbc_dec, 0L);
    sco_cb.sbc_dec.endian = SBC_LE;
    sco_cb.pcm_enc_seq = 0;

    InitCQueue(&sco_downlink_queue, SCO_DOWNLINK_QUEUE_SIZE, sco_downlink_data_buffer);    
    memcpy(bes_mmap_read_context.mmap_path, "/dev/ashmem", 12);
    memcpy(bes_mmap_read_context.mmap_name, "bes_mmap_downlink", 18);
    memcpy(bes_mmap_write_context.mmap_path, "/dev/ashmem", 12);
    memcpy(bes_mmap_write_context.mmap_name, "bes_mmap_uplink", 16);
    bes_mmap_read_register_cb(&bes_mmap_read_context, bes_mmap_sco_read_cb);
    bes_mmap_read_init(&bes_mmap_read_context);
#endif    
}

void userial_vendor_set_h5(bool h5_enable)
{
    vnd_userial.h5_enable = h5_enable;
}
/*******************************************************************************
**
** Function        userial_vendor_open
**
** Description     Open the serial port with the given configuration
**
** Returns         device fd
**
*******************************************************************************/
int userial_vendor_open(tUSERIAL_CFG *p_cfg)
{
    uint32_t baud;
    uint8_t data_bits;
    uint16_t parity;
    uint8_t stop_bits;

    vnd_userial.fd = -1;

    if (!userial_to_tcio_baud(p_cfg->baud, &baud))
    {
        return -1;
    }

    if(p_cfg->fmt & USERIAL_DATABITS_8)
        data_bits = CS8;
    else if(p_cfg->fmt & USERIAL_DATABITS_7)
        data_bits = CS7;
    else if(p_cfg->fmt & USERIAL_DATABITS_6)
        data_bits = CS6;
    else if(p_cfg->fmt & USERIAL_DATABITS_5)
        data_bits = CS5;
    else
    {
        ALOGE("userial vendor open: unsupported data bits");
        return -1;
    }

    if(p_cfg->fmt & USERIAL_PARITY_NONE)
        parity = 0;
    else if(p_cfg->fmt & USERIAL_PARITY_EVEN)
        parity = PARENB;
    else if(p_cfg->fmt & USERIAL_PARITY_ODD)
        parity = (PARENB | PARODD);
    else
    {
        ALOGE("userial vendor open: unsupported parity bit mode");
        return -1;
    }

    if(p_cfg->fmt & USERIAL_STOPBITS_1)
        stop_bits = 0;
    else if(p_cfg->fmt & USERIAL_STOPBITS_2)
        stop_bits = CSTOPB;
    else
    {
        ALOGE("userial vendor open: unsupported stop bits");
        return -1;
    }

    ALOGI("userial vendor open: opening %s", vnd_userial.port_name);

    if ((vnd_userial.fd = open(vnd_userial.port_name, O_RDWR)) == -1)
    {
        ALOGE("userial vendor open: unable to open %s", vnd_userial.port_name);
        return -1;
    }

    tcflush(vnd_userial.fd, TCIOFLUSH);

    tcgetattr(vnd_userial.fd, &vnd_userial.termios);
    cfmakeraw(&vnd_userial.termios);

    if(p_cfg->hw_fctrl == USERIAL_HW_FLOW_CTRL_ON){
        vnd_userial.termios.c_cflag |= (CRTSCTS | stop_bits);
    }else
        vnd_userial.termios.c_cflag |= stop_bits;
    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);
    tcflush(vnd_userial.fd, TCIOFLUSH);

    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);
    tcflush(vnd_userial.fd, TCIOFLUSH);
    tcflush(vnd_userial.fd, TCIOFLUSH);

    /* set input/output baudrate */
    cfsetospeed(&vnd_userial.termios, baud);
    cfsetispeed(&vnd_userial.termios, baud);
    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    userial_ioctl_init_bt_wake(vnd_userial.fd);
#endif
    vnd_userial.btdriver_state = true;
    ALOGI("device fd = %d open baud = %d", vnd_userial.fd, baud);
    if(vnd_userial.h5_enable){
        h5_init();
    }
    return vnd_userial.fd;
}

static void userial_uart_close(void)
{
    int result;
    if ((result = close(vnd_userial.fd)) < 0)
        ALOGE( "%s (fd:%d) FAILED result:%d", __func__, vnd_userial.fd, result);
    
     vnd_userial.fd = -1;
    pthread_join(vnd_userial.thread_uart_id, NULL);
}

static void userial_socket_close(void)
{
    int result;

    if ((result = close(vnd_userial.uart_fd[0])) < 0)
        ALOGE( "%s (fd:%d) FAILED result:%d", __func__, vnd_userial.uart_fd[0], result);

    if (epoll_ctl(vnd_userial.epoll_host_fd, EPOLL_CTL_DEL, vnd_userial.uart_fd[1], NULL) == -1)
      ALOGE("%s unable to unregister fd %d from epoll set: %s", __func__, vnd_userial.uart_fd[1], strerror(errno));

    if (epoll_ctl(vnd_userial.epoll_host_fd, EPOLL_CTL_DEL, vnd_userial.signal_fd[1], NULL) == -1)
      ALOGE("%s unable to unregister signal fd %d from epoll set: %s", __func__, vnd_userial.signal_fd[1], strerror(errno));

    if ((result = close(vnd_userial.uart_fd[1])) < 0)
        ALOGE( "%s (fd:%d) FAILED result:%d", __func__, vnd_userial.uart_fd[1], result);

    pthread_join(vnd_userial.thread_socket_id, NULL);
    close(vnd_userial.epoll_host_fd);

    if ((result = close(vnd_userial.signal_fd[0])) < 0)
        ALOGE( "%s (signal fd[0]:%d) FAILED result:%d", __func__, vnd_userial.signal_fd[0], result);
    if ((result = close(vnd_userial.signal_fd[1])) < 0)
        ALOGE( "%s (signal fd[1]:%d) FAILED result:%d", __func__, vnd_userial.signal_fd[1], result);

    vnd_userial.epoll_host_fd = -1;
    vnd_userial.uart_fd[0] = -1;
    vnd_userial.uart_fd[1] = -1;
    vnd_userial.signal_fd[0] = -1;
    vnd_userial.signal_fd[1] = -1;
}

void userial_send_close_signal(void)
{
    unsigned char close_signal = 1;
    ssize_t ret;
    BES_NO_INTR(ret = write(vnd_userial.signal_fd[0], &close_signal, 1));
}

/*******************************************************************************
**
** Function        userial_vendor_close
**
** Description     Conduct vendor-specific close work
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_close(void)
{
    int result;

    if (vnd_userial.fd == -1)
        return;

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    /* de-assert bt_wake BEFORE closing port */
    ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_DEASSERT, NULL);
#endif
    vnd_userial.thread_running = false;
    ALOGI("device fd = %d close", vnd_userial.fd);
    // flush Tx before close to make sure no chars in buffer
    tcflush(vnd_userial.fd, TCIOFLUSH);
    userial_send_close_signal();
    userial_uart_close();
    
    userial_socket_close();

    if(vnd_userial.h5_enable){
        h5_close();
    }
    vnd_userial.btdriver_state = false;
#ifdef CONFIG_SCO_OVER_HCI
    sbc_finish(&sco_cb.sbc_enc);
    sbc_finish(&sco_cb.sbc_dec);
    bes_mmap_read_close(&bes_mmap_read_context);
#endif    
}

/*******************************************************************************
**
** Function        userial_vendor_set_baud
**
** Description     Set new baud rate
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_set_baud(uint8_t userial_baud)
{
    uint32_t tcio_baud;

    if(USERIAL_VENDOR_SET_BAUD_DELAY_US > 0) {
	    usleep(USERIAL_VENDOR_SET_BAUD_DELAY_US);
    }

    userial_to_tcio_baud(userial_baud, &tcio_baud);

    cfsetospeed(&vnd_userial.termios, tcio_baud);
    cfsetispeed(&vnd_userial.termios, tcio_baud);
    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);
}

/*******************************************************************************
**
** Function        userial_vendor_ioctl
**
** Description     ioctl inteface
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_ioctl(userial_vendor_ioctl_op_t op, void *p_data)
{
    switch(op)
    {
#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
        case USERIAL_OP_ASSERT_BT_WAKE:
            VNDUSERIALDBG("## userial_vendor_ioctl: Asserting BT_Wake ##");
            ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_ASSERT, NULL);
            break;

        case USERIAL_OP_DEASSERT_BT_WAKE:
            VNDUSERIALDBG("## userial_vendor_ioctl: De-asserting BT_Wake ##");
            ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_DEASSERT, NULL);
            break;

        case USERIAL_OP_GET_BT_WAKE_STATE:
            ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_GET_ST, p_data);
            break;
#endif  //  (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)

        default:
            break;
    }
}

/*******************************************************************************
**
** Function        userial_set_port
**
** Description     Configure UART port name
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int userial_set_port(char *p_conf_value)
{
    strcpy(vnd_userial.port_name, p_conf_value);

    return 0;
}

//FROM HOST HCI process. The data type only have ACL/SCO/COMMAND
// direction  BT HOST ----> CONTROLLER
#ifdef BES_HANDLE_CMD
static void userial_handle_cmd(unsigned char * recv_buffer, int total_length)
{
    BES_UNUSED(total_length);
    uint16_t opcode = STR_TO_U16(recv_buffer);
    uint16_t scan_int, scan_win;
    static uint16_t voice_settings;

    ALOGD("cmd: 0x%04x %d" ,opcode, total_length);
    userial_print_raw_data(recv_buffer, total_length);
    switch (opcode) {
        case HCI_BLE_WRITE_SCAN_PARAMS :
            scan_int = STR_TO_U16(&recv_buffer[4]);
            scan_win = STR_TO_U16(&recv_buffer[6]);
            if(scan_win > 20){
                if((scan_int/scan_win) > 2) {
                  *(uint16_t*)&recv_buffer[4] = (scan_int * 20) / scan_win;
                  *(uint16_t*)&recv_buffer[6] = 20;
                }
                else {
                  *(uint16_t*)&recv_buffer[4] = 40;
                  *(uint16_t*)&recv_buffer[6] = 20;
                }
            }
            else if(scan_win == scan_int) {
              *(uint16_t*)&recv_buffer[4] = (scan_int * 5) & 0xFE;
            }
            else if((scan_int/scan_win) <= 2) {
              *(uint16_t*)&recv_buffer[4] = (scan_int * 3) & 0xFE;
            }
        break;

        case HCI_LE_SET_EXTENDED_SCAN_PARAMETERS:
            scan_int =STR_TO_U16(&recv_buffer[7]);
            scan_win = STR_TO_U16(&recv_buffer[9]);
            if(scan_win > 20){
                if((scan_int/scan_win) > 2) {
                    *(uint16_t*)&recv_buffer[7] = (scan_int * 20) / scan_win;
                    *(uint16_t*)&recv_buffer[9] = 20;
                }
                else {
                    *(uint16_t*)&recv_buffer[7] = 40;
                    *(uint16_t*)&recv_buffer[9] = 20;
                }
            }
            else if(scan_win == scan_int) {
              *(uint16_t*)&recv_buffer[7] = (scan_int * 5) & 0xFE;
            }
            else if((scan_int/scan_win) <= 2) {
              *(uint16_t*)&recv_buffer[9] = (scan_int * 3) & 0xFE;
            }

        break;

        case HCI_WRITE_VOICE_SETTINGS :
            voice_settings = STR_TO_U16(&recv_buffer[3]);
#ifdef CONFIG_SCO_OVER_HCI
            sco_cb.voice_settings = voice_settings;
#endif            
        break;
#ifdef CONFIG_SCO_OVER_HCI
        case HCI_SETUP_ESCO_CONNECTION :
            sco_cb.voice_settings = *(uint16_t*)&recv_buffer[15];
        break;
#endif        
        case HCI_SET_EVENT_MASK:
          ALOGE("set event mask, it should bt stack init, set coex bt on");
        break;

        case HCI_ACCEPT_CONNECTION_REQUEST:

        break;

        case HCI_BLE_WRITE_ADV_PARAMS:
        {

        }
        break;
        case HCI_BLE_WRITE_ADV_ENABLE:
        {
            
        }
        break;
        case HCI_BLE_CREATE_LL_CONN:
        
        break;
        default:
        break;
    }
}
#endif

uint16_t userial_vendor_transmit_data_to_btc(uint8_t *data, uint16_t total_length) {
    assert(data != NULL);
    assert(total_length > 0);

    uint16_t length = total_length;
    uint16_t transmitted_length = 0;

    ALOGD("TX: %d", total_length);
    userial_print_raw_data(data, total_length);
    while (length > 0 && vnd_userial.btdriver_state) {
        ssize_t ret = write(vnd_userial.fd, data + transmitted_length, length);
        switch (ret) {
            case -1:
            ALOGE("In %s, error writing to the uart serial port: %s", __func__, strerror(errno));
            goto done;
        case 0:
            // If we wrote nothing, don't loop more because we
            // can't go to infinity or beyond, ohterwise H5 can resend data
            ALOGE("%s, ret %zd", __func__, ret);
            goto done;
        default:
            transmitted_length += ret;
            length -= ret;
            break;
        }
    }

done:;
    return transmitted_length;
}

static void userial_recv_H4_rawdata(void *context)
{
    BES_UNUSED(context);
    serial_data_type_t type = 0;
    ssize_t bytes_read;
    uint16_t opcode;
    uint16_t transmitted_length = 0;
    //unsigned char *buffer = NULL;

    switch (packet_recv_state) {
        case BESBT_PACKET_IDLE:
            packet_bytes_need = 1;
            do {
                BES_NO_INTR(bytes_read = read(vnd_userial.uart_fd[1], &type, 1));
                if(bytes_read == -1) {
                    ALOGE("%s, state = %d, read error %s", __func__, packet_recv_state, strerror(errno));
                    return;
                }
                if(!bytes_read && packet_bytes_need) {
                    ALOGE("%s, state = %d, bytes_read 0", __func__, packet_recv_state);
                    return;
                }

                if (type < DATA_TYPE_COMMAND || type > DATA_TYPE_SCO) {
                    ALOGE("%s invalid data type: %d", __func__, type);
                    assert((type >= DATA_TYPE_COMMAND) && (type <= DATA_TYPE_SCO));
                }
                else {
                    packet_bytes_need -= bytes_read;
                    packet_recv_state = BESBT_PACKET_TYPE;
                    current_type = type;
                    h4_read_buffer[0] = type;
                }
            }while(packet_bytes_need);
            //fall through

        case BESBT_PACKET_TYPE:
            packet_bytes_need = hci_preamble_sizes[HCI_PACKET_TYPE_TO_INDEX(current_type)];
            h4_read_length = 0;
            packet_recv_state = BESBT_PACKET_HEADER;
            //fall through

        case BESBT_PACKET_HEADER:
            do {
                BES_NO_INTR(bytes_read = read(vnd_userial.uart_fd[1], &h4_read_buffer[h4_read_length + 1], packet_bytes_need));
                if(bytes_read == -1) {
                    ALOGE("%s, state = %d, read error %s", __func__, packet_recv_state, strerror(errno));
                    return;
                }
                if(!bytes_read && packet_bytes_need) {
                    ALOGE("%s, state = %d, bytes_read 0, type : %d", __func__, packet_recv_state, current_type);
                    return;
                }
                packet_bytes_need -= bytes_read;
                h4_read_length += bytes_read;
            }while(packet_bytes_need);
            packet_recv_state = BESBT_PACKET_CONTENT;

            if(current_type == DATA_TYPE_ACL) {
                packet_bytes_need = *(uint16_t *)&h4_read_buffer[COMMON_DATA_LENGTH_INDEX];
            } else if(current_type == DATA_TYPE_EVENT) {
                packet_bytes_need = h4_read_buffer[EVENT_DATA_LENGTH_INDEX];
            } else {
                packet_bytes_need = h4_read_buffer[COMMON_DATA_LENGTH_INDEX];
            }
            //fall through

        case BESBT_PACKET_CONTENT:
            while(packet_bytes_need) {
                BES_NO_INTR(bytes_read = read(vnd_userial.uart_fd[1], &h4_read_buffer[h4_read_length + 1], packet_bytes_need));
                if(bytes_read == -1) {
                    ALOGE("%s, state = %d, read error %s", __func__, packet_recv_state, strerror(errno));
                    return;
                }
                if(!bytes_read) {
                    ALOGE("%s, state = %d, bytes_read 0", __func__, packet_recv_state);
                    return;
                }

                packet_bytes_need -= bytes_read;
                h4_read_length += bytes_read;
            }
            packet_recv_state = BESBT_PACKET_END;
            //fall through

        case BESBT_PACKET_END:
            switch (current_type) {
                case DATA_TYPE_COMMAND:
#ifdef BES_HANDLE_CMD
                    userial_handle_cmd(&h4_read_buffer[1], h4_read_length);
#endif
                    if(vnd_userial.h5_enable){
                        h5_transmit_data_to_btc(h4_read_buffer, (h4_read_length + 1));
                    }
                    else
                        userial_vendor_transmit_data_to_btc(h4_read_buffer, (h4_read_length + 1));
                break;

                case DATA_TYPE_ACL:
                    if(vnd_userial.h5_enable){
                        h5_transmit_data_to_btc(h4_read_buffer, (h4_read_length + 1));
                    }
                    else
                        userial_vendor_transmit_data_to_btc(h4_read_buffer, (h4_read_length + 1));
                break;
           
                default:
                    ALOGE("%s invalid data type: %d", __func__, current_type);
                break;
            }

        break;

        default:

        break;
    }

    packet_recv_state = BESBT_PACKET_IDLE;
    packet_bytes_need = 0;
    current_type = 0;
    h4_read_length = 0;
}

//PROCESS from the controller. The data type have ACL/SCO/EVENT
// direction CONTROLLER -----> BT HOST
#ifdef BES_HANDLE_EVENT
static void userial_handle_event(unsigned char * recv_buffer, int total_length)
{
    BES_UNUSED(total_length);
    uint8_t event;
    uint8_t *p_data = recv_buffer;
    event = p_data[0];
    
    ALOGD("event:");
    userial_print_raw_data(recv_buffer, total_length);    
    switch (event) {
        case HCI_COMMAND_COMPLETE_EVT:
        {
            uint16_t opcode = STR_TO_U16(&p_data[3]);
            uint8_t* stream = &p_data[6];

            ALOGD("Bes evt:%02x %04x", event, opcode);
            if(opcode == HCI_READ_LOCAL_VERSION_INFO) {

            }
            else if(opcode == HCI_BLE_WRITE_ADV_ENABLE){

            }
        }
        break;
#ifdef CONFIG_SCO_OVER_HCI
        case HCI_ESCO_CONNECTION_COMP_EVT: {
            uint8_t status = p_data[2];
            if(!status){           
                sco_cb.sco_handle = STR_TO_U16(&p_data[3]);                
                if(!(sco_cb.voice_settings & 0x0003)) {
                    sco_cb.sco_packet_len = 120;
                    sco_cb.codec_type = SCO_CODEC_CVSD;
                }
                else{
                    //for bes msbc vendor packet
                    //lsb is valid. so packet len is 60*2
                    sco_cb.sco_packet_len = 120;
                    sco_cb.codec_type = SCO_CODEC_MSBC;
                }
                pthread_mutex_lock(&sco_cb.sco_mutex);
                sco_cb.sco_thread_event = 1;
                pthread_cond_signal(&sco_cb.sco_cond);    
                pthread_mutex_unlock(&sco_cb.sco_mutex);
            }
            ALOGD("sco_cb.codec_type:%d st:%x", sco_cb.codec_type, status);
        }
        break;
#endif
        default :
            break;
    }
}

#ifdef CONFIG_SCO_OVER_HCI
//uplink data headset mic-->remote device
void userial_write_pcm_to_audio_card(unsigned char *pcm, int length)
{

}

static void userial_handle_recv_sco_packet(unsigned char * recv_buffer, int total_length)
{
    uint8_t * data_p = NULL;
    uint8_t pcm_data[SCO_PACKET_PCM_LEN];
    size_t written = 0;
    uint16_t sco_handle = 0;
    uint8_t sco_data_len = recv_buffer[SCO_PREAMBLE_SIZE - 1];

    data_p = recv_buffer + SCO_PREAMBLE_SIZE;
    sco_handle = STR_TO_U16(recv_buffer);
    if(sco_handle !=  sco_cb.sco_handle)
        return;

    ALOGD("recv_sco_packet:%d %d", sco_cb.codec_type, sco_data_len);
    if(sco_cb.codec_type == SCO_CODEC_MSBC){
        int res = 0;
        res = sbc_decode(&sco_cb.sbc_dec, 
                            data_p + 2, 
                            SCO_PACKET_LEN - 2, 
                            pcm_data, 
                            SCO_PACKET_PCM_LEN, 
                            &written);
        if(res > 0){
            //decode success!!!
            userial_write_pcm_to_audio_card(pcm_data, SCO_PACKET_PCM_LEN);
        }

        ALOGD("sbc_decode:%d", res);
    }
    else{      
        userial_write_pcm_to_audio_card(data_p, SCO_PACKET_PCM_LEN);
    }
}
#endif

static int userial_handle_recv_data(unsigned char * recv_buffer, int total_length)
{
    serial_data_type_t type = 0;
    unsigned char * p_data = recv_buffer;
    int length = total_length;
    uint8_t event;

    switch (received_packet_state) {
        case BESBT_PACKET_IDLE:
            received_packet_bytes_need = 1;
            while(length) {
                type = p_data[0];
                length--;
                p_data++;
                if (type < DATA_TYPE_ACL || type > DATA_TYPE_EVENT) {
                    ALOGE("%s invalid data type: %d", __func__, type);
                    assert((type > DATA_TYPE_COMMAND) && (type <= DATA_TYPE_EVENT));
                    if(!length)
                        return total_length;

                    continue;
                }
                break;
            }
            recv_packet_current_type = type;
            received_packet_state = BESBT_PACKET_TYPE;
            //fall through

        case BESBT_PACKET_TYPE:
            received_packet_bytes_need = hci_preamble_sizes[HCI_PACKET_TYPE_TO_INDEX(recv_packet_current_type)];
            received_resvered_length = 0;
            received_packet_state = BESBT_PACKET_HEADER;
            //fall through

        case BESBT_PACKET_HEADER:
            if(length >= received_packet_bytes_need) {
                memcpy(&received_resvered_header[received_resvered_length], p_data, received_packet_bytes_need);
                received_resvered_length += received_packet_bytes_need;
                length -= received_packet_bytes_need;
                p_data += received_packet_bytes_need;
            }
            else {
                memcpy(&received_resvered_header[received_resvered_length], p_data, length);
                received_resvered_length += length;
                received_packet_bytes_need -= length;
                length = 0;
                return total_length;
            }
            received_packet_state = BESBT_PACKET_CONTENT;

            if(recv_packet_current_type == DATA_TYPE_ACL) {
                received_packet_bytes_need = STR_TO_U16(&received_resvered_header[ACL_PREAMBLE_SIZE - 2]);
            }
            else if(recv_packet_current_type == DATA_TYPE_EVENT){
                received_packet_bytes_need = received_resvered_header[EVENT_PREAMBLE_SIZE - 1];
            }
            else if(recv_packet_current_type == DATA_TYPE_SCO){
                received_packet_bytes_need = received_resvered_header[SCO_PREAMBLE_SIZE - 1];
            }
            else {
                received_packet_bytes_need = received_resvered_header[2];
            }
            //fall through

        case BESBT_PACKET_CONTENT:
            if(recv_packet_current_type == DATA_TYPE_EVENT) {
                event = received_resvered_header[0];

                if(event == HCI_COMMAND_COMPLETE_EVT) {
                    if(received_resvered_length == 2) {
                      if(length >= 1) {
                          *p_data = 1;
                      }
                    }
                }
                else if(event == HCI_COMMAND_STATUS_EVT) {
                    if(received_resvered_length < 4) {
                      int act_len = 4 - received_resvered_length;
                      if(length >= act_len) {
                          *(p_data + act_len -1) = 1;
                      }
                    }
                }
            }
            if(length >= received_packet_bytes_need) {
                memcpy(&received_resvered_header[received_resvered_length], p_data, received_packet_bytes_need);
                length -= received_packet_bytes_need;
                p_data += received_packet_bytes_need;
                received_resvered_length += received_packet_bytes_need;
                received_packet_bytes_need = 0;
            }
            else {
                memcpy(&received_resvered_header[received_resvered_length], p_data, length);
                received_resvered_length += length;
                received_packet_bytes_need -= length;
                length = 0;
                return total_length;
            }
            received_packet_state = BESBT_PACKET_END;
            //fall through

        case BESBT_PACKET_END:
            switch (recv_packet_current_type) {
                case DATA_TYPE_EVENT :
                    userial_handle_event(received_resvered_header, received_resvered_length);
                break;
#ifdef CONFIG_SCO_OVER_HCI
                case DATA_TYPE_SCO:
                    userial_handle_recv_sco_packet(received_resvered_header, received_resvered_length);
                break;
#endif
                default :
                break;
            }
        break;

        default:

        break;
    }

    received_packet_state = BESBT_PACKET_IDLE;
    received_packet_bytes_need = 0;
    recv_packet_current_type = 0;
    received_resvered_length = 0;

    return (total_length - length);
}
#endif

void userial_recv_uart_rawdata(unsigned char *buffer, unsigned int total_length)
{
    unsigned int length = total_length;
    uint16_t transmitted_length = 0;
#ifdef BES_HANDLE_EVENT
    unsigned int read_length = 0;
    do {
        read_length += userial_handle_recv_data(buffer + read_length, total_length - read_length);

    }while(read_length < total_length);
#endif
    while (length > 0) {
        ssize_t ret;
        BES_NO_INTR(ret = write(vnd_userial.uart_fd[1], buffer + transmitted_length, length));
        switch (ret) {
        case -1:
            ALOGE("In %s, error writing to the uart serial port: %s", __func__, strerror(errno));
            goto done;
        case 0:
            // If we wrote nothing, don't loop more because we
            // can't go to infinity or beyond
            goto done;
        default:
            transmitted_length += ret;
            length -= ret;
            break;
        }
    }
done:;
    return;
}

static void* userial_recv_uart_thread(void *arg)
{
    BES_UNUSED(arg);
    struct pollfd pfd[2];
    pfd[0].events = POLLIN|POLLHUP|POLLERR|POLLRDHUP;
    pfd[0].fd = vnd_userial.signal_fd[1];
    pfd[1].events = POLLIN|POLLHUP|POLLERR|POLLRDHUP;
    pfd[1].fd = vnd_userial.fd;
    int ret;
    unsigned char read_buffer[2056] = {0};
    ssize_t bytes_read;

    ALOGD("uart_thread fd = %d", vnd_userial.fd);
    while(vnd_userial.thread_running) {
        do{
            ret = poll(pfd, 2, 500);
        }while(ret == -1 && errno == EINTR && vnd_userial.thread_running);

        //exit signal is always at first index
        if(pfd[0].revents && !vnd_userial.thread_running) {
            ALOGE("receive exit signal and stop thread ");
            return NULL;
        }
	 ALOGD("uart rx:%x %x %x", pfd[1].revents, POLLIN, pfd[0].revents);
        if (pfd[1].revents & POLLIN) 
	 {
            BES_NO_INTR(bytes_read = read(vnd_userial.fd, read_buffer, sizeof(read_buffer)));
            if(!bytes_read)
                continue;

            ALOGE("uart rx:");
            userial_print_raw_data(read_buffer, bytes_read); 
            if(vnd_userial.h5_enable){
                bes_h5_recv(read_buffer, bytes_read);
            }
            else
                userial_recv_uart_rawdata(read_buffer, bytes_read);
        }

        if (pfd[1].revents & (POLLERR|POLLHUP)) {
            ALOGE("%s poll error, fd : %d", __func__, vnd_userial.fd);
            vnd_userial.btdriver_state = false;
            close(vnd_userial.fd);
            return NULL;
        }
        if (ret < 0)
        {
            ALOGE("%s : error (%d)", __func__, ret);
            continue;
        }
    }
    ALOGD("%s exit", __func__);
    return NULL;
}

#ifdef CONFIG_SCO_OVER_HCI
//downlink phone --> headset
int bes_mmap_sco_read_cb(unsigned char * str, unsigned short length)
{
    int ret = 0;
    unsigned int write_left = 0;

    pthread_mutex_lock(&sco_cb.sco_mutex);
    write_left = AvailableOfCQueue(&sco_downlink_queue);
    if(write_left > length){
        write_left = length;
    }
    ret = EnCQueue(&sco_downlink_queue, str, write_left);
    sco_cb.sco_thread_event = 1;
    pthread_cond_signal(&sco_cb.sco_cond);
    pthread_mutex_unlock(&sco_cb.sco_mutex);
    ALOGD("bes_mmap_sco_read_cb:%d %d", length, LengthOfCQueue(&sco_downlink_queue));
    return ret;
}

static void* userial_send_sco_thread(void *arg)
{
    int ret = 0;
    ssize_t written = 0;
    bool    enable_send = 0;
    uint8_t pcm_data[240];
    uint8_t sco_packet_data[60 + SCO_PACKET_HEAD_LEN];
    uint8_t* enc_data = &sco_packet_data[SCO_PACKET_HEAD_LEN];
    BES_UNUSED(arg);

    while(sco_cb.thread_sco_running) {
        pthread_mutex_lock(&sco_cb.sco_mutex);
        while((sco_cb.sco_thread_event == 0) && sco_cb.thread_sco_running) {
            pthread_cond_wait(&sco_cb.sco_cond, &sco_cb.sco_mutex);
        }
        sco_cb.sco_thread_event = 0;
        sco_cb.pcm_enc_seq = 0;
        pthread_mutex_unlock(&sco_cb.sco_mutex);

        while(1){
            enable_send = 0;
            pthread_mutex_lock(&sco_cb.sco_mutex);
            if(LengthOfCQueue(&sco_downlink_queue) >= sizeof(pcm_data)){
                enable_send = 1;
                ret = PeekCQueueToBuf(&sco_downlink_queue, pcm_data, sizeof(pcm_data));
                if(!ret)
                    DeCQueue(&sco_downlink_queue, NULL, sizeof(pcm_data));
            }
            pthread_mutex_unlock(&sco_cb.sco_mutex);

            if(enable_send){
                if(sco_cb.codec_type == SCO_CODEC_MSBC){
                    //todo for downlink
                    int res = sbc_encode(&sco_cb.sbc_enc, 
                                            pcm_data,
                                            sizeof(pcm_data),
                                            &enc_data[2],
                                            58,
                                            &written);
                    if(res <= 0){
                        ALOGE("sbc encode error!");
                    }else{
                        uint16_t msbc_head = btui_msbc_h2[sco_cb.pcm_enc_seq % 4];
                        enc_data[0] = msbc_head & 0xff;
                        enc_data[1] = (msbc_head >> 0x08) & 0xff;
                        //padding
                        enc_data[59] = 0;
                        sco_cb.pcm_enc_seq++;

                        sco_packet_data[0] = DATA_TYPE_SCO;
                        U16_TO_STR(&sco_packet_data[1], sco_cb.sco_handle);
                        sco_packet_data[3] = 60;
                        if(vnd_userial.h5_enable){
                            h5_transmit_data_to_btc(sco_packet_data, sizeof(sco_packet_data));
                        }
                        else
                            userial_vendor_transmit_data_to_btc(sco_packet_data, sizeof(sco_packet_data));
                    }
                }
                else{
                    if(vnd_userial.h5_enable){
                        h5_transmit_data_to_btc(pcm_data, sco_cb.sco_packet_len);
                    }
                    else{
                        userial_vendor_transmit_data_to_btc(pcm_data, sco_cb.sco_packet_len);
                    }
                }
            }else{
                break;
            }
        }
    }

    return NULL;
}
#endif

static void* userial_recv_socket_thread(void *arg)
{
    BES_UNUSED(arg);
    struct epoll_event events[64];
    int j;
    
    ALOGD("socket_thread fd = %d", vnd_userial.epoll_host_fd);
    while(vnd_userial.thread_running) {
        int ret;
        do{
            ret = epoll_wait(vnd_userial.epoll_host_fd, events, 32, 500);
        }while(vnd_userial.thread_running && ret == -1 && errno == EINTR);

        if (ret == -1) {
            ALOGE("%s error in epoll_wait: %s", __func__, strerror(errno));
        }
        for (j = 0; j < ret; ++j) {
            struct bt_object_t *object = (struct bt_object_t *)events[j].data.ptr;
            if (events[j].data.ptr == NULL)
                continue;
            else {
                if (events[j].events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR) && object->read_ready){
                    object->read_ready(object->context);
                }
                if (events[j].events & EPOLLOUT && object->write_ready){
                    object->write_ready(object->context);
                }
            }
        }
    }
    ALOGD("%s exit", __func__);
    return NULL;
}

int userial_socket_open()
{
    int ret = 0;
    struct epoll_event event;
    if((ret = socketpair(AF_UNIX, SOCK_STREAM, 0, vnd_userial.uart_fd)) < 0) {
        ALOGE("%s, errno : %s", __func__, strerror(errno));
        return ret;
    }

    if((ret = socketpair(AF_UNIX, SOCK_STREAM, 0, vnd_userial.signal_fd)) < 0) {
        ALOGE("%s, errno : %s", __func__, strerror(errno));
        return ret;
    }

    vnd_userial.epoll_host_fd = epoll_create(64);
    if (vnd_userial.epoll_host_fd == -1) {
        ALOGE("%s unable to create epoll instance: %s", __func__, strerror(errno));
        return -1;
    }

    bt_socket_object.fd = vnd_userial.uart_fd[1];
    bt_socket_object.read_ready = userial_recv_H4_rawdata;
    memset(&event, 0, sizeof(event));
    event.events |= EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    event.data.ptr = (void *)&bt_socket_object;
    if (epoll_ctl(vnd_userial.epoll_host_fd, EPOLL_CTL_ADD, vnd_userial.uart_fd[1], &event) == -1) {
        ALOGE("%s unable to register fd %d to epoll set: %s", __func__, vnd_userial.uart_fd[1], strerror(errno));
        close(vnd_userial.epoll_host_fd);
        return -1;
    }

    event.data.ptr = NULL;
    if (epoll_ctl(vnd_userial.epoll_host_fd, EPOLL_CTL_ADD, vnd_userial.signal_fd[1], &event) == -1) {
        ALOGE("%s unable to register signal fd %d to epoll set: %s", __func__, vnd_userial.signal_fd[1], strerror(errno));
        close(vnd_userial.epoll_host_fd);
        return -1;
    }
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    vnd_userial.thread_running = true;
    if (pthread_create(&vnd_userial.thread_socket_id, &thread_attr, userial_recv_socket_thread, NULL)!=0 )
    {
        ALOGE("pthread_create : %s", strerror(errno));
        close(vnd_userial.epoll_host_fd);
        return -1;
    }

    if (pthread_create(&vnd_userial.thread_uart_id, &thread_attr, userial_recv_uart_thread, NULL)!=0 )
    {
        ALOGE("pthread_create : %s", strerror(errno));
        close(vnd_userial.epoll_host_fd);
        vnd_userial.thread_running = false;
        pthread_join(vnd_userial.thread_socket_id, NULL);
        return -1;
    }
#ifdef CONFIG_SCO_OVER_HCI
    {
       pthread_attr_t thread_attr;
       pthread_attr_init(&thread_attr);
       pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
       if(pthread_create(&sco_cb.thread_send_sco_id, &thread_attr, userial_send_sco_thread, NULL)!= 0 )
       {
           ALOGE("pthread_create : %s", strerror(errno));
       }     
    }
#endif
    ret = vnd_userial.uart_fd[0];
    ALOGD("%s exit", __func__);
    return ret;
}



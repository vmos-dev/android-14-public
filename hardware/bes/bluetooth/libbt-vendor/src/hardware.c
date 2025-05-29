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
 *  Filename:      hardware.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *                      low power mode operations
 *
 ******************************************************************************/

#define LOG_TAG "bt_hwcfg"

#include <utils/Log.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_bes.h"
#include "esco_parameters.h"
#include "userial.h"
#include "userial_vendor.h"
#include "upio.h"
#include "bes_utils.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef BTHW_DBG
#define BTHW_DBG FALSE
#endif

#if (BTHW_DBG == TRUE)
#define BTHWDBG(param, ...) {ALOGD("BES_ "param, ## __VA_ARGS__);}
#else
#define BTHWDBG(param, ...) {}
#endif

#define FW_PATCHFILE_EXTENSION      ".hcd"
#define FW_PATCHFILE_EXTENSION_LEN  4
#define FW_PATCHFILE_PATH_MAXLEN    248 /* Local_Name length of return of
                                           HCI_Read_Local_Name */

#define HCI_CMD_MAX_LEN             258

#define HCI_RESET                               0x0C03
#define HCI_DBG_SET_BD_ADDR_CMD_OPCODE                0xFC32
#define HCI_READ_LOCAL_BDADDR                   0x1009

#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE        5
#define HCI_EVT_CMD_CMPL_LOCAL_NAME_STRING      6
#define HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY     6
#define HCI_EVT_CMD_CMPL_OPCODE                 3
#define LPM_CMD_PARAM_SIZE                      12
#define UPDATE_BAUDRATE_CMD_PARAM_SIZE          6
#define HCI_CMD_PREAMBLE_SIZE                   3
#define HCD_REC_PAYLOAD_LEN_BYTE                2
#define BD_ADDR_LEN                             6
#define LOCAL_NAME_BUFFER_LEN                   32
#define LOCAL_BDADDR_PATH_BUFFER_LEN            256

#define STREAM_TO_UINT16(u16, p) {u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); (p) += 2;}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT32_TO_STREAM(p, u32) {*(p)++ = (uint8_t)(u32); *(p)++ = (uint8_t)((u32) >> 8); *(p)++ = (uint8_t)((u32) >> 16); *(p)++ = (uint8_t)((u32) >> 24);}

#define SCO_INTERFACE_PCM  0
#define SCO_INTERFACE_I2S  1

/* one byte is for enable/disable
      next 2 bytes are for codec type */
#define SCO_CODEC_PARAM_SIZE                    3

/******************************************************************************
**  Local type definitions
******************************************************************************/

/* Hardware Configuration State */
enum {
    HW_CFG_START = 1,
    HW_CFG_READ_LOCAL_NAME,
    HW_CFG_SET_BD_ADDR
#if (USE_CONTROLLER_BDADDR == TRUE)
    , HW_CFG_READ_BD_ADDR
#endif
};

/* h/w config control block */
typedef struct
{
    uint8_t state;                          /* Hardware configuration state */
    int     fw_fd;                          /* FW patch file fd */
    uint8_t f_set_baud_2;                   /* Baud rate switch state */
    char    local_chip_name[LOCAL_NAME_BUFFER_LEN];
} bt_hw_cfg_cb_t;

/* low power mode parameters */
typedef struct
{
    uint8_t sleep_mode;                     /* 0(disable),1(UART),9(H5) */
    uint8_t host_stack_idle_threshold;      /* Unit scale 300ms/25ms */
    uint8_t host_controller_idle_threshold; /* Unit scale 300ms/25ms */
    uint8_t bt_wake_polarity;               /* 0=Active Low, 1= Active High */
    uint8_t host_wake_polarity;             /* 0=Active Low, 1= Active High */
    uint8_t allow_host_sleep_during_sco;
    uint8_t combine_sleep_mode_and_lpm;
    uint8_t enable_uart_txd_tri_state;      /* UART_TXD Tri-State */
    uint8_t sleep_guard_time;               /* sleep guard time in 12.5ms */
    uint8_t wakeup_guard_time;              /* wakeup guard time in 12.5ms */
    uint8_t txd_config;                     /* TXD is high in sleep state */
    uint8_t pulsed_host_wake;               /* pulsed host wake if mode = 1 */
} bt_lpm_param_t;

/* Firmware re-launch settlement time */
typedef struct {
    const char *chipset_name;
    const uint32_t delay_time;
} fw_settlement_entry_t;

/******************************************************************************
**  Externs
******************************************************************************/

void hw_config_cback(void *p_evt_buf);
extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];


/******************************************************************************
**  Static variables
******************************************************************************/

static char fw_patchfile_path[256] = FW_PATCHFILE_LOCATION;
static char fw_patchfile_name[128] = { 0 };
#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
static int fw_patch_settlement_delay = -1;
#endif

static bt_hw_cfg_cb_t hw_cfg_cb;

static bt_lpm_param_t lpm_param =
{
    LPM_SLEEP_MODE,
    LPM_IDLE_THRESHOLD,
    LPM_HC_IDLE_THRESHOLD,
    LPM_BT_WAKE_POLARITY,
    LPM_HOST_WAKE_POLARITY,
    LPM_ALLOW_HOST_SLEEP_DURING_SCO,
    LPM_COMBINE_SLEEP_MODE_AND_LPM,
    LPM_ENABLE_UART_TXD_TRI_STATE,
    0,  /* not applicable */
    0,  /* not applicable */
    0,  /* not applicable */
    LPM_PULSED_HOST_WAKE
};

/******************************************************************************
**  Controller Initialization Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function        ms_delay
**
** Description     sleep unconditionally for timeout milliseconds
**
** Returns         None
**
*******************************************************************************/
void ms_delay (uint32_t timeout)
{
    struct timespec delay;
    int err;

    if (timeout == 0)
        return;

    delay.tv_sec = timeout / 1000;
    delay.tv_nsec = 1000 * 1000 * (timeout%1000);

    /* [u]sleep can't be used because it uses SIGALRM */
    do {
        err = nanosleep(&delay, &delay);
    } while (err < 0 && errno ==EINTR);
}

/*******************************************************************************
**
** Function        line_speed_to_userial_baud
**
** Description     helper function converts line speed number into USERIAL baud
**                 rate symbol
**
** Returns         unit8_t (USERIAL baud symbol)
**
*******************************************************************************/
uint8_t line_speed_to_userial_baud(uint32_t line_speed)
{
    uint8_t baud;

    if (line_speed == 4000000)
        baud = USERIAL_BAUD_4M;
    else if (line_speed == 3000000)
        baud = USERIAL_BAUD_3M;
    else if (line_speed == 2000000)
        baud = USERIAL_BAUD_2M;
    else if (line_speed == 1500000)
        baud = USERIAL_BAUD_1_5M;
    else if (line_speed == 1000000)
        baud = USERIAL_BAUD_1M;
    else if (line_speed == 921600)
        baud = USERIAL_BAUD_921600;
    else if (line_speed == 460800)
        baud = USERIAL_BAUD_460800;
    else if (line_speed == 230400)
        baud = USERIAL_BAUD_230400;
    else if (line_speed == 115200)
        baud = USERIAL_BAUD_115200;
    else if (line_speed == 57600)
        baud = USERIAL_BAUD_57600;
    else if (line_speed == 19200)
        baud = USERIAL_BAUD_19200;
    else if (line_speed == 9600)
        baud = USERIAL_BAUD_9600;
    else if (line_speed == 1200)
        baud = USERIAL_BAUD_1200;
    else if (line_speed == 600)
        baud = USERIAL_BAUD_600;
    else
    {
        ALOGE( "userial vendor: unsupported baud speed %d", line_speed);
        baud = USERIAL_BAUD_115200;
    }

    return baud;
}


/*******************************************************************************
**
** Function         hw_strncmp
**
** Description      Used to compare two strings in caseless
**
** Returns          0: match, otherwise: not match
**
*******************************************************************************/
static int hw_strncmp (const char *p_str1, const char *p_str2, const int len)
{
    int i;

    if (!p_str1 || !p_str2)
        return (1);

    for (i = 0; i < len; i++)
    {
        if (toupper(p_str1[i]) != toupper(p_str2[i]))
            return (i+1);
    }

    return 0;
}

/*******************************************************************************
**
** Function         hw_config_set_bdaddr
**
** Description      Program controller's Bluetooth Device Address
**
** Returns          TRUE, if valid address is sent
**                  FALSE, otherwise
**
*******************************************************************************/
static uint8_t hw_config_set_bdaddr(HC_BT_HDR *p_buf)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    ALOGI("Setting local bd addr to %02X:%02X:%02X:%02X:%02X:%02X",
        vnd_local_bd_addr[0], vnd_local_bd_addr[1], vnd_local_bd_addr[2],
        vnd_local_bd_addr[3], vnd_local_bd_addr[4], vnd_local_bd_addr[5]);

    UINT16_TO_STREAM(p, HCI_DBG_SET_BD_ADDR_CMD_OPCODE);
    *p++ = BD_ADDR_LEN; /* parameter length */
    *p++ = vnd_local_bd_addr[5];
    *p++ = vnd_local_bd_addr[4];
    *p++ = vnd_local_bd_addr[3];
    *p++ = vnd_local_bd_addr[2];
    *p++ = vnd_local_bd_addr[1];
    *p = vnd_local_bd_addr[0];

    p_buf->len = HCI_CMD_PREAMBLE_SIZE + BD_ADDR_LEN;
    hw_cfg_cb.state = HW_CFG_SET_BD_ADDR;

    retval = bt_vendor_cbacks->xmit_cb(HCI_DBG_SET_BD_ADDR_CMD_OPCODE, p_buf, \
                                 hw_config_cback);

    return (retval);
}

#if (USE_CONTROLLER_BDADDR == TRUE)
/*******************************************************************************
**
** Function         hw_config_read_bdaddr
**
** Description      Read controller's Bluetooth Device Address
**
** Returns          TRUE, if valid address is sent
**                  FALSE, otherwise
**
*******************************************************************************/
static uint8_t hw_config_read_bdaddr(HC_BT_HDR *p_buf)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    UINT16_TO_STREAM(p, HCI_READ_LOCAL_BDADDR);
    *p = 0; /* parameter length */

    p_buf->len = HCI_CMD_PREAMBLE_SIZE;
    hw_cfg_cb.state = HW_CFG_READ_BD_ADDR;

    retval = bt_vendor_cbacks->xmit_cb(HCI_READ_LOCAL_BDADDR, p_buf, \
                                 hw_config_cback);

    return (retval);
}
#endif // (USE_CONTROLLER_BDADDR == TRUE)

static timer_t localtimer = 0;

static void local_timer_handler(union sigval sigev_value)
{
    bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
    OsFreeTimer(localtimer);
}
static void start_fwcfg_cbtimer()
{
    if(localtimer == 0)
        localtimer = OsAllocateTimer(local_timer_handler, NULL);
    OsStartTimer(localtimer, 40, 0);
}

/*******************************************************************************
**
** Function         hw_config_cback
**
** Description      Callback function for controller configuration
**
** Returns          None
**
*******************************************************************************/
void hw_config_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    char        *p_name, *p_tmp;
    uint8_t     *p, status;
    uint16_t    opcode;
    HC_BT_HDR  *p_buf=NULL;
    uint8_t     is_proceeding = FALSE;
    int         i;
    int         delay=100;
#if (USE_CONTROLLER_BDADDR == TRUE)
    const uint8_t null_bdaddr[BD_ADDR_LEN] = {0,0,0,0,0,0};
#endif

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    /* Ask a new buffer big enough to hold any HCI commands sent in here */
    if ((status == 0) && bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_MAX_LEN);

    if (p_buf != NULL)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->len = 0;
        p_buf->layer_specific = 0;

        p = (uint8_t *) (p_buf + 1);

        switch (hw_cfg_cb.state)
        {
            case HW_CFG_START:
                if (UART_TARGET_BAUD_RATE > 3000000)
                {
                
                }
                is_proceeding = hw_config_set_bdaddr(p_buf);
                break;
                
                /* fall through intentionally */
            case HW_CFG_SET_BD_ADDR:
                ALOGI("vendor lib fwcfg completed");
                bt_vendor_cbacks->dealloc(p_buf);
                bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);

                hw_cfg_cb.state = 0;

                if (hw_cfg_cb.fw_fd != -1)
                {
                    close(hw_cfg_cb.fw_fd);
                    hw_cfg_cb.fw_fd = -1;
                }

                is_proceeding = TRUE;
                break;
        } // switch(hw_cfg_cb.state)
    } // if (p_buf != NULL)

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (is_proceeding == FALSE)
    {
        ALOGE("vendor lib fwcfg aborted!!!");
        if (bt_vendor_cbacks)
        {
            if (p_buf != NULL)
                bt_vendor_cbacks->dealloc(p_buf);

            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }

        if (hw_cfg_cb.fw_fd != -1)
        {
            close(hw_cfg_cb.fw_fd);
            hw_cfg_cb.fw_fd = -1;
        }

        hw_cfg_cb.state = 0;
    }
}

/*****************************************************************************
**   Hardware Configuration Interface Functions
*****************************************************************************/


/*******************************************************************************
**
** Function        hw_config_start
**
** Description     Kick off controller initialization process
**
** Returns         None
**
*******************************************************************************/
void hw_config_start(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    hw_cfg_cb.state = 0;
    hw_cfg_cb.fw_fd = -1;
    hw_cfg_cb.f_set_baud_2 = FALSE;

    /* Start from sending HCI_RESET */

    if (bt_vendor_cbacks)
    {
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        hw_cfg_cb.state = HW_CFG_START;

        bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_config_cback);
    }
    else
    {
        if (bt_vendor_cbacks)
        {
            ALOGE("vendor lib fw conf aborted [no buffer]");
            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}

/*******************************************************************************
**
** Function        hw_lpm_get_idle_timeout
**
** Description     Calculate idle time based on host stack idle threshold
**
** Returns         idle timeout value
**
*******************************************************************************/
uint32_t hw_lpm_get_idle_timeout(void)
{
    uint32_t timeout_ms;

    /* set idle time to be LPM_IDLE_TIMEOUT_MULTIPLE times of
     * host stack idle threshold (in 300ms/25ms)
     */
    timeout_ms = (uint32_t)lpm_param.host_stack_idle_threshold \
                            * LPM_IDLE_TIMEOUT_MULTIPLE;

    timeout_ms *= 300;

    return timeout_ms;
}

/*******************************************************************************
**
** Function        hw_lpm_set_wake_state
**
** Description     Assert/Deassert BT_WAKE
**
** Returns         None
**
*******************************************************************************/
void hw_lpm_set_wake_state(uint8_t wake_assert)
{
    uint8_t state = (wake_assert) ? UPIO_ASSERT : UPIO_DEASSERT;

    upio_set(UPIO_BT_WAKE, state, lpm_param.bt_wake_polarity);
}

/*******************************************************************************
**
** Function        hw_set_patch_file_path
**
** Description     Set the location of firmware patch file
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_file_path(char *p_conf_name, char *p_conf_value, int param)
{

    strcpy(fw_patchfile_path, p_conf_value);

    return 0;
}

/*******************************************************************************
**
** Function        hw_set_patch_file_name
**
** Description     Give the specific firmware patch filename
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_file_name(char *p_conf_name, char *p_conf_value, int param)
{

    strcpy(fw_patchfile_name, p_conf_value);

    return 0;
}

#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
/*******************************************************************************
**
** Function        hw_set_patch_settlement_delay
**
** Description     Give the specific firmware patch settlement time in milliseconds
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_settlement_delay(char *p_conf_name, char *p_conf_value, int param)
{
    fw_patch_settlement_delay = atoi(p_conf_value);

    return 0;
}
#endif  //VENDOR_LIB_RUNTIME_TUNING_ENABLED

/*****************************************************************************
**   Sample Codes Section
*****************************************************************************/

#if (HW_END_WITH_HCI_RESET == TRUE)
/*******************************************************************************
**
** Function         hw_epilog_cback
**
** Description      Callback function for Command Complete Events from HCI
**                  commands sent in epilog process.
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p, status;
    uint16_t    opcode;

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    BTHWDBG("%s Opcode:0x%04X Status: %d", __FUNCTION__, opcode, status);

    if (bt_vendor_cbacks)
    {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);

        /* Once epilog process is done, must call epilog_cb callback
           to notify caller */
        bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    }
}

/*******************************************************************************
**
** Function         hw_epilog_process
**
** Description      Sample implementation of epilog process
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_process(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    BTHWDBG("hw_epilog_process");

    /* Sending a HCI_RESET */
    if (bt_vendor_cbacks)
    {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_epilog_cback);
    }
    else
    {
        if (bt_vendor_cbacks)
        {
            ALOGE("vendor lib epilog process aborted [no buffer]");
            bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}
#endif // (HW_END_WITH_HCI_RESET == TRUE)

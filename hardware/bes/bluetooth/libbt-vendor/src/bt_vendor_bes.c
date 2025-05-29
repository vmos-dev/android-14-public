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
 *  Filename:      bt_vendor_brcm.c
 *
 *  Description:   Broadcom vendor specific library implementation
 *
 ******************************************************************************/

#define LOG_TAG "bt_vendor"

#include <utils/Log.h>
#include <string.h>
#include "bt_vendor_bes.h"
#include "upio.h"
#include "userial_vendor.h"

#ifndef BTVND_DBG
#define BTVND_DBG FALSE
#endif

#if (BTVND_DBG == TRUE)
#define BTVNDDBG(param, ...) {ALOGD("BES_ "param, ## __VA_ARGS__);}
#else
#define BTVNDDBG(param, ...) {}
#endif

/******************************************************************************
**  Externs
******************************************************************************/

void hw_config_start(void);
void hw_lpm_set_wake_state(uint8_t wake_assert);
#if (HW_END_WITH_HCI_RESET == TRUE)
void hw_epilog_process(void);
#endif

/******************************************************************************
**  Variables
******************************************************************************/

bt_vendor_callbacks_t *bt_vendor_cbacks = NULL;
uint8_t vnd_local_bd_addr[6]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/******************************************************************************
**  Local type definitions
******************************************************************************/

/******************************************************************************
**  Static Variables
******************************************************************************/

static  tUSERIAL_CFG userial_init_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_921600,
    USERIAL_HW_FLOW_CTRL_ON,
    false
};

/******************************************************************************
**  Functions
******************************************************************************/

/*****************************************************************************
**
**   BLUETOOTH VENDOR INTERFACE LIBRARY FUNCTIONS
**
*****************************************************************************/
static char *bes_trim(char *str) {
    while (isspace(*str))
        ++str;

    if (!*str)
        return str;

    char *end_str = str + strlen(str) - 1;
    while (end_str > str && isspace(*end_str))
        --end_str;

    end_str[1] = '\0';
    return str;
}

static void load_besbt_stack_conf()
{
    char *split;
    FILE *fp = fopen(VENDOR_LIB_CONF_FILE, "rt");
    if (!fp) {
      ALOGE("%s unable to open file '%s': %s", __func__, VENDOR_LIB_CONF_FILE, strerror(errno));
      return;
    }
    int line_num = 0;
    char line[1024];
    char uart_device[1024];

    while (fgets(line, sizeof(line), fp)) {
        char *line_ptr = bes_trim(line);
        ++line_num;

        // Skip blank and comment lines.
        if (*line_ptr == '\0' || *line_ptr == '#' || *line_ptr == '[')
          continue;

        split = strchr(line_ptr, '=');
        if (!split) {
            ALOGE("%s no key/value separator found on line %d.", __func__, line_num);
            continue;
        }

        *split = '\0';
        char *endptr;

        userial_vendor_set_h5(userial_init_cfg.h5_enable);
        if(!strcmp(bes_trim(line_ptr), "BtDevicePath")) {
            sprintf(uart_device, "%s", bes_trim(split+1));
            userial_set_port(uart_device);
        }
        else if(!strcmp(bes_trim(line_ptr), "UartBaudRate")){
            uint8_t uart_baud = 0;
            uint32_t baud_rate = 0;
            baud_rate = strtol(bes_trim(split+1), &endptr, 0);
            cfg_to_uart_baud(&uart_baud, baud_rate);
            userial_init_cfg.baud = uart_baud;
        }
        else if(!strcmp(bes_trim(line_ptr), "UartFlowCtl")){
            if(!strcmp(bes_trim(split+1), "true")){
                userial_init_cfg.hw_fctrl = USERIAL_HW_FLOW_CTRL_ON;
            }else
                userial_init_cfg.hw_fctrl = USERIAL_HW_FLOW_CTRL_OFF;
        }
        else if(!strcmp(bes_trim(line_ptr), "UartH5")){
            if(!strcmp(bes_trim(split+1), "true")){
                userial_vendor_set_h5(true);
            }
            else{
                userial_vendor_set_h5(false);
            }
        }
    }

    fclose(fp);

}

static int init(const bt_vendor_callbacks_t* p_cb, unsigned char *local_bdaddr)
{
    ALOGI("init");

    if (p_cb == NULL)
    {
        ALOGE("init failed with no user callbacks!");
        return -1;
    }

#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
    ALOGW("*****************************************************************");
    ALOGW("*****************************************************************");
    ALOGW("** Warning - BT Vendor Lib is loaded in debug tuning mode!");
    ALOGW("**");
    ALOGW("** If this is not intentional, rebuild libbt-vendor.so ");
    ALOGW("** with VENDOR_LIB_RUNTIME_TUNING_ENABLED=FALSE and ");
    ALOGW("** check if any run-time tuning parameters needed to be");
    ALOGW("** carried to the build-time configuration accordingly.");
    ALOGW("*****************************************************************");
    ALOGW("*****************************************************************");
#endif
    userial_vendor_init();

    load_besbt_stack_conf();
    
    upio_init();

    /* store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t *) p_cb;

    /* This is handed over from the stack */
    memcpy(vnd_local_bd_addr, local_bdaddr, 6);
    return 0;
}

uint32_t hw_lpm_get_idle_timeout(void);
/** Requested operations */
static int op(bt_vendor_opcode_t opcode, void *param)
{
    int retval = 0;

    BTVNDDBG("op for %d", opcode);

    switch(opcode)
    {
        case BT_VND_OP_POWER_CTRL:
            {
                int *state = (int *) param;
                // upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
                if (*state == BT_VND_PWR_ON)
                {
                    ALOGW("NOTE: BT_VND_PWR_ON now forces power-off first");
                    upio_set_bluetooth_power(UPIO_BT_POWER_ON);
                    usleep(200000);
                } else {
                    /* Make sure wakelock is released */
                    upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
                    hw_lpm_set_wake_state(false);
                    usleep(200000);
                }
            }
            break;

        case BT_VND_OP_FW_CFG:
            {
                hw_config_start();
            }
            break;

        case BT_VND_OP_SCO_CFG:
            {
                retval = -1;
            }
            break;

        case BT_VND_OP_USERIAL_OPEN:
            {
                int (*fd_array)[] = (int (*)[]) param;
                int fd, idx;
                fd = userial_vendor_open((tUSERIAL_CFG *) &userial_init_cfg);

                fd = userial_socket_open();
                if (fd != -1)
                {
                    for (idx=0; idx < CH_MAX; idx++)
                        (*fd_array)[idx] = fd;

                    retval = 1;
                }
                /* retval contains numbers of open fd of HCI channels */
            }
            break;

        case BT_VND_OP_USERIAL_CLOSE:
            {
                userial_vendor_close();
            }
            break;

        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
            {
                uint32_t *timeout_ms = (uint32_t *) param;
                *timeout_ms = hw_lpm_get_idle_timeout();
            }
            break;

        case BT_VND_OP_LPM_SET_MODE:
            {

            }
            break;

        case BT_VND_OP_LPM_WAKE_SET_STATE:
            {
                uint8_t *state = (uint8_t *) param;
                uint8_t wake_assert = (*state == BT_VND_LPM_WAKE_ASSERT) ? \
                                        TRUE : FALSE;

                hw_lpm_set_wake_state(wake_assert);
            }
            break;

         case BT_VND_OP_SET_AUDIO_STATE:
            {
            }
            break;

        case BT_VND_OP_EPILOG:
            {
#if (HW_END_WITH_HCI_RESET == FALSE)
                if (bt_vendor_cbacks)
                {
                    bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
                }
#else
                hw_epilog_process();
#endif
            }
            break;
    }

    return retval;
}

/** Closes the interface */
static void cleanup( void )
{
    BTVNDDBG("cleanup");

    upio_cleanup();

    bt_vendor_cbacks = NULL;
}

// Entry point of DLib
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    init,
    op,
    cleanup
};

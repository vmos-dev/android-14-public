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
 *  Filename:      userial_vendor.h
 *
 *  Description:   Contains vendor-specific definitions used in serial port
 *                 controls
 *
 ******************************************************************************/

#ifndef USERIAL_VENDOR_H
#define USERIAL_VENDOR_H

#include "bt_vendor_bes.h"
#include "userial.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define BES_UNUSED(x) (void)(x)
#define BES_NO_INTR(fn)  do {} while ((fn) == -1 && errno == EINTR)

#define STR_TO_U16(x) ((x)[0] | ((x)[1] << 8))
#define U16_TO_STR(p, x) {(p)[0] = (x) & 0xff; (p)[1] = ((x) >> 0x08)&0xff;}

typedef enum {
  DATA_TYPE_UNKNOWN = 0,
  DATA_TYPE_COMMAND = 1,
  DATA_TYPE_ACL = 2,
  DATA_TYPE_SCO = 3,
  DATA_TYPE_EVENT = 4
} serial_data_type_t;

/**** baud rates ****/
#define USERIAL_BAUD_300        0
#define USERIAL_BAUD_600        1
#define USERIAL_BAUD_1200       2
#define USERIAL_BAUD_2400       3
#define USERIAL_BAUD_9600       4
#define USERIAL_BAUD_19200      5
#define USERIAL_BAUD_57600      6
#define USERIAL_BAUD_115200     7
#define USERIAL_BAUD_230400     8
#define USERIAL_BAUD_460800     9
#define USERIAL_BAUD_921600     10
#define USERIAL_BAUD_1M         11
#define USERIAL_BAUD_1_5M       12
#define USERIAL_BAUD_2M         13
#define USERIAL_BAUD_3M         14
#define USERIAL_BAUD_4M         15
#define USERIAL_BAUD_AUTO       16

/**** Data Format ****/
/* Stop Bits */
#define USERIAL_STOPBITS_1      1
#define USERIAL_STOPBITS_1_5    (1<<1)
#define USERIAL_STOPBITS_2      (1<<2)

/* Parity Bits */
#define USERIAL_PARITY_NONE     (1<<3)
#define USERIAL_PARITY_EVEN     (1<<4)
#define USERIAL_PARITY_ODD      (1<<5)

/* Data Bits */
#define USERIAL_DATABITS_5      (1<<6)
#define USERIAL_DATABITS_6      (1<<7)
#define USERIAL_DATABITS_7      (1<<8)
#define USERIAL_DATABITS_8      (1<<9)

#define USERIAL_HW_FLOW_CTRL_OFF  0
#define USERIAL_HW_FLOW_CTRL_ON    1

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
/* These are the ioctl values used for bt_wake ioctl via UART driver. you may
 * need to redefine them on you platform!
 * Logically they need to be unique and not colide with existing uart ioctl's.
 */
#ifndef USERIAL_IOCTL_BT_WAKE_ASSERT
#define USERIAL_IOCTL_BT_WAKE_ASSERT   0x8003
#endif
#ifndef USERIAL_IOCTL_BT_WAKE_DEASSERT
#define USERIAL_IOCTL_BT_WAKE_DEASSERT 0x8004
#endif
#ifndef USERIAL_IOCTL_BT_WAKE_GET_ST
#define USERIAL_IOCTL_BT_WAKE_GET_ST   0x8005
#endif
#endif // (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)

/******************************************************************************
**  Type definitions
******************************************************************************/
#define BT_DATA_RECEIVED 1
#define BT_DATA_SEND     0
struct bt_object_t {
  int fd;                              // the file descriptor to monitor for events.
  void *context;                       // a context that's passed back to the *_ready functions..
  pthread_mutex_t lock;                // protects the lifetime of this object and all variables.

  void (*read_ready)(void *context);   // function to call when the file descriptor becomes readable.
  void (*write_ready)(void *context);  // function to call when the file descriptor becomes writeable.
};

/* Structure used to configure serial port during open */
typedef struct
{
    uint16_t fmt;       /* Data format */
    uint8_t  baud;      /* Baud rate */
    uint8_t hw_fctrl; /*hardware flowcontrol*/
    uint8_t h5_enable;
} tUSERIAL_CFG;

typedef enum {
#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    USERIAL_OP_ASSERT_BT_WAKE,
    USERIAL_OP_DEASSERT_BT_WAKE,
    USERIAL_OP_GET_BT_WAKE_STATE,
#endif
    USERIAL_OP_NOP,
} userial_vendor_ioctl_op_t;

enum {
    BESBT_PACKET_IDLE,
    BESBT_PACKET_TYPE,
    BESBT_PACKET_HEADER,
    BESBT_PACKET_CONTENT,
    BESBT_PACKET_END
};
/******************************************************************************
**  Extern variables and functions
******************************************************************************/

/******************************************************************************
**  Functions
******************************************************************************/

/*******************************************************************************
**
** Function        userial_vendor_init
**
** Description     Initialize userial vendor-specific control block
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_init(void);

void userial_vendor_set_h5(bool h5_enable);

/*******************************************************************************
**
** Function        userial_vendor_open
**
** Description     Open the serial port with the given configuration
**
** Returns         device fd
**
*******************************************************************************/
int userial_vendor_open(tUSERIAL_CFG *p_cfg);

/*******************************************************************************
**
** Function        userial_vendor_close
**
** Description     Conduct vendor-specific close work
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_close(void);

/*******************************************************************************
**
** Function        userial_vendor_set_baud
**
** Description     Set new baud rate
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_set_baud(uint8_t userial_baud);

/*******************************************************************************
**
** Function        userial_vendor_ioctl
**
** Description     ioctl inteface
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_ioctl(userial_vendor_ioctl_op_t op, void *p_data);

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
int userial_set_port(char *p_conf_value);

/*******************************************************************************
**
** Function        cfg_to_uart_baud
**
** Description     helper function converts  user cfg
**                      conforming baud rates to USERIAL baud rates
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t cfg_to_uart_baud(uint8_t * cfg_baud, uint32_t baud);

/*******************************************************************************
**
** Function        userial_socket_open
**
** Description     create communication socket to connect host <-->controller
**                      
**
** Returns         fd
**
*******************************************************************************/
int userial_socket_open();

void userial_recv_uart_rawdata(unsigned char *buffer, unsigned int total_length);

uint16_t userial_vendor_transmit_data_to_btc(uint8_t *data, uint16_t total_length);

#define BES_HANDLE_EVENT
#define BES_HANDLE_CMD
#define CONFIG_SCO_OVER_HCI
#endif /* USERIAL_VENDOR_H */


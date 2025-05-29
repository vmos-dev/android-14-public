/*
 * Copyright 2023 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TV_PCIE_H__
#define __TV_PCIE_H__

#include <utils/RefBase.h>

//=======================rk_pcie_types.h=================================
#define RKEP_MODE_ERR                   0xffff

/* DMA Task支持最大数量 */
#define PCIE_DMA_TASK_MAX               (255)

enum EBuff_Type {
    E_SEND = 0, //发送buff
    E_RECV,    //接收buff
};

enum ETask_Msg_Type {
    E_TASK_MSG_NONE = 0,
    E_TASK_MSG_CREATE,      //创建任务
    E_TASK_MSG_DESTORY,     //销毁任务
    E_TASK_MSG_USER,        //用户自定义
    E_TASK_MSG_MAX,
};

struct pcie_buff_node {
    unsigned char *vir_addr;
    size_t phy_addr;
    size_t size;
};

struct pcie_task_msg_st {
    enum ETask_Msg_Type type;
    int task_id;
    size_t size;
    char lock;
    unsigned char data[0];
};
//===================================================================

#define PCIE_RECV_SEND_BUF_NUM      SIDEBAND_WINDOW_BUFF_CNT

enum Pcie_Mode {
    PCIE_DISABLE = 0x0,
    PCIE_RC = 0x1,
    PCIE_EP = 0x2,
};

enum Pcie_State {
    PCIE_STATE_UNSET = 0x0,
    PCIE_STATE_WAIT_DEV_INIT = 0x1,
    PCIE_STATE_WAIT_CONNECT = 0x2,
    PCIE_STATE_WAIT_REQ = 0x3,
    PCIE_STATE_STREAM_TRANS = 0x4,
};

enum Pcie_Cmd {
    PCIE_CMD_HDMIIN_NON = -1,
    PCIE_CMD_HDMIIN_INIT = 0x0,
    PCIE_CMD_HDMIIN_CTRL = 0x1,
    PCIE_CMD_HDMIIN_SOURCE_CHANGE = 0x2,
    PCIE_CMD_HDMIIN_INPUTIN_OUT = 0x3,
    PCIE_CMD_HDMIIN_RESET = 0x4,
};

struct pcie_user_cmd_st {
    unsigned int cmd = PCIE_CMD_HDMIIN_NON;
    unsigned int rc_exit = 0;
    unsigned int ep_exit = 0;
    unsigned int ep_need_restart = PCIE_CMD_HDMIIN_NON;

    //msg timestamp
    long rcMsgTimestamp = 0;
    long epMsgTimestamp = 0;

    //input info
    int inDevConnected = 0;
    int frameWidth = 0;
    int frameHeight = 0;
    int framePixelFormat = 0;
};

#endif

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

#ifndef __TV_PCIE_RC_H__
#define __TV_PCIE_RC_H__

#include <utils/Mutex.h>
#include <utils/threads.h>
#include "common/TvInput_Buffer_Manager_gralloc4_impl.h"
#include <common/Utils.h>
#include "TvPcie.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "tv_input_pcie_rc"
#endif

//=======================rk_pcie_rc.h=================================
/* Vendor和Device相同的PCI设备最大支持数量 */
#define PCI_DEV_MAX             32
/* PCI总线地址最大字符数量 */
#define PCI_BUS_MAX_NAME        16

typedef void* RK_PCIE_HANDLES;

struct pcie_dev_bus {
    int dev_max_num;
    char pci_bus_address[PCI_DEV_MAX][PCI_BUS_MAX_NAME];
};

struct pcie_dev_attr_st {
    int vendor_id;
    int device_id;
    char pci_bus_address[PCI_BUS_MAX_NAME];
    unsigned char using_rc_dma: 1;
    unsigned char rc_dma_chn: 1;
    unsigned char enable_speed: 1;
};

struct pcie_task_attr_st {
    int wait_timeout_ms;
    unsigned int send_buff_num;
    size_t send_buff_size;
    unsigned int recv_buff_num;
    size_t recv_buff_size;
};

struct pcie_dev_mode {
    unsigned short mode;
    unsigned short submode;
};
//=====================================================================

#define PCIE_VENDOR_ID              0x1d87
#define PCIE_DEVICE_ID              0x356a

struct rc_context_st {
    long buff_size;
    RK_PCIE_HANDLES rk_handle;
    struct pcie_dev_attr_st dev_attr;
    struct pcie_dev_mode devmode;
    struct pcie_task_attr_st task_attr;
    struct pcie_user_cmd_st user_cmd;
    char pci_bus_address[PCI_BUS_MAX_NAME];
    int task_id;
    struct pcie_dev_bus dev_bus;
};

namespace android {

class TvPcieRc : public RefBase {
  public:
    TvPcieRc();
    virtual ~TvPcieRc();

  private:
    class SendStreamThread : public Thread {
        TvPcieRc* mSource;
        public:
            SendStreamThread(TvPcieRc* source) :
                Thread(false), mSource(source) { }
            virtual void onFirstRef() {
                run("tif pcie_ep thread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                mSource->sendStreamThread();
                return false;
            }
    };

  public:
    void setDebugLevel(int debugLevel);
    int init(int width, int height);
    int connect(int timeout_ms);
    int stop();
    int sendMsgToEp(pcie_user_cmd_st cmd_msg);
    int sendStreamToEp(buffer_handle_t srcHandle);
    void rescan_device();
    void markEpNeedRestart(int cmd);
    bool isEpExit();
    bool isMsgNotRecv();

  private:
    void sendStreamThread();

  private:
    common::TvInputBufferManager* mBuffMgr = nullptr;
    int mDebugLevel = 0;
    struct rc_context_st rc_ctx;
    struct pcie_user_cmd_st *mShareUserCmd = nullptr;
    bool mDevInited = false;
    bool mEpConnected = false;
    size_t mBufferSize = 0;
    Mutex mPcieLock;
    sp<SendStreamThread> mSendStreamThread = nullptr;
    std::vector<pcie_buff_node*> mBufSendList;
    bool mIsTasking = false;
    bool mStop = false;
    bool mStopSendStream = false;
    bool mUsedRgaCopy = true;
};

} // namespace android
#endif

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

#include <dlfcn.h>
#include "TvPcieRc.h"
#include "common/RgaCropScale.h"

using ::android::tvinput::RgaCropScale;

namespace android {

//=======================================rc=====================================
typedef int (*rk_pcie_get_pci_bus_addresses_func)(int vendor_id, int device_id, struct pcie_dev_bus *pci_bus);
typedef RK_PCIE_HANDLES (*rk_pcie_device_init_func)(struct pcie_dev_attr_st *dev);
typedef int (*rk_pcie_device_deinit_func)(RK_PCIE_HANDLES handles, int en_remove);
typedef int (*rk_pcie_task_create_func)(RK_PCIE_HANDLES handles, struct pcie_task_attr_st *task);
typedef int (*rk_pcie_task_destroy_func)(RK_PCIE_HANDLES handles, int task_id);
typedef int (*rk_pcie_get_ep_info_func)(RK_PCIE_HANDLES handles, void *status, size_t status_size);
typedef void* (*rk_pcie_get_user_cmd_func)(RK_PCIE_HANDLES handles);
typedef pcie_buff_node* (*rk_pcie_get_buff_func)(RK_PCIE_HANDLES handles, int task_id, enum EBuff_Type type);
typedef int (*rk_pcie_send_buff_func)(RK_PCIE_HANDLES handles, int task_id, struct pcie_buff_node *pbuff);
typedef int (*rk_pcie_release_buff_func)(RK_PCIE_HANDLES handles, int task_id, struct pcie_buff_node *pbuff);
typedef size_t (*rk_pcie_get_buff_max_size_func)(RK_PCIE_HANDLES handles, int task_id, enum EBuff_Type type);
typedef int (*rk_pcie_get_ep_mode_func)(RK_PCIE_HANDLES handles, struct pcie_dev_mode *devmode);
typedef void (*rk_pcie_rescan_devices_func)(void);
typedef int (*rk_pcie_task_msg_func)(RK_PCIE_HANDLES handles, int task_id, void *data, size_t size);

struct PcieRc_ops {
    int (*rk_pcie_get_pci_bus_addresses)(int vendor_id, int device_id, struct pcie_dev_bus *pci_bus);
    RK_PCIE_HANDLES (*rk_pcie_device_init)(struct pcie_dev_attr_st *dev);
    int (*rk_pcie_device_deinit)(RK_PCIE_HANDLES handles, int en_remove);
    int (*rk_pcie_task_create)(RK_PCIE_HANDLES handles, struct pcie_task_attr_st *task);
    int (*rk_pcie_task_destroy)(RK_PCIE_HANDLES handles, int task_id);
    int (*rk_pcie_get_ep_info)(RK_PCIE_HANDLES handles, void *status, size_t status_size);
    void* (*rk_pcie_get_user_cmd)(RK_PCIE_HANDLES handles);
    pcie_buff_node* (*rk_pcie_get_buff)(RK_PCIE_HANDLES handles, int task_id, enum EBuff_Type type);
    int (*rk_pcie_send_buff)(RK_PCIE_HANDLES handles, int task_id, struct pcie_buff_node *pbuff);
    int (*rk_pcie_release_buff)(RK_PCIE_HANDLES handles, int task_id, struct pcie_buff_node *pbuff);
    size_t (*rk_pcie_get_buff_max_size)(RK_PCIE_HANDLES handles, int task_id, enum EBuff_Type type);
    int (*rk_pcie_get_ep_mode)(RK_PCIE_HANDLES handles, struct pcie_dev_mode *devmode);
    void (*rk_pcie_rescan_devices)(void);
    int (*rk_pcie_task_msg)(RK_PCIE_HANDLES handles, int task_id, void *data, size_t size);
};

#define PCIE_RC_LIB_PATH "/vendor/lib64/lib_rk_pcie_rc.so"
static struct PcieRc_ops g_rc_ops;
static void *g_rc_handle = nullptr;
//==============================================================================

static int mRcDebugLevel = 0;

TvPcieRc::TvPcieRc() {
    DEBUG_PRINT(3, "");
    g_rc_handle = dlopen(PCIE_RC_LIB_PATH, RTLD_NOW);
    if (g_rc_handle == NULL) {
        DEBUG_PRINT(3, "cat not open %s", PCIE_RC_LIB_PATH);
    } else {
        g_rc_ops.rk_pcie_get_pci_bus_addresses = (rk_pcie_get_pci_bus_addresses_func)dlsym(g_rc_handle, "rk_pcie_get_pci_bus_addresses");
        g_rc_ops.rk_pcie_device_init = (rk_pcie_device_init_func)dlsym(g_rc_handle, "rk_pcie_device_init");
        g_rc_ops.rk_pcie_device_deinit = (rk_pcie_device_deinit_func)dlsym(g_rc_handle, "rk_pcie_device_deinit");
        g_rc_ops.rk_pcie_task_create = (rk_pcie_task_create_func)dlsym(g_rc_handle, "rk_pcie_task_create");
        g_rc_ops.rk_pcie_task_destroy = (rk_pcie_task_destroy_func)dlsym(g_rc_handle, "rk_pcie_task_destroy");
        g_rc_ops.rk_pcie_get_ep_info = (rk_pcie_get_ep_info_func)dlsym(g_rc_handle, "rk_pcie_get_ep_info");
        g_rc_ops.rk_pcie_get_user_cmd = (rk_pcie_get_user_cmd_func)dlsym(g_rc_handle, "rk_pcie_get_user_cmd");
        g_rc_ops.rk_pcie_get_buff = (rk_pcie_get_buff_func)dlsym(g_rc_handle, "rk_pcie_get_buff");
        g_rc_ops.rk_pcie_send_buff = (rk_pcie_send_buff_func)dlsym(g_rc_handle, "rk_pcie_send_buff");
        g_rc_ops.rk_pcie_release_buff = (rk_pcie_release_buff_func)dlsym(g_rc_handle, "rk_pcie_release_buff");
        g_rc_ops.rk_pcie_get_buff_max_size = (rk_pcie_get_buff_max_size_func)dlsym(g_rc_handle, "rk_pcie_get_buff_max_size");
        g_rc_ops.rk_pcie_get_ep_mode = (rk_pcie_get_ep_mode_func)dlsym(g_rc_handle, "rk_pcie_get_ep_mode");
        g_rc_ops.rk_pcie_rescan_devices = (rk_pcie_rescan_devices_func)dlsym(g_rc_handle, "rk_pcie_rescan_devices");
        g_rc_ops.rk_pcie_task_msg = (rk_pcie_task_msg_func)dlsym(g_rc_handle, "rk_pcie_task_msg");
    }

    mBuffMgr = common::TvInputBufferManager::GetInstance();
}

TvPcieRc::~TvPcieRc() {
    DEBUG_PRINT(3, "");
}

void TvPcieRc::sendStreamThread() {
    DEBUG_PRINT(3, "start connected ep=%d, stop=%d, stopSendStream=%d",
        mEpConnected, mStop, mStopSendStream);
    while (mEpConnected && !mStop && !mStopSendStream) {
        {
            if (!mBufSendList.empty()) {
                mIsTasking = true;
                int task_id = rc_ctx.task_id;
                DEBUG_PRINT(mDebugLevel, "dev task_id=%d, start rk_pcie_send_buff", task_id);
                g_rc_ops.rk_pcie_send_buff(rc_ctx.rk_handle, task_id, mBufSendList[0]);
                DEBUG_PRINT(mDebugLevel, "dev task_id=%d, end rk_pcie_send_buff", task_id);
                mBufSendList.erase(mBufSendList.begin());
                mIsTasking = false;
            }
        }
        usleep(500);
    }
    DEBUG_PRINT(3, "end connected ep=%d, stop=%d, stopSendStream=%d",
        mEpConnected, mStop, mStopSendStream);
}

int TvPcieRc::sendStreamToEp(buffer_handle_t srcHandle) {
    if (g_rc_handle == NULL || !mEpConnected || mStop || mStopSendStream || srcHandle == NULL) {
        DEBUG_PRINT(3, "failed with g_rc_handle NULL or ep=%d, stop=%d, stopSendStream=%d",
            mEpConnected, mStop, mStopSendStream);
        return -1;
    }
    Mutex::Autolock autoLock(mPcieLock);
    if (!mEpConnected || mStop || mStopSendStream) {
        return -1;
    }
    int task_id = rc_ctx.task_id;
    if (mBufferSize == 0) {
        mBufferSize = g_rc_ops.rk_pcie_get_buff_max_size(rc_ctx.rk_handle, task_id, E_SEND);
        DEBUG_PRINT(3, "task_id=%d, buffer_size=%zu", task_id, mBufferSize);
    }
    size_t buffer_size = mBufferSize;
    struct pcie_buff_node* node = g_rc_ops.rk_pcie_get_buff(rc_ctx.rk_handle, task_id, E_SEND);
    if (node) {
        DEBUG_PRINT(mDebugLevel, "rk_pcie_get_buff success");
        node->size = buffer_size;
        // fill stream data
        int srcWidth = mBuffMgr->GetWidth(srcHandle);
        int srcHeight = mBuffMgr->GetHeight(srcHandle);
        if (mUsedRgaCopy) {
            DEBUG_PRINT(mDebugLevel, "rga_copy start %dx%d, node size=%zu",
                srcWidth, srcHeight, node->size);
            RgaCropScale::rga_copy(srcHandle->data[0], nullptr, -1,
                srcWidth, srcHeight, HAL_PIXEL_FORMAT_YCbCr_422_SP,
                -1, nullptr, node->phy_addr);
            DEBUG_PRINT(mDebugLevel, "rga_copy end %dx%d", srcWidth, srcHeight);
        } else {
            char* tmpSrcPtr = NULL;
            int lockMode = GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_HW_CAMERA_MASK;
            mBuffMgr->LockLocked(srcHandle, lockMode, 0, 0, srcWidth, srcHeight, (void**)&tmpSrcPtr);
            size_t srcDatasize = 0;
            for (int i = 0; i < mBuffMgr->GetNumPlanes(srcHandle); i++) {
                srcDatasize += mBuffMgr->GetPlaneSize(srcHandle, i);
            }
            size_t size = srcDatasize < node->size ? srcDatasize : node->size;
            DEBUG_PRINT(mDebugLevel, "memcpy start srcDatasize=%zu, node size=%zu", srcDatasize, node->size);
            std::memcpy(node->vir_addr, tmpSrcPtr, size);
            DEBUG_PRINT(mDebugLevel, "memcpy end srcDatasize=%zu, node size=%zu", srcDatasize, node->size);
            mBuffMgr->UnlockLocked(srcHandle);
        }
        mBufSendList.push_back(node);
    }
    return 0;
}

int TvPcieRc::sendMsgToEp(pcie_user_cmd_st cmd_msg) {
    DEBUG_PRINT(3, "cmd=%d", cmd_msg.cmd);
    Mutex::Autolock autoLock(mPcieLock);
    int ret = -1;
    if (g_rc_handle == NULL || !mEpConnected || mStop) {
        DEBUG_PRINT(3, "failed with g_rc_handle NULL or connectedEpIndex=%d, stop=%d",
            mEpConnected, mStop);
        return ret;
    }
    rc_ctx.user_cmd.cmd = cmd_msg.cmd;
    rc_ctx.user_cmd.inDevConnected = cmd_msg.inDevConnected;
    rc_ctx.user_cmd.frameWidth = cmd_msg.frameWidth;
    rc_ctx.user_cmd.frameHeight = cmd_msg.frameHeight;
    rc_ctx.user_cmd.framePixelFormat = cmd_msg.framePixelFormat;
    rc_ctx.user_cmd.ep_exit = 0;
    int task_id =  rc_ctx.task_id;
    long msgTimestamp = (long)(systemTime()/1000000);
    DEBUG_PRINT(3, "rk_pcie_task_msg taskId=%d, cmd=%d, timestamp=%ld", task_id, cmd_msg.cmd, msgTimestamp);
    if (mShareUserCmd) {
        mShareUserCmd->rcMsgTimestamp = msgTimestamp;
    }
    ret = g_rc_ops.rk_pcie_task_msg(rc_ctx.rk_handle, task_id,
        (void *)&rc_ctx.user_cmd, sizeof(struct pcie_user_cmd_st));
    return ret;
}

int TvPcieRc::init(int width, int height) {
    int ret = -1;
    if (g_rc_handle == NULL) {
        DEBUG_PRINT(3, " failed with g_rc_handle NULL");
        return ret;
    }

    Mutex::Autolock autoLock(mPcieLock);
    DEBUG_PRINT(3, "%dx%d", width, height);
    memset(&rc_ctx, 0, sizeof(rc_ctx));
    ret = g_rc_ops.rk_pcie_get_pci_bus_addresses(PCIE_VENDOR_ID, PCIE_DEVICE_ID, &rc_ctx.dev_bus);
    if (ret != 0) {
        DEBUG_PRINT(3, "get pci bus address fail");
        return ret;
    }
    DEBUG_PRINT(3, "ep dev max num : %d", rc_ctx.dev_bus.dev_max_num);

    int dev_max_num = rc_ctx.dev_bus.dev_max_num;
    if (dev_max_num == 0) {
        DEBUG_PRINT(3, "no ep device, exit ...");
        return -1;
    }
    int multiple = 2;//nv16
    rc_ctx.buff_size = width*height*multiple;

    rc_ctx.dev_attr.using_rc_dma = 1;
    rc_ctx.dev_attr.rc_dma_chn = 1;
    rc_ctx.dev_attr.vendor_id = PCIE_VENDOR_ID;
    rc_ctx.dev_attr.device_id = PCIE_DEVICE_ID;
    rc_ctx.dev_attr.enable_speed = 0;
    memcpy(rc_ctx.dev_attr.pci_bus_address, rc_ctx.dev_bus.pci_bus_address, strlen(rc_ctx.dev_bus.pci_bus_address[0]));
    rc_ctx.rk_handle = g_rc_ops.rk_pcie_device_init(&rc_ctx.dev_attr);
    if (rc_ctx.rk_handle == NULL) {
        DEBUG_PRINT(3, "device init fail,bus:%s\n", rc_ctx.dev_attr.pci_bus_address);
        return -1;
    }
    mShareUserCmd = (struct pcie_user_cmd_st *)g_rc_ops.rk_pcie_get_user_cmd(rc_ctx.rk_handle);
    if (mShareUserCmd) {
        DEBUG_PRINT(3, "set share user cmd: rc_exit 0");
        mShareUserCmd->rc_exit = 0;
    }

    //g_rc_ops.rk_pcie_get_ep_mode(rc_ctx.rk_handle, &rc_ctx.devmode);
    //DEBUG_PRINT(3, "ep device init, dev mode: %d, submode: %d\n", rc_ctx.devmode.mode, rc_ctx.devmode.submode);
    mDevInited = true;
    mStop = false;
    return ret;
}

int TvPcieRc::connect(int timeout_ms) {
    Mutex::Autolock autoLock(mPcieLock);
    int ret = -1;
    if (g_rc_handle == NULL || !mDevInited) {
        DEBUG_PRINT(3, "failed with g_rc_handle NULL or deviceInit=%d", mDevInited);
        return ret;
    }

    DEBUG_PRINT(3, "timeout_ms=%d", timeout_ms);
    rc_ctx.task_attr.send_buff_num = PCIE_RECV_SEND_BUF_NUM;
    rc_ctx.task_attr.send_buff_size = rc_ctx.buff_size;
    rc_ctx.task_attr.recv_buff_num = PCIE_RECV_SEND_BUF_NUM;
    rc_ctx.task_attr.recv_buff_size = rc_ctx.buff_size;
    rc_ctx.task_attr.wait_timeout_ms = timeout_ms;

    rc_ctx.task_id = g_rc_ops.rk_pcie_task_create(rc_ctx.rk_handle, &rc_ctx.task_attr);
    DEBUG_PRINT(3, "pcie init create task task_id=%d", rc_ctx.task_id);
    if (rc_ctx.task_id < 0) {
        return rc_ctx.task_id;
    }
    mEpConnected = true;

    g_rc_ops.rk_pcie_get_ep_mode(rc_ctx.rk_handle, &rc_ctx.devmode);
    DEBUG_PRINT(3, "ep ap init, dev mode: %d, submode: %d\n", rc_ctx.devmode.mode, rc_ctx.devmode.submode);
    if (mSendStreamThread) {
        DEBUG_PRINT(3, "========SendStreamThread not null !!!==========");
    }
    mBufSendList.clear();
    mStopSendStream = false;
    mSendStreamThread = new SendStreamThread(this);
    return 0;
}

int TvPcieRc::stop() {
    DEBUG_PRINT(3, "start epConnect=%d, devInit=%d", mEpConnected, mDevInited);
    int ret = 0;
    mStop = true;
    mStopSendStream = true;
    rc_ctx.user_cmd.cmd = PCIE_CMD_HDMIIN_NON;
    if (mShareUserCmd) {
        DEBUG_PRINT(3, "set share user cmd: rc_exit 1");
        mShareUserCmd->rc_exit = 1;
    }

    if (mSendStreamThread) {
        DEBUG_PRINT(3, "SendStreamThread start exit");
        mSendStreamThread->requestExit();
        mSendStreamThread.clear();
        mSendStreamThread = nullptr;
        DEBUG_PRINT(3, "SendStreamThread end exit");
    }

    Mutex::Autolock autoLock(mPcieLock);
    DEBUG_PRINT(3, "enter lock do destory");
    int tryNum = 10;//1s
    while (mIsTasking && tryNum > 0) {
        tryNum--;
        usleep(100000);
        DEBUG_PRINT(3, "wait tasking finish, tryNum=%d, isTasking=%d", tryNum, mIsTasking);
    }
    if (mEpConnected) {
        mEpConnected = false;
        DEBUG_PRINT(3, "do pcie destory task_id=%d", rc_ctx.task_id);
        ret = g_rc_ops.rk_pcie_task_destroy(rc_ctx.rk_handle, rc_ctx.task_id);
        DEBUG_PRINT(3, "pcie destory task ret=%d", ret);
    }
    if (mDevInited) {
        mDevInited = false;
        DEBUG_PRINT(3, "do rk_pcie_device_deinit");
        ret = g_rc_ops.rk_pcie_device_deinit(rc_ctx.rk_handle, 0);
        DEBUG_PRINT(3, "rk_pcie_device_deinit ret = %d", ret);
    }

    DEBUG_PRINT(3, "end");
    return 0;
}

void TvPcieRc::rescan_device() {
    if (g_rc_handle == NULL) {
        DEBUG_PRINT(3, "failed with g_rc_handle NULL");
        return;
    }
    DEBUG_PRINT(3, "start");
    g_rc_ops.rk_pcie_rescan_devices();
    DEBUG_PRINT(3, "end");
}

void TvPcieRc::markEpNeedRestart(int cmd) {
    mStopSendStream = true;
    Mutex::Autolock autoLock(mPcieLock);
    if (!mDevInited || mStop) {
        DEBUG_PRINT(3, "force retrun due to device init=%d, stop=%d",
            mDevInited, mStop);
        return;
    }
    if (mShareUserCmd) {
        DEBUG_PRINT(3, "cmd=%d", cmd);
        mShareUserCmd->ep_need_restart = cmd;
    }
}

bool TvPcieRc::isEpExit() {
    Mutex::Autolock autoLock(mPcieLock);
    if (!mDevInited || mStop) {
        DEBUG_PRINT(3, "force retrun false due to device init=%d, stop=%d",
            mDevInited, mStop);
        return false;
    }
    if (mShareUserCmd) {
        return mShareUserCmd->ep_exit == 1;
    }
    return false;
}

bool TvPcieRc::isMsgNotRecv() {
    Mutex::Autolock autoLock(mPcieLock);
    if (!mDevInited || mStop) {
        DEBUG_PRINT(3, "force retrun false due to device init=%d, stop=%d",
            mDevInited, mStop);
        return false;
    }
    if (mShareUserCmd) {
        long timestamp = mShareUserCmd->rcMsgTimestamp;
        if (timestamp != 0) {
            return true;
        }
    }
    return false;
}

void TvPcieRc::setDebugLevel(int debugLevel) {
    //debugLevel = 3;
    if (mDebugLevel != debugLevel) {
        mDebugLevel = debugLevel;
        mRcDebugLevel = debugLevel;
    }
}

}

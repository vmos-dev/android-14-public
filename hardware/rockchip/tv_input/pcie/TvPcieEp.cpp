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
#include "TvPcieEp.h"
#include "common/RgaCropScale.h"

using ::android::tvinput::RgaCropScale;

namespace android {

//=======================================ep=====================================
typedef int (*rk_pcie_device_init_func)(void);
typedef int (*rk_pcie_device_deinit_func)(void);
typedef pcie_task_msg_st* (*rk_pcie_wait_task_msg_func)(int timeout_ms);
typedef void (*rk_pcie_task_msg_release_func)(struct pcie_task_msg_st *msg);
typedef int (*rk_pcie_task_create_func)(int task_id);
typedef int (*rk_pcie_task_destroy_func)(int task_id);
typedef int (*rk_pcie_set_ep_info_func)(void *status, size_t status_size);
typedef void* (*rk_pcie_get_user_cmd_func)(void);
typedef pcie_buff_node* (*rk_pcie_get_buff_func)(int task_id, enum EBuff_Type type);
typedef int (*rk_pcie_send_buff_func)(int task_id, struct pcie_buff_node *pbuff);
typedef int (*rk_pcie_release_buff_func)(int task_id, struct pcie_buff_node *pbuff);
typedef size_t (*rk_pcie_get_buff_max_size_func)(int task_id, enum EBuff_Type type);

struct PcieEp_ops {
    int (*rk_pcie_device_init)(void);
    int (*rk_pcie_device_deinit)(void);
    pcie_task_msg_st* (*rk_pcie_wait_task_msg)(int timeout_ms);
    void (*rk_pcie_task_msg_release)(struct pcie_task_msg_st *msg);
    int (*rk_pcie_task_create)(int task_id);
    int (*rk_pcie_task_destroy)(int task_id);
    int (*rk_pcie_set_ep_info)(void *status, size_t status_size);
    void* (*rk_pcie_get_user_cmd)(void);
    pcie_buff_node* (*rk_pcie_get_buff)(int task_id, enum EBuff_Type type);
    int (*rk_pcie_send_buff)(int task_id, struct pcie_buff_node *pbuff);
    int (*rk_pcie_release_buff)(int task_id, struct pcie_buff_node *pbuff);
    size_t (*rk_pcie_get_buff_max_size)(int task_id, enum EBuff_Type type);
};

#define PCIE_EP_LIB_PATH "/vendor/lib64/lib_rk_pcie_ep.so"
static struct PcieEp_ops g_ep_ops;
static void *g_ep_handle = nullptr;
//==============================================================================

static int mEpDebugLevel = 0;

TvPcieEp::TvPcieEp() {
    DEBUG_PRINT(3, "");
    g_ep_handle = dlopen(PCIE_EP_LIB_PATH, RTLD_NOW);
    if (g_ep_handle == NULL) {
        DEBUG_PRINT(3, "cat not open %s", PCIE_EP_LIB_PATH);
    } else {
        g_ep_ops.rk_pcie_device_init = (rk_pcie_device_init_func)dlsym(g_ep_handle, "rk_pcie_device_init");
        g_ep_ops.rk_pcie_device_deinit = (rk_pcie_device_deinit_func)dlsym(g_ep_handle, "rk_pcie_device_deinit");
        g_ep_ops.rk_pcie_wait_task_msg = (rk_pcie_wait_task_msg_func)dlsym(g_ep_handle, "rk_pcie_wait_task_msg");
        g_ep_ops.rk_pcie_task_msg_release = (rk_pcie_task_msg_release_func)dlsym(g_ep_handle, "rk_pcie_task_msg_release");
        g_ep_ops.rk_pcie_task_create = (rk_pcie_task_create_func)dlsym(g_ep_handle, "rk_pcie_task_create");
        g_ep_ops.rk_pcie_task_destroy = (rk_pcie_task_destroy_func)dlsym(g_ep_handle, "rk_pcie_task_destroy");
        g_ep_ops.rk_pcie_set_ep_info = (rk_pcie_set_ep_info_func)dlsym(g_ep_handle, "rk_pcie_set_ep_info");
        g_ep_ops.rk_pcie_get_user_cmd = (rk_pcie_get_user_cmd_func)dlsym(g_ep_handle, "rk_pcie_get_user_cmd");
        g_ep_ops.rk_pcie_get_buff = (rk_pcie_get_buff_func)dlsym(g_ep_handle, "rk_pcie_get_buff");
        g_ep_ops.rk_pcie_send_buff = (rk_pcie_send_buff_func)dlsym(g_ep_handle, "rk_pcie_send_buff");
        g_ep_ops.rk_pcie_release_buff = (rk_pcie_release_buff_func)dlsym(g_ep_handle, "rk_pcie_release_buff");
        g_ep_ops.rk_pcie_get_buff_max_size = (rk_pcie_get_buff_max_size_func)dlsym(g_ep_handle, "rk_pcie_get_buff_max_size");
    }

    mBuffMgr = common::TvInputBufferManager::GetInstance();
    memset(&mInputInfo, 0, sizeof(mInputInfo));
    mBufPrepareList.clear();
    mBufDoneList.clear();
}

TvPcieEp::~TvPcieEp() {
    DEBUG_PRINT(3, "");
    if (g_ep_handle) {
        dlclose(g_ep_handle);
    }
}

int TvPcieEp::recvStreamThread() {
    int ret = isEpNeedRestart();
    if (ret != PCIE_CMD_HDMIIN_NON) {
        DEBUG_PRINT(3, "ep need restart cmd=%d", ret);
        usleep(100000);
        if (mCallback == NULL) {
            DEBUG_PRINT(3, "mCallback is null");
        } else {
            mCallback(mContext, ret);
        }
        return -1;
    }
    Mutex::Autolock autoLock(mPcieLock);
    if (!mRcConnected || ep_ctx.recv_rc_exit) {
        DEBUG_PRINT(3, "not connect to Rc or recv exit %d %d", mRcConnected, ep_ctx.recv_rc_exit);
        return ret;
    }
    struct pcie_buff_node *node = g_ep_ops.rk_pcie_get_buff(ep_ctx.task_id, E_RECV);
    if (node) {
        if (mBufPrepareList.empty()) {
            DEBUG_PRINT(3, "skip stream frame");
        } else {
            int bufIndex = 0;
            int dstWidth = mBuffMgr->GetWidth(mBufPrepareList[bufIndex]);
            int dstHeight = mBuffMgr->GetHeight(mBufPrepareList[bufIndex]);
            if (mUsedRgaCopy) {
                DEBUG_PRINT(mDebugLevel, "rga_copy start %dx%d, %zu", dstWidth, dstHeight, node->phy_addr);
                RgaCropScale::rga_copy(-1, nullptr, node->phy_addr,
                    dstWidth, dstHeight, HAL_PIXEL_FORMAT_YCbCr_422_SP,
                    mBufPrepareList[bufIndex]->data[0], nullptr, -1);
                DEBUG_PRINT(mDebugLevel, "rga_copy end %dx%d", dstWidth, dstHeight);
            } else {
                char* tmpDstPtr = NULL;
                int lockMode = GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_HW_CAMERA_MASK;
                mBuffMgr->LockLocked(mBufPrepareList[bufIndex], lockMode, 0, 0, dstWidth, dstHeight, (void**)&tmpDstPtr);
                size_t dstDatasize = 0;
                for (int i = 0; i < mBuffMgr->GetNumPlanes(mBufPrepareList[bufIndex]); i++) {
                    dstDatasize += mBuffMgr->GetPlaneSize(mBufPrepareList[bufIndex], i);
                }
                size_t size = dstDatasize < node->size ? dstDatasize : node->size;
                DEBUG_PRINT(mDebugLevel, "memcpy start srcDatasize=%zu, node size=%zu", dstDatasize, node->size);
                std::memcpy(tmpDstPtr, node->vir_addr, size);
                DEBUG_PRINT(mDebugLevel, "memcpy end srcDatasize=%zu, node size=%zu", dstDatasize, node->size);
                mBuffMgr->UnlockLocked(mBufPrepareList[0]);
            }
            {
                Mutex::Autolock autoLock(mBufLock);
                mBufDoneList.push_back(mBufPrepareList[0]);
                mBufPrepareList.erase(mBufPrepareList.begin());
            }
            ret = 0;
        }
        g_ep_ops.rk_pcie_release_buff(ep_ctx.task_id, node);
        DEBUG_PRINT(mDebugLevel, "task_id=%d end deal stream recv", ep_ctx.task_id);
    }
    usleep(1000);
    return NO_ERROR;
}

int TvPcieEp::init(NotifyTvPcieEpCallback callback, void* context) {
    int ret = -1;
    if (g_ep_handle == NULL) {
        DEBUG_PRINT(3, " failed with g_ep_handle NULL");
        return ret;
    }
    Mutex::Autolock autoLock(mPcieLock);
    DEBUG_PRINT(3, "");
    ret = g_ep_ops.rk_pcie_device_init();
    DEBUG_PRINT(3, "pcie init device ret=%d", ret);
    if (ret == 0) {
        mDevInited = true;
        mCallback = callback;
        mContext = context;
        mStop = false;
    }
    return ret;
}

int TvPcieEp::connect(int timeout_ms) {
    if (mRcConnected) {
        DEBUG_PRINT(3, "already connect, direct return success");
        return 0;
    }
    int ret = -1;
    if (g_ep_handle == NULL || !mDevInited) {
        DEBUG_PRINT(3, "failed with g_ep_handle NULL or deviceInit=%d", mDevInited);
        return ret;
    }

    DEBUG_PRINT(3, "timeout_ms=%d", timeout_ms);
    ret = waitRcMsg(timeout_ms, E_TASK_MSG_CREATE, -1);
    if (ret != 0) {
        DEBUG_PRINT(3, "pcie connect to rc fail");
        return ret;
    }
    mRcConnected = true;
    return ret;
}

void TvPcieEp::createWorkThread(int task_id) {
    DEBUG_PRINT(3, "start task_id=%d", task_id);
    memset(&ep_ctx, 0, sizeof(ep_ctx));
    ep_ctx.task_id = task_id;
    ep_ctx.recv_rc_exit = 0;
    if (mRecvStreamThread) {
        DEBUG_PRINT(3, "========recv stream thread not null !!!==========task_id=%d", task_id);
    }
    mRecvStreamThread = new RecvStreamThread(this);
    mShareUserCmd = (struct pcie_user_cmd_st *)g_ep_ops.rk_pcie_get_user_cmd();
    if (mShareUserCmd) {
        DEBUG_PRINT(3, "set share user cmd: ep_exit 0");
        mShareUserCmd->ep_exit = 0;
    }
    ep_ctx.enable = 1;
    DEBUG_PRINT(3, "end task_id=%d", task_id);
}

void TvPcieEp::destoryWorkThread(int task_id) {
    DEBUG_PRINT(3, "start task_id=%d", task_id);
    if (mRecvStreamThread) {
        mRecvStreamThread->requestExit();
        mRecvStreamThread.clear();
        mRecvStreamThread = nullptr;
        DEBUG_PRINT(3, "end exit recv stream thread");
    }
    if (mRcConnected) {
        ep_ctx.recv_rc_exit = 1;
        ep_ctx.enable = 0;
    }
    DEBUG_PRINT(3, "end task_id=%d", task_id);
}

int TvPcieEp::stop() {
    DEBUG_PRINT(3, "start");
    mStop = true;
    if (g_ep_handle == NULL) {
        DEBUG_PRINT(3, "failed with g_ep_handle NULL");
        return 0;
    }
    int task_id = ep_ctx.task_id;
    DEBUG_PRINT(3, "start deviceInit=%d, connectedRc=%d, task_id=%d",
        mDevInited, mRcConnected, task_id);
    if (mShareUserCmd) {
        DEBUG_PRINT(3, "set share user cmd: ep_exit 1");
        mShareUserCmd->ep_exit = 1;
        DEBUG_PRINT(3, "set share user cmd: ep_need_restart PCIE_CMD_HDMIIN_NON");
        mShareUserCmd->ep_need_restart = PCIE_CMD_HDMIIN_NON;
    }

    Mutex::Autolock autoLock(mPcieLock);
    DEBUG_PRINT(3, "enter lock do destory");
    if (mRcConnected) {
        mRcConnected = false;
        destoryWorkThread(task_id);
        g_ep_ops.rk_pcie_task_destroy(task_id);
    }
    if (mDevInited) {
        mDevInited = false;
        g_ep_ops.rk_pcie_device_deinit();
    }
    DEBUG_PRINT(3, "end");
    return 0;
}

bool TvPcieEp::isRcExit() {
    Mutex::Autolock autoLock(mPcieLock);
    if (mRcConnected && mShareUserCmd && !mStop) {
        return mShareUserCmd->rc_exit == 1;
    }
    return false;
}

int TvPcieEp::isEpNeedRestart() {
    Mutex::Autolock autoLock(mPcieLock);
    if (mRcConnected && mShareUserCmd && !mStop) {
        return mShareUserCmd->ep_need_restart;
    }
    return PCIE_CMD_HDMIIN_NON;
}

void TvPcieEp::getInputInfo(int& width, int& height, int& pixelFormat,
        int& inDevConnected) {
    width = mInputInfo.frameWidth;
    height = mInputInfo.frameHeight;
    pixelFormat = mInputInfo.framePixelFormat;
    inDevConnected = mInputInfo.inDevConnected;
}

int TvPcieEp::waitRcMsg(int timeout_ms, int limitMsg, int cmd) {
    Mutex::Autolock autoLock(mPcieLock);
    int ret = -1;
    if (g_ep_handle == NULL || !mDevInited) {
        DEBUG_PRINT(3, "failed with g_ep_handle NULL or deviceInited=%d", mDevInited);
        return -1;
    }

    struct pcie_task_msg_st *msg = g_ep_ops.rk_pcie_wait_task_msg(timeout_ms);
    if (msg == NULL) {
        DEBUG_PRINT(3, "rk_pcie_wait_task_msg failed, timeout_ms=%d", timeout_ms);
        return ret;
    }
    DEBUG_PRINT(3, "msg type=%d, task_id=%d", msg->type, msg->task_id);

    bool limitMsgCompare = false;
    switch (msg->type) {
        case E_TASK_MSG_CREATE:
            ret = g_ep_ops.rk_pcie_task_create(msg->task_id);
            DEBUG_PRINT(3, "rk_pcie_task_create taskid=%d, ret=%d", msg->task_id, ret);
            if (msg->type == limitMsg) {
                limitMsgCompare = true;
            }
            break;
        case E_TASK_MSG_DESTORY:
            ret = g_ep_ops.rk_pcie_task_destroy(msg->task_id);
            DEBUG_PRINT(3, "rk_pcie_task_destroy %d ret=%d", msg->task_id, ret);
            if (msg->type == limitMsg) {
                limitMsgCompare = true;
            }
            break;
        case E_TASK_MSG_USER: {
            struct pcie_user_cmd_st *data = (struct pcie_user_cmd_st *)msg->data;
            DEBUG_PRINT(3, "E_TASK_MSG_USER cmd=%d", data->cmd);
            int task_id = msg->task_id;
            if (data->cmd == PCIE_CMD_HDMIIN_INIT) {
                mInputInfo.frameWidth = data->frameWidth;
                mInputInfo.frameHeight = data->frameHeight;
                mInputInfo.framePixelFormat = data->framePixelFormat;
                mInputInfo.inDevConnected = data->inDevConnected;
                DEBUG_PRINT(3, "E_TASK_MSG_USER %dx%d pixel=%d, inDevConnected=%d", 
                    mInputInfo.frameWidth, mInputInfo.frameHeight, mInputInfo.frameHeight, mInputInfo.inDevConnected);
                createWorkThread(task_id);
                if (mShareUserCmd) {
                    long msgTimestamp = mShareUserCmd->rcMsgTimestamp;
                    if (msgTimestamp != 0) {
                        long currentTimestamp = (long)(systemTime()/1000000);
                        mShareUserCmd->rcMsgTimestamp = 0;
                        DEBUG_PRINT(3, "%ld rev rc msg, set timestamp from %ld to 0", currentTimestamp, msgTimestamp);
                    }
                }
            }
            if (msg->type == limitMsg && cmd == data->cmd) {
                limitMsgCompare = true;
            }
            ret = 0;
            break;
        }
        default:
            DEBUG_PRINT(3, "error msg type!!! %d", msg->type);
            break;
    }
    g_ep_ops.rk_pcie_task_msg_release(msg);
    return limitMsgCompare && ret == 0 ? 0 : -1;
}

void TvPcieEp::qBuf(buffer_handle_t handle) {
    Mutex::Autolock autoLock(mBufLock);
    mBufPrepareList.push_back(handle);
}

int TvPcieEp::dqBufFd() {
    Mutex::Autolock autoLock(mBufLock);
    int ret = -1;
    if (!mBufDoneList.empty()) {
        ret = mBufDoneList[0]->data[0];
        mBufDoneList.erase(mBufDoneList.begin());
        DEBUG_PRINT(mDebugLevel, "success fd=%d", ret);
        return ret;
    }
    return ret;
}

void TvPcieEp::setDebugLevel(int debugLevel) {
    //debugLevel = 3;
    if (mDebugLevel != debugLevel) {
        mDebugLevel = debugLevel;
        mEpDebugLevel = debugLevel;
    }
}

}

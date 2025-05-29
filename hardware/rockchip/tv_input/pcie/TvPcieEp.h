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

#ifndef __TV_PCIE_EP_H__
#define __TV_PCIE_EP_H__

#include <utils/Mutex.h>
#include <utils/threads.h>
#include "common/TvInput_Buffer_Manager_gralloc4_impl.h"
#include <common/Utils.h>
#include "TvPcie.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "tv_input_pcie_ep"
#endif

typedef void (*NotifyTvPcieEpCallback)(void* context, int cmd);

struct ep_context_st {
    int task_id;
    char enable;
    char recv_rc_exit;
};

namespace android {

class TvPcieEp : public RefBase {
  public:
    TvPcieEp();
    virtual ~TvPcieEp();

  private:
    class RecvStreamThread : public Thread {
        TvPcieEp* mSource;
        public:
            RecvStreamThread(TvPcieEp* source) :
                Thread(false), mSource(source) { }
            virtual void onFirstRef() {
                run("tif pcie_ep thread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                return mSource->recvStreamThread() == NO_ERROR;
            }
    };

  public:
    void setDebugLevel(int debugLevel);
    int init(NotifyTvPcieEpCallback callback, void* context);
    int connect(int timeout_ms);
    int stop();
    int waitRcMsg(int timeout_ms, int limitMsg, int cmd);
    bool isRcExit();
    void getInputInfo(int& width, int& height, int& pixelFormat,
        int& inDevConnected);
    void qBuf(buffer_handle_t handle);
    int dqBufFd();

  private:
    void createWorkThread(int task_id);
    void destoryWorkThread(int task_id);
    int recvStreamThread();
    int isEpNeedRestart();

  private:
    common::TvInputBufferManager* mBuffMgr = nullptr;
    int mDebugLevel = 0;
    ep_context_st ep_ctx;
    struct pcie_user_cmd_st *mShareUserCmd = nullptr;
    bool mDevInited = false;
    bool mRcConnected = false;
    pcie_user_cmd_st mInputInfo;
    void* mContext = nullptr;
    NotifyTvPcieEpCallback mCallback = NULL;
    Mutex mPcieLock;
    Mutex mBufLock;
    sp<RecvStreamThread> mRecvStreamThread = nullptr;
    std::vector<buffer_handle_t> mBufPrepareList;
    std::vector<buffer_handle_t> mBufDoneList;
    bool mStop = false;
    bool mUsedRgaCopy = true;
};

} // namespace android
#endif

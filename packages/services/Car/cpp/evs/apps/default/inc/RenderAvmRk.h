/*
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 * Authors:
 *  kenvenis.chen <kenvenis.chen@rock-chips.com>
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

#ifndef CAR_EVS_APP_RENDERAVMRK_H
#define CAR_EVS_APP_RENDERAVMRK_H

#include <utils/Log.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <queue>
#include "RenderBase.h"
#include "Semaphore.h"
#include "Utils.h"

#include <aidl/android/hardware/automotive/evs/IEvsEnumerator.h>
#include "ConfigManager.h"
#include "VideoTex.h"
#include <math/mat4.h>
#include "rk_avm_api.h"

#if RKAVM
namespace android
{
using aidl::android::hardware::automotive::evs::BufferDesc;

class AvmAlgoProcessAdapter//:public CameraHal_Tracer
{
    class ProcessThread : public Thread
    {
        AvmAlgoProcessAdapter *mProcessAdapter;
    public:
        ProcessThread(AvmAlgoProcessAdapter *proadap)
            : Thread(false), mProcessAdapter(proadap) {}

        virtual bool threadLoop()
        {
            mProcessAdapter->processThread();

            return false;
        }
    };

public:
    enum ProcessThreadCommands
    {
        // Comands
        CMD_PROCESS_PAUSE,
        CMD_PROCESS_START,
        CMD_PROCESS_STOP,
        CMD_PROCESS_FRAME,
        CMD_PROCESS_DISPLAY,
        CMD_PROCESS_SHARE_FD,
        CMD_PROCESS_INVAL
    };

    enum PROCESS_STATUS
    {
        STA_PROCESS_RUNNING,
        STA_PROCESS_PAUSE,
        STA_PROCESS_STOP,
    };
    enum ProcessCommandStatus
    {
        CMD_PROCESS_START_PREPARE = 1,
        CMD_PROCESS_START_DONE,
        CMD_PROCESS_PAUSE_PREPARE,
        CMD_PROCESS_PAUSE_DONE,
        CMD_PROCESS_STOP_PREPARE,
        CMD_PROCESS_STOP_DONE,
        CMD_PROCESS_SET_DISP_PREPARE,
        CMD_PROCESS_SET_DISP_DONE,
        CMD_PROCESS_FRAME_PREPARE,
        CMD_PROCESS_FRAME_PROCESSING,
        CMD_PROCESS_FRAME_DONE,
        CMD_PROCESS_FRAME_PROCESSING_DONE,
        CMD_PROCESS_FRAME_SHARE_FD_PREPARE,
        CMD_PROCESS_FRAME_SHARE_FD_DONE,
    };

    bool isNeedSendToProcess();
    void notifyNewFrame();

    int startProcess();
    int stopProcess();
    int pauseProcess();
    int setDisplay(int sw, int sh);

    int getProcessStatus(void);
    int getProcessState();
    bool getDispInitState()
    {
        return disp_init;
    }
    void setSurround3dType(int type);
    void setInputBuffer(BufferDesc& InBuffer, int id)
    {
        myBuffer[id] = dupBufferDesc(InBuffer);
    }
    void setInputBuffer(android::sp<android::GraphicBuffer> &this_buffer, int id)
    {
        myActivatedCamBuffer[id] = this_buffer;
    }

    void relInputBuffer()
    {
        for(int id = 0; id <(int) myActivatedCamBuffer.size(); id++)
            myActivatedCamBuffer[id] = nullptr;
        myActivatedCamBuffer.clear();
    }
    AvmAlgoProcessAdapter();
    ~AvmAlgoProcessAdapter();

private:
    void processThread();
    void setBufferState(int index, int status);
    void setProcessState(int state);
    void initSurround3d();
    void deinitSurround3d();

    bool mReadyInit = false;
    bool mBufRdyCnt= false;

    std::queue<enum ProcessThreadCommands> mPThreadCmdsQueue;
    Semaphore mPCmdsem;
    Semaphore mStateComplete;
    int mProcessRuning;

    Mutex mProcessLock;
    Condition mProcessCond;
    int mProcessState;
    int tmpFd;
    int dw, dh;
    bool disp_init = false;
    char dump_type[PROPERTY_VALUE_MAX];
    sp<ProcessThread> mProcessThread;
    std::vector<BufferDesc> myBuffer;
    std::map<int, android::sp<android::GraphicBuffer>> myActivatedCamBuffer;
};
}
/*
 * Combines the views from all available cameras into one reprojected top down view.
 */
class RenderAvmRk final : public RenderBase
{
public:
    RenderAvmRk(std::shared_ptr<aidl::android::hardware::automotive::evs::IEvsEnumerator> enumerator,
                const std::vector<ConfigManager::CameraInfo>& camList,
                const ConfigManager &config);

    virtual bool activate() override;
    virtual void deactivate() override;

    virtual bool drawFrame(const BufferDesc &tgtBuffer);
    void AVMSetSurroundType(enum RK_AVM_CMD cmd);
    static android::AvmAlgoProcessAdapter *mProcess;

protected:
    struct ActiveCamera
    {
        const ConfigManager::CameraInfo    &info;
        std::unique_ptr<VideoTex>           tex;

        ActiveCamera(const ConfigManager::CameraInfo &c) : info(c) {};
    };

    bool AVMCreateImageKHR(int TexWidth, int TexHeight, int kfd, int texid, EGLImageKHR *Img);

    bool AVMCreateEGLImage(buffer_handle_t handle, int w, int h, int texid, EGLImageKHR *Img);

    bool AVMRenderAdjust();
    bool AVMRenderTopAdjust();
    bool AVMRenderTopFishEye(GLuint texId);

    std::shared_ptr<aidl::android::hardware::automotive::evs::IEvsEnumerator> mEnumerator;
    const ConfigManager            &mConfig;
    std::vector<ActiveCamera>       mActiveCameras;
    EGLImageKHR myImg = EGL_NO_IMAGE_KHR;//no use activecamera tex imgKHR for getting RKAVM render out shared texture
    EGLImageKHR myTopImg = EGL_NO_IMAGE_KHR;
    EGLImageKHR myRevImg = EGL_NO_IMAGE_KHR;
    GLuint myTexId;
    GLuint myAdjId;
    GLuint myProgramHandle;
    int32_t myFd, afd;
};
#endif
#endif //CAR_EVS_APP_RENDERAVMRK_H

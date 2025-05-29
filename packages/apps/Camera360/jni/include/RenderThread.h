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
#include <android/hardware_buffer.h>
#include <queue>
#include "RSemaphore.h"
#include "rk_avm_api.h"


namespace android
{
class AvmProcessAdapter
{
    class ProcessThread : public Thread
    {
		AvmProcessAdapter *mProcessAdapter;	
    public:
        ProcessThread(AvmProcessAdapter *proadap)
            : Thread(false), mProcessAdapter(proadap) {}

        virtual bool threadLoop()
        {
            mProcessAdapter->processThread();

            return false;
        }
    };

public:

    typedef struct InputBuffer_desc
	{
		AHardwareBuffer *buf;
		AHardwareBuffer_Desc desc;
		uint32_t camId;
	}AvmInputBuffer;
	
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
	void notifyNewFrame(const std::vector<AHardwareBuffer*>& InputCam);

    int startProcess();
    int stopProcess();
    int pauseProcess();

    int getProcessStatus(void);
    int getProcessState();
	int setDisplay(int sw, int sh);
	void setDisplayAddr(void *addr, int dw, int dh)
	{
		if(addr)
		{
			daddr = addr;
		}
		if(!getDispInitState())
		{
			setDisplay(dw, dh);
		}
	}
	bool getDispInitState()
    {
        return disp_init;
    }
	
	void AvmPreSplitData(int type,void *addr, int w, int h, int camId);
    void setSurround3dType(int type);
    AvmProcessAdapter();
    ~AvmProcessAdapter();

private:
    void processThread();
    void setBufferState(int index, int status);
    void setProcessState(int state);
    void initSurround3d();
    void deinitSurround3d();

    bool mReadyInit = false;

    std::queue<enum ProcessThreadCommands> mPThreadCmdsQueue;
	std::vector<AHardwareBuffer*> mActivatedCam;
    RSemaphore mPCmdsem;
    RSemaphore mStateComplete;
    int mProcessRuning;

    Mutex mProcessLock;
    Condition mProcessCond;
    int mProcessState;
	int myType = (int)rk_cmd_render_front_top_fish_eye;
    int tmpFd;
	void *daddr = nullptr;
	int bit_dw, bit_dh;
	bool disp_init = false;
	char dump_type[PROPERTY_VALUE_MAX];
    sp<ProcessThread> mProcessThread;
};
}								

#endif //CAR_EVS_APP_RENDERAVMRK_H

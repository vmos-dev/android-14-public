/*
 * Copyright (C) 2016 The Android Open Source Project
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
#include "AsyncWorker.h"
#include "sw-sync/sync/sync.h"
#include "sw-sync/sw_sync.h"
#include <unistd.h>
#include <sys/time.h>

#define HAL_PRIORITY_URGENT_DISPLAY (-8)
/*-------------------------------------------
                  Functions
-------------------------------------------*/
static inline int64_t getCurrentTimeUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

AsyncWorker::AsyncWorker() : Worker("NpuWorker", HAL_PRIORITY_URGENT_DISPLAY)
{
    InitWorker();
}

AsyncWorker::~AsyncWorker() { Exit(); }

void AsyncWorker::Queue(int relaseFence)
{
    Lock();
    mReleaseFenceQueue.push(relaseFence);
    Unlock();
    Signal();
    return;
}

int AsyncWorker::WaitFinish()
{
    while (true)
    {
        Lock();
        if (mReleaseFenceQueue.empty() && mRunning == false)
        {
            Unlock();
            break;
        }
        Unlock();
        usleep(50 * 1000);
    }
    return 0;
}

void AsyncWorker::Routine()
{
    // ATRACE_CALL();
    Lock();

    if (mReleaseFenceQueue.empty())
    {
        int ret = WaitForSignalOrExitLocked();
        if (ret)
        {
            if (ret != -EINTR)
            {
                fprintf(stderr,
                        "Failed to wait WaitForSignalOrExitLocked ret=%d\n",
                        ret);
            }
            Unlock();
            return;
        }
    }

    int releaseFence = -1;
    if (!mReleaseFenceQueue.empty())
    {
        mRunning     = true;
        releaseFence = mReleaseFenceQueue.front();
        mReleaseFenceQueue.pop();
    }
    else
    {
        Unlock();
        return;
    }
    Unlock();

    mCnt++;
    int64_t start_us = getCurrentTimeUs();
    if (releaseFence > 0)
    {
        int ret = sync_wait(releaseFence, 1500);
        if (ret)
        {
            fprintf(stderr, "Failed to wait releaseFence %d/%d 1500ms \n",
                    releaseFence, ret);
        }
        close(releaseFence);
        releaseFence = -1;
    }
    int64_t elapse_us = getCurrentTimeUs() - start_us;
    fprintf(stderr,
            "Svep Async-Thread(1) Wait success: Time = %.2fms, FPS = %.2f, "
            "cnt=%d\n",
            elapse_us / 1000.f, 1000.f * 1000.f / elapse_us, mCnt);

    Lock();
    mRunning = false;
    Unlock();
    return;
}

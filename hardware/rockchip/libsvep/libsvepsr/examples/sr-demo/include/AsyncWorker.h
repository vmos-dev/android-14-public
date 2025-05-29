
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
#pragma once

#include "worker.h"

#include <queue>
#include <memory>

class AsyncWorker : public Worker
{
public:
    AsyncWorker();
    ~AsyncWorker() override;

    void Queue(int relaseFence);
    int WaitFinish();

protected:
    void Routine() override;

private:
    std::queue<int> mReleaseFenceQueue;
    int mCnt = 0;
    bool mRunning;
};
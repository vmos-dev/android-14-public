/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <errno.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>
/*****************************************************************************/

struct input_event;

class InputEventCircularReader {
    struct input_event* const mBuffer;
    struct input_event* const mBufferEnd;
    struct input_event* mHead;
    struct input_event* mCurr;
    ssize_t mTotalSpace;

   public:
    InputEventCircularReader();
    InputEventCircularReader(size_t numEvents);
    ~InputEventCircularReader();
    ssize_t fill(int fd);
    ssize_t readEvent(input_event const** events);
    void next();

   private:
    ssize_t getFreeSpace();
    ssize_t getDirtySpace();
};

/*****************************************************************************/

/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "sensors-impl/InputEventReader.h"

#include <errno.h>
#include <linux/input.h>
#include <log/log.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <unistd.h>

#define DBG_LOG(...)	\
    do {		\
        if (Debug_en)	\
	    ALOGI(__VA_ARGS__);	\
    } while (0);

int Debug_en = 0;

/*****************************************************************************/

struct input_event;

InputEventCircularReader::InputEventCircularReader(size_t numEvents)
    : mBuffer(new input_event[numEvents]),
      mBufferEnd(mBuffer + numEvents),
      mHead(mBuffer),
      mCurr(mBuffer),
      mTotalSpace(numEvents) {}

InputEventCircularReader::~InputEventCircularReader() { delete[] mBuffer; }

ssize_t InputEventCircularReader::getFreeSpace() {
    if (mCurr > mHead) return mCurr - mHead;
    if (mHead > mCurr) return (mTotalSpace + mHead - mCurr);
    return mTotalSpace;
}

ssize_t InputEventCircularReader::getDirtySpace() {
    if (mCurr > mHead) return mTotalSpace + mCurr - mHead;
    if (mHead > mCurr) return mHead - mCurr;
    return 0;
}

ssize_t InputEventCircularReader::fill(int fd) {
    size_t numEventsRead = 0;
    size_t freeSpace = 0;

    // if buffer has available events, dont fill data.
    if (mCurr != mHead) return 0;

    freeSpace = getFreeSpace();

    if (freeSpace > 0) {
        if (mHead + freeSpace >= mBufferEnd) {
            freeSpace = mBufferEnd - mHead;
            if (mCurr == mBuffer) freeSpace -= 1;
        }
        const ssize_t nread = read(fd, mHead, freeSpace * sizeof(input_event));
        if (nread <= 0 || nread % sizeof(input_event)) {
            // we got a partial event!!
            DBG_LOG("sensor InputEventCircular read data frome fd fail");
            return nread < 0 ? -errno : -EINVAL;
        }

        numEventsRead = nread / sizeof(input_event);
        if (numEventsRead) {
            mHead += numEventsRead;

            if (mHead == mBufferEnd) mHead = mBuffer;
        }
    } else {
        ALOGE("sensor InputEventCircular no free space for data");
    }

    return numEventsRead;
}

ssize_t InputEventCircularReader::readEvent(input_event const** events) {
    *events = mCurr;
    ssize_t available = getDirtySpace();
    if (available) {
        next();
        return 1;
    }
    return 0;
}

void InputEventCircularReader::next() {
    if (mCurr == mHead) return;
    mCurr++;

    if (mCurr >= mBufferEnd) {
        mCurr = mBuffer;
    }
}

/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <algorithm>
#include <inttypes.h>

#include <aidlcommonsupport/NativeHandle.h>
#include <sync/sync.h>
#include <ui/GraphicBufferMapper.h>

#include "CachedStreamBuffer.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using base::unique_fd;

namespace {
const native_handle_t* importAidlNativeHandle(const NativeHandle& anh) {
    typedef decltype(native_handle_t::version) T;
    std::vector<T> data(sizeof(native_handle_t) / sizeof(T) + anh.fds.size() +
        anh.ints.size());

    native_handle_t* h = native_handle_init(
        reinterpret_cast<char*>(&data[0]), anh.fds.size(), anh.ints.size());
    std::transform(anh.fds.begin(), anh.fds.end(), &h->data[0],
                   [](const ndk::ScopedFileDescriptor& sfd){ return sfd.get(); });
    std::copy(anh.ints.begin(), anh.ints.end(), &h->data[anh.fds.size()]);

    const native_handle_t* importedH = nullptr;
    if (GraphicBufferMapper::get().importBufferNoValidate(h, &importedH) != NO_ERROR) {
        return FAILURE(nullptr);
    }
    ALOGD("%s importBufferNoValidate importedH->data[0] :%d",__FUNCTION__,importedH->data[0] );
    return importedH;
}

int  importAidlNativeHandleFence(const NativeHandle& nh) {
    const size_t nfds = nh.fds.size();
    const size_t nints = nh.ints.size();

    if (nints == 0) {
        switch (nfds) {
        case 0:
            return -1;

        case 1: {
                const int fd = fcntl(nh.fds.front().get(), F_DUPFD_CLOEXEC, 0);
                ALOGD("%s sfd:%d F_DUPFD_CLOEXEC fd:%d",__FUNCTION__,
                nh.fds.size()> 0 ?
                nh.fds[0].get():-1,
                fd);
                if (fd < 0) {
                    return FAILURE_V(unique_fd(), "fcntl failed with %s (%d)",
                                     strerror(errno), errno);
                }

                return fd;
            }

        default:
            return FAILURE_V(unique_fd(), "unexpected fence shape, nfds=%zu, must "
                             "be one", nfds);
        }
    } else {
        return -1;
    }
}

NativeHandle moveFenceToAidlNativeHandle(unique_fd fence) {
    if (!fence.ok()) {
        return {};
    }

    typedef decltype(native_handle_t::version) T;
    T on_stack[sizeof(native_handle_t) / sizeof(T) + 1];

    native_handle_t* nh = native_handle_init(
        reinterpret_cast<char*>(&on_stack[0]), 1, 0);
    nh->data[0] = fence.release();
    ALOGD("%s Acquire fencefd:%d release  to releaseFence nh->data[0]:%d",__FUNCTION__,
    fence.get(),nh->data[0]);
    return makeToAidl(nh);
}
}  //namespace

CachedStreamBuffer::CachedStreamBuffer(const StreamBuffer& sb)
        //: mBuffer(importAidlNativeHandle(sb.buffer))
        :mBufferId(sb.bufferId)
        //, mAcquireFence(importAidlNativeHandleFence(sb.acquireFence))
        , mStreamId(sb.streamId) {
    // ALOGD("%s sb.acquireFence:%d ,mAcquireFence:%d",__FUNCTION__,
    // sb.acquireFence.fds.size()> 0 ?
    // sb.acquireFence.fds[0].get():-1,
    // mAcquireFence);
    //LOG_ALWAYS_FATAL_IF(!mBuffer);
    LOG_ALWAYS_FATAL_IF(!mBufferId);
    LOG_ALWAYS_FATAL_IF(mStreamId < 0);
}

CachedStreamBuffer::CachedStreamBuffer(CachedStreamBuffer&& rhs) noexcept
        //: mBuffer(std::exchange(rhs.mBuffer, nullptr))
        : mBufferId(std::exchange(rhs.mBufferId, 0))
        //, mAcquireFence(std::exchange(rhs.mAcquireFence, {}))
        , mStreamId(std::exchange(rhs.mStreamId, -1))
        , mProcessed(std::exchange(rhs.mProcessed, true)) {
    //LOG_ALWAYS_FATAL_IF(!mBuffer);
    LOG_ALWAYS_FATAL_IF(!mBufferId);
    LOG_ALWAYS_FATAL_IF(mStreamId < 0);
}

CachedStreamBuffer::~CachedStreamBuffer() {
    LOG_ALWAYS_FATAL_IF(!mProcessed);
    //ALOGD("%s mStreamId:%d,mBufferId:%d,mAcquireFence:%d",__FUNCTION__,mStreamId,mBufferId,mAcquireFence);
    if (mStreamId >= 0) {
        //LOG_ALWAYS_FATAL_IF(!mBuffer);
        //LOG_ALWAYS_FATAL_IF(GraphicBufferMapper::get().freeBuffer(mBuffer) != NO_ERROR);
    }
}

void CachedStreamBuffer::importAcquireFence(const NativeHandle& fence) {
    LOG_ALWAYS_FATAL_IF(!mProcessed);
    ALOGD("%s importAidlNativeHandleFence fenceFd:%d",__FUNCTION__,
    fence.fds.size()> 0 ?
    fence.fds[0].get():-1
    );
    mAcquireFence = importAidlNativeHandleFence(fence);
    mProcessed = false;
}

bool CachedStreamBuffer::waitAcquireFence(const unsigned timeoutMs) {
#if 0
    if (mAcquireFence.ok()) {
        if (sync_wait(mAcquireFence.get(), timeoutMs)) {
            return FAILURE(false);
        } else {
            mAcquireFence.reset();
            return true;
        }
    } else {
        return true;
    }
#endif
return true;
}

StreamBuffer CachedStreamBuffer::finish(const bool success) {
    using aidl::android::hardware::camera::device::BufferStatus;
    // LOG_ALWAYS_FATAL_IF(mProcessed);
    LOG_ALWAYS_FATAL_IF(!mBufferId);
    LOG_ALWAYS_FATAL_IF(mStreamId < 0);

    StreamBuffer sb;
    sb.streamId = mStreamId;
    sb.bufferId = mBufferId;
    sb.status = success ? BufferStatus::OK : BufferStatus::ERROR;
    //ALOGD("%s moveFenceToAidlNativeHandle mAcquireFence:%d to releaseFence",__FUNCTION__,mAcquireFence.get());
    //sb.releaseFence = moveFenceToAidlNativeHandle(std::move(mAcquireFence));

    mProcessed = true;
    return sb;
}


}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

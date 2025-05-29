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

#define FAILURE_DEBUG_PREFIX "StreamBufferCache"

#include "StreamBufferCache.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

CachedStreamBuffer*
StreamBufferCache::update(const StreamBuffer& sb) {
    // ALOGD("%s streamId:%d bufferId:%d, buffer fd:%d, acquireFence fd:%d,releaseFence fd:%d",__FUNCTION__,
    // sb.streamId,
    // sb.bufferId,
    // sb.buffer.fds.size() > 0 ?         sb.buffer.fds[0].get(): -1,
    // sb.acquireFence.fds.size() > 0 ?   sb.acquireFence.fds[0].get():-1,
    // sb.releaseFence.fds.size() > 0 ?   sb.releaseFence.fds[0].get():-1
    //  );
    const auto bi = mCache.find(sb.bufferId);
    if (bi == mCache.end()) {
        //ALOGD("%s insert bufferId:%d",__FUNCTION__,sb.bufferId);
        const auto r = mCache.insert({sb.bufferId, CachedStreamBuffer(sb)});
        LOG_ALWAYS_FATAL_IF(!r.second);
        r.first->second.dump();
        return &(r.first->second);

    } else {
        CachedStreamBuffer* csb = &bi->second;
        // ALOGD("%s importAcquireFence bufferId:%d",__FUNCTION__,sb.bufferId);
        // csb->importAcquireFence(sb.acquireFence);
        //csb->dump();
        return csb;
    }
}

void StreamBufferCache::remove(const int64_t bufferId) {
    mCache.erase(bufferId);
}

void StreamBufferCache::clearStreamInfo() {
    for (auto& kv : mCache) {
        kv.second.setStreamInfo(nullptr);
    }
}

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

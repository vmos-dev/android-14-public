/*
 **
 ** Copyright 2012 The Android Open Source Project
 **
 ** Licensed under the Apache License Version 2.0(the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing software
 ** distributed under the License is distributed on an "AS IS" BASIS
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

// #define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "FramebufferSurface"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/String8.h>
#include <log/log.h>

#include <hardware/hardware.h>
#include <gui/BufferItem.h>
#include <gui/BufferQueue.h>
#include <gui/Surface.h>

#include <ui/DebugUtils.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>

#include "FramebufferSurface.h"
#include "HWComposer.h"
#include "../SurfaceFlinger.h"

namespace android {

using ui::Dataspace;

/* SurfaceFlinger cannot simply include './src/mali_gralloc_usages.h', so a copy is defined. */
#define RK_GRALLOC_USAGE_EXTERNAL_DISP (1ULL << 55)

FramebufferSurface::FramebufferSurface(HWComposer& hwc, PhysicalDisplayId displayId,
                                       const sp<IGraphicBufferConsumer>& consumer,
                                       const ui::Size& size, const ui::Size& maxSize)
      : ConsumerBase(consumer),
        mDisplayId(displayId),
        mMaxSize(maxSize),
        mCurrentBufferSlot(-1),
        mCurrentBuffer(),
        mCurrentFence(Fence::NO_FENCE),
        mHwc(hwc),
        mHasPendingRelease(false),
        mPreviousBufferSlot(BufferQueue::INVALID_BUFFER_SLOT),
        mPreviousBuffer() {
    ALOGV("Creating for display %s", to_string(displayId).c_str());

    mName = "FramebufferSurface";
    mConsumer->setConsumerName(mName);
    /* modify usage for rockchip afbc layer select
     * When the external display of the chip platform
     * does not support afbc, configure
     * RK_GRALLOC_USAGE_EXTERNAL_DISP/GRALLOC_USAGE__RK_EXT__EXTERNAL_DISP
     * to gralloc. */
    uint64_t flags = GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER |
                     GRALLOC_USAGE_HW_COMPOSER;

#if DISABLE_EXTERNAL_DISP_AFBC
        if ((displayId.value & 0xff) != LEGACY_DISPLAY_TYPE_PRIMARY) {
            /* Through this usage, gralloc will disable AFBC on the external display. */
#if USE_GRALLOC_4
            flags |= RK_GRALLOC_USAGE_EXTERNAL_DISP;
#else
            flags |= GRALLOC_USAGE__RK_EXT__EXTERNAL_DISP;
#endif
        }
#endif

    mConsumer->setConsumerUsageBits(flags);

    const auto limitedSize = limitSize(size);
    mConsumer->setDefaultBufferSize(limitedSize.width, limitedSize.height);
    mConsumer->setMaxAcquiredBufferCount(
            SurfaceFlinger::maxFrameBufferAcquiredBuffers - 1);

    for (size_t i = 0; i < sizeof(mHwcBufferIds) / sizeof(mHwcBufferIds[0]); ++i) {
        mHwcBufferIds[i] = UINT64_MAX;
    }
}

void FramebufferSurface::resizeBuffers(const ui::Size& newSize) {
    const auto limitedSize = limitSize(newSize);
    mConsumer->setDefaultBufferSize(limitedSize.width, limitedSize.height);
}

status_t FramebufferSurface::beginFrame(bool /*mustRecompose*/) {
    return NO_ERROR;
}

status_t FramebufferSurface::prepareFrame(CompositionType /*compositionType*/) {
    return NO_ERROR;
}

status_t FramebufferSurface::advanceFrame() {
    Mutex::Autolock lock(mMutex);

    BufferItem item;
    status_t err = acquireBufferLocked(&item, 0);
    if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
        mDataspace = Dataspace::UNKNOWN;
        return NO_ERROR;
    } else if (err != NO_ERROR) {
        ALOGE("error acquiring buffer: %s (%d)", strerror(-err), err);
        mDataspace = Dataspace::UNKNOWN;
        return err;
    }

    // If the BufferQueue has freed and reallocated a buffer in mCurrentSlot
    // then we may have acquired the slot we already own.  If we had released
    // our current buffer before we call acquireBuffer then that release call
    // would have returned STALE_BUFFER_SLOT, and we would have called
    // freeBufferLocked on that slot.  Because the buffer slot has already
    // been overwritten with the new buffer all we have to do is skip the
    // releaseBuffer call and we should be in the same state we'd be in if we
    // had released the old buffer first.
    if (mCurrentBufferSlot != BufferQueue::INVALID_BUFFER_SLOT &&
        item.mSlot != mCurrentBufferSlot) {
        mHasPendingRelease = true;
        mPreviousBufferSlot = mCurrentBufferSlot;
        mPreviousBuffer = mCurrentBuffer;
    }
    mCurrentBufferSlot = item.mSlot;
    mCurrentBuffer = mSlots[mCurrentBufferSlot].mGraphicBuffer;
    mCurrentFence = item.mFence;
    mDataspace = static_cast<Dataspace>(item.mDataSpace);

    // assume HWC has previously seen the buffer in this slot
    sp<GraphicBuffer> hwcBuffer = sp<GraphicBuffer>(nullptr);
    if (mCurrentBuffer->getId() != mHwcBufferIds[mCurrentBufferSlot]) {
        mHwcBufferIds[mCurrentBufferSlot] = mCurrentBuffer->getId();
        hwcBuffer = mCurrentBuffer; // HWC hasn't previously seen this buffer in this slot
    }
    status_t result = mHwc.setClientTarget(mDisplayId, mCurrentBufferSlot, mCurrentFence, hwcBuffer,
                                           mDataspace);
    if (result != NO_ERROR) {
        ALOGE("error posting framebuffer: %s (%d)", strerror(-result), result);
        return result;
    }

    return NO_ERROR;
}

void FramebufferSurface::freeBufferLocked(int slotIndex) {
    ConsumerBase::freeBufferLocked(slotIndex);
    if (slotIndex == mCurrentBufferSlot) {
        mCurrentBufferSlot = BufferQueue::INVALID_BUFFER_SLOT;
    }
}

void FramebufferSurface::onFrameCommitted() {
    if (mHasPendingRelease) {
        sp<Fence> fence = mHwc.getPresentFence(mDisplayId);
        if (fence->isValid()) {
            status_t result = addReleaseFence(mPreviousBufferSlot,
                    mPreviousBuffer, fence);
            ALOGE_IF(result != NO_ERROR, "onFrameCommitted: failed to add the"
                    " fence: %s (%d)", strerror(-result), result);
        }
        status_t result = releaseBufferLocked(mPreviousBufferSlot, mPreviousBuffer);
        ALOGE_IF(result != NO_ERROR, "onFrameCommitted: error releasing buffer:"
                " %s (%d)", strerror(-result), result);

        mPreviousBuffer.clear();
        mHasPendingRelease = false;
    }
}

ui::Size FramebufferSurface::limitSize(const ui::Size& size) {
    return limitSizeInternal(size, mMaxSize);
}

ui::Size FramebufferSurface::limitSizeInternal(const ui::Size& size, const ui::Size& maxSize) {
    ui::Size limitedSize = size;
    bool wasLimited = false;
    if (size.width > maxSize.width && maxSize.width != 0) {
        const float aspectRatio = static_cast<float>(size.width) / size.height;
        limitedSize.height = maxSize.width / aspectRatio;
        limitedSize.width = maxSize.width;
        wasLimited = true;
    }
    if (limitedSize.height > maxSize.height && maxSize.height != 0) {
        const float aspectRatio = static_cast<float>(size.width) / size.height;
        limitedSize.height = maxSize.height;
        limitedSize.width = maxSize.height * aspectRatio;
        wasLimited = true;
    }
    ALOGI_IF(wasLimited, "Framebuffer size has been limited to [%dx%d] from [%dx%d]",
             limitedSize.width, limitedSize.height, size.width, size.height);
    return limitedSize;
}

void FramebufferSurface::dumpAsString(String8& result) const {
    Mutex::Autolock lock(mMutex);
    result.append("   FramebufferSurface\n");
    result.appendFormat("      mDataspace=%s (%d)\n",
                        dataspaceDetails(static_cast<android_dataspace>(mDataspace)).c_str(),
                        mDataspace);
    ConsumerBase::dumpLocked(result, "      ");
}

void FramebufferSurface::dumpLocked(String8& result, const char* prefix) const {
    ConsumerBase::dumpLocked(result, prefix);
}

const sp<Fence>& FramebufferSurface::getClientTargetAcquireFence() const {
    return mCurrentFence;
}

} // namespace android

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic pop // ignored "-Wconversion"

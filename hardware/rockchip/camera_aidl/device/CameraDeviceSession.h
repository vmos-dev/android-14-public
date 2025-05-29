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

#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <aidl/android/hardware/camera/common/Status.h>
#include <aidl/android/hardware/camera/device/BnCameraDeviceSession.h>

#include <fmq/AidlMessageQueue.h>

#include "BlockingQueue.h"
#include "HwCamera.h"
#include "StreamBufferCache.h"
#include <unordered_map>

#include "HandleImporter.h"
#include <fmq/MessageQueue.h>
#include <aidl/android/hardware/camera/device/NotifyMsg.h>

#include <hardware/gralloc1.h>
#define RK_GRALLOC_USAGE_RANGE_FULL GRALLOC1_CONSUMER_USAGE_PRIVATE_17
#define RK_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601 GRALLOC1_CONSUMER_USAGE_PRIVATE_18

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using aidl::android::hardware::camera::common::Status;

using aidl::android::hardware::camera::device::BnCameraDeviceSession;
using aidl::android::hardware::camera::device::BufferCache;
using aidl::android::hardware::camera::device::CameraMetadata;
using aidl::android::hardware::camera::device::CameraOfflineSessionInfo;
using aidl::android::hardware::camera::device::CaptureRequest;
using aidl::android::hardware::camera::device::CaptureResult;
using aidl::android::hardware::camera::device::HalStream;
using aidl::android::hardware::camera::device::ICameraDeviceCallback;
using aidl::android::hardware::camera::device::ICameraOfflineSession;
using aidl::android::hardware::camera::device::RequestTemplate;
using aidl::android::hardware::camera::device::StreamBuffer;
using aidl::android::hardware::camera::device::StreamConfiguration;

using aidl::android::hardware::common::fmq::MQDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;

using aidl::android::hardware::graphics::common::BufferUsage;
using aidl::android::hardware::graphics::common::PixelFormat;

using ndk::ScopedAStatus;

using ::android::hardware::camera::common::V1_0::helper::HandleImporter;

using ::android::hardware::kSynchronizedReadWrite;
using ::android::hardware::MessageQueue;

using ::aidl::android::hardware::camera::device::NotifyMsg;

struct CameraDevice;


// The camera3_stream_t sent to conventional HAL. Added mId fields to enable stream ID lookup
// fromt a downcasted camera3_stream
struct Camera3Stream : public camera3_stream {
    int mId;
};

/**
 * Function pointer types with C calling convention to
 * use for HAL callback functions.
 */
extern "C" {
    typedef void (callbacks_process_capture_result_t)(
        const struct camera3_callback_ops *,
        const camera3_capture_result_t *);

    typedef void (callbacks_notify_t)(
        const struct camera3_callback_ops *,
        const camera3_notify_msg_t *);
}

struct CameraDeviceSession : public BnCameraDeviceSession,protected camera3_callback_ops{
    CameraDeviceSession(std::shared_ptr<CameraDevice> parent,
                        std::shared_ptr<ICameraDeviceCallback> cb,
                        hw::HwCamera& hwCamera);
    ~CameraDeviceSession() override;

    ScopedAStatus close() override;
    ScopedAStatus configureStreams(const StreamConfiguration& cfg,
                                   std::vector<HalStream>* halStreamsOut) override;
    ScopedAStatus constructDefaultRequestSettings(RequestTemplate tpl,
                                                  CameraMetadata* metadata) override;
    ScopedAStatus flush() override;
    ScopedAStatus getCaptureRequestMetadataQueue(
        MQDescriptor<int8_t, SynchronizedReadWrite>* desc) override;
    ScopedAStatus getCaptureResultMetadataQueue(
        MQDescriptor<int8_t, SynchronizedReadWrite>* desc) override;
    ScopedAStatus isReconfigurationRequired(const CameraMetadata& oldSessionParams,
                                            const CameraMetadata& newSessionParams,
                                            bool* result) override;
    ScopedAStatus processCaptureRequest(const std::vector<CaptureRequest>& requests,
                                        const std::vector<BufferCache>& cachesToRemove,
                                        int32_t* count) override;
    ScopedAStatus signalStreamFlush(const std::vector<int32_t>& streamIds,
                                    int32_t streamConfigCounter) override;
    ScopedAStatus switchToOffline(const std::vector<int32_t>& streamsToKeep,
                                  CameraOfflineSessionInfo* offlineSessionInfo,
                                  std::shared_ptr<ICameraOfflineSession>* session) override;
    ScopedAStatus repeatingRequestEnd(int32_t frameNumber,
                                      const std::vector<int32_t>& streamIds) override;

    static bool isStreamCombinationSupported(const StreamConfiguration& cfg,
                                             hw::HwCamera& hwCamera);
    status_t constructCaptureResult(CaptureResult& result,
                                const camera3_capture_result *hal_result);
    bool preProcessConfigurationLocked(const StreamConfiguration& requestedConfiguration,
            camera3_stream_configuration_t *stream_list /*out*/,
            std::vector<camera3_stream_t*> *streams /*out*/);
    void postProcessConfigurationLocked(const StreamConfiguration& requestedConfiguration);
protected:

    void notify(NotifyMsg msg);
    /**
     * Static callback forwarding methods from HAL to instance
     */
    static callbacks_process_capture_result_t sProcessCaptureResult;
    static callbacks_notify_t sNotify;

    // By default camera service uses frameNumber/streamId pair to retrieve the buffer that
    // was sent to HAL. Override this implementation if HAL is using buffers from buffer management
    // APIs to send output buffer.
    virtual uint64_t getCapResultBufferId(const buffer_handle_t& buf, int streamId);

    // Static helper method to copy/shrink capture result metadata sent by HAL
    // Temporarily allocated metadata copy will be hold in mds
    static void sShrinkCaptureResult(
            camera3_capture_result* dst, const camera3_capture_result* src,
            std::vector<::android::hardware::camera::common::V1_0::helper::CameraMetadata>* mds,
            std::vector<const camera_metadata_t*>* physCamMdArray,
            bool handlePhysCam);
    static bool sShouldShrink(const camera_metadata_t* md);
    static camera_metadata_t* sCreateCompactCopy(const camera_metadata_t* src);

    struct AETriggerCancelOverride {
        bool applyAeLock;
        uint8_t aeLock;
        bool applyAePrecaptureTrigger;
        uint8_t aePrecaptureTrigger;
    };
common::V1_0::helper::CameraMetadata mDeviceInfo;

    using RequestMetadataQueue = MessageQueue<uint8_t, kSynchronizedReadWrite>;
    std::unique_ptr<RequestMetadataQueue> mRequestMetadataQueue;
    using ResultMetadataQueue = MessageQueue<uint8_t, kSynchronizedReadWrite>;
    std::shared_ptr<ResultMetadataQueue> mResultMetadataQueue;
    void processCaptureResult(
            const camera3_callback_ops *cb,
            const camera3_capture_result *hal_result);

private:
    using MetadataQueue = AidlMessageQueue<int8_t, SynchronizedReadWrite>;
    using HwCaptureRequest = hw::HwCaptureRequest;

    struct DelayedCaptureResult {
        hw::DelayedStreamBuffer delayedBuffer;
        int frameNumber;
    };

    void closeImpl();
    void flushImpl(std::chrono::steady_clock::time_point start);
    int waitFlushingDone(std::chrono::steady_clock::time_point start);
    static std::pair<Status, std::vector<HalStream>>
        configureStreamsStatic(const StreamConfiguration& cfg,
                               hw::HwCamera& hwCamera);
    Status processOneCaptureRequest(const CaptureRequest& request);
    void captureThreadLoop();
    void delayedCaptureThreadLoop();
    bool popCaptureRequest(HwCaptureRequest* req);
    struct timespec captureOneFrame(struct timespec nextFrameT, HwCaptureRequest req);
    void disposeCaptureRequest(HwCaptureRequest req);
    void consumeCaptureResult(CaptureResult cr);
    void notifyBuffersReturned(size_t n);

    const std::shared_ptr<CameraDevice> mParent;
    const std::shared_ptr<ICameraDeviceCallback> mCb;
    hw::HwCamera& mHwCamera;
    camera3_device_t* mDevice;
    const uint32_t mDeviceVersion;
    const bool mFreeBufEarly;
    MetadataQueue mRequestQueue;
    MetadataQueue mResultQueue;
    std::mutex mResultQueueMutex;

    StreamBufferCache mStreamBufferCache;

    BlockingQueue<HwCaptureRequest> mCaptureRequests;
    BlockingQueue<DelayedCaptureResult> mDelayedCaptureResults;

    size_t mNumBuffersInFlight = 0;
    std::condition_variable mNoBuffersInFlight;
    std::mutex mNumBuffersInFlightMtx;

    std::thread mCaptureThread;
    std::thread mDelayedCaptureThread;

    std::atomic<bool> mFlushing = false;

    std::atomic<bool> mClosed = false;

    bool mIsAELockAvailable;
    bool mDerivePostRawSensKey;
    uint32_t mNumPartialResults;

    // Stream ID -> Camera3Stream cache
    std::map<int, Camera3Stream> mStreamMap;

    mutable Mutex mInflightLock; // protecting mInflightBuffers and mCirculatingBuffers
    // (streamID, frameNumber) -> inflight buffer cache
    std::map<std::pair<int, uint32_t>, camera3_stream_buffer_t>  mInflightBuffers;
    mutable Mutex mInflightRequestLock;
    std::map<uint32_t, HwCaptureRequest>  mInflightRequest;
    std::unordered_map<int,std::unordered_map<int,buffer_handle_t>> mMapReqInputBuffers;
    std::unordered_map<int,std::unordered_map<int,buffer_handle_t>> mMapReqOutputBuffers;

    // (frameNumber, AETriggerOverride) -> inflight request AETriggerOverrides
    std::map<uint32_t, AETriggerCancelOverride> mInflightAETriggerOverrides;
    ::android::hardware::camera::common::V1_0::helper::CameraMetadata mOverridenResult;
    std::map<uint32_t, bool> mInflightRawBoostPresent;
    ::android::hardware::camera::common::V1_0::helper::CameraMetadata mOverridenRequest;

    static const uint64_t BUFFER_ID_NO_BUFFER = 0;
    // buffers currently ciculating between HAL and camera service
    // key: bufferId sent via HIDL interface
    // value: imported buffer_handle_t
    // Buffer will be imported during process_capture_request and will be freed
    // when the its stream is deleted or camera device session is closed
    typedef std::unordered_map<uint64_t, buffer_handle_t> CirculatingBuffers;
    // Stream ID -> circulating buffers map
    std::map<int, CirculatingBuffers> mCirculatingBuffers;

    bool mInitFail;
    bool mFirstRequest = false;

    std::vector<int> mVideoStreamIds;

    static HandleImporter sHandleImporter;
    static buffer_handle_t sEmptyBuffer;

    bool initialize();

    static bool shouldFreeBufEarly();

    // Validate and import request's input buffer and acquire fence
    virtual Status importRequest(
            const CaptureRequest& request,
            std::vector<buffer_handle_t*>& allBufPtrs,
            std::vector<int>& allFences);

    Status importRequestImpl(
            const CaptureRequest& request,
            std::vector<buffer_handle_t*>& allBufPtrs,
            std::vector<int>& allFences,
            // Optional argument for ICameraDeviceSession@3.5 impl
            bool allowEmptyBuf = false);
    Status importBuffer(int32_t streamId,
        uint64_t bufId, buffer_handle_t buf,
        /*out*/buffer_handle_t** outBufPtr,
        bool allowEmptyBuf);

    static void cleanupInflightFences(
            std::vector<int>& allFences, size_t numFences);

    void cleanupBuffersLocked(int id);
    void updateBufferCaches(const std::vector<BufferCache>& cachesToRemove);
    android_dataspace mapToLegacyDataspace(
        android_dataspace dataSpace) const;


    bool handleAePrecaptureCancelRequestLocked(
            const camera3_capture_request_t &halRequest,
            android::hardware::camera::common::V1_0::helper::CameraMetadata *settings /*out*/,
            AETriggerCancelOverride *override /*out*/);

    void overrideResultForPrecaptureCancelLocked(
            const AETriggerCancelOverride &aeTriggerCancelOverride,
            ::android::hardware::camera::common::V1_0::helper::CameraMetadata *settings /*out*/);

    mutable Mutex mInflightExposureTimeLock;
    std::map<int32_t, int64_t>  mInflightExposureTimeNs;
    int64_t mSensorExposureTimeNs;
    int64_t getSensorExposureTime(){
        Mutex::Autolock _l(mInflightExposureTimeLock);
        return mSensorExposureTimeNs;
    }
    void setSensorExposureTime(int32_t frameNumber, int64_t expouserTimeNs){
        Mutex::Autolock _l(mInflightExposureTimeLock);
        mInflightExposureTimeNs[frameNumber] = expouserTimeNs;
    }
};

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

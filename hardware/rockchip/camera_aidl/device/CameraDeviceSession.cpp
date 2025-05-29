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

#define FAILURE_DEBUG_PREFIX "CameraDeviceSession"

#include <inttypes.h>

#include <chrono>
//#include <memory>
#include <set>
#include <cutils/properties.h>

#include <log/log.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <utils/ThreadDefs.h>

#include <aidl/android/hardware/camera/device/ErrorCode.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>

#include "debug.h"
#include "CameraDeviceSession.h"
#include "CameraDevice.h"
#include "metadata_utils.h"
#include <sync/sync.h>
#include <system/camera_metadata.h>

#include <utils/Trace.h>

#include "hardware/camera3.h"

#include <ui/GraphicBufferMapper.h>

#include <aidl/android/hardware/camera/device/ErrorMsg.h>
#include <aidl/android/hardware/camera/device/ShutterMsg.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using aidl::android::hardware::camera::common::Status;
using aidl::android::hardware::camera::device::CaptureResult;
using aidl::android::hardware::camera::device::ErrorCode;
using aidl::android::hardware::camera::device::StreamRotation;
using aidl::android::hardware::camera::device::StreamType;
using aidl::android::hardware::camera::device::Stream;

using aidl::android::hardware::graphics::common::Dataspace;

using aidl::android::hardware::camera::device::BufferStatus;

using base::unique_fd;

using ::aidl::android::hardware::camera::device::ErrorMsg;
using ::aidl::android::hardware::camera::device::ShutterMsg;

namespace {
constexpr char kClass[] = "CameraDeviceSession";

constexpr int64_t kOneSecondNs = 1000000000;
constexpr int64_t kDefaultSensorExposureTimeNs = kOneSecondNs / 100;
constexpr size_t kMsgQueueSize = 256 * 1024;

// Size of request metadata fast message queue. Change to 0 to always use hwbinder buffer.
static constexpr int32_t CAMERA_REQUEST_METADATA_QUEUE_SIZE = 1 << 20 /* 1MB */;
// Size of result metadata fast message queue. Change to 0 to always use hwbinder buffer.
static constexpr int32_t CAMERA_RESULT_METADATA_QUEUE_SIZE  = 1 << 20 /* 1MB */;

// Metadata sent by HAL will be replaced by a compact copy
// if their (total size >= compact size + METADATA_SHRINK_ABS_THRESHOLD &&
//           total_size >= compact size * METADATA_SHRINK_REL_THRESHOLD)
// Heuristically picked by size of one page
static constexpr int METADATA_SHRINK_ABS_THRESHOLD = 4096;
static constexpr int METADATA_SHRINK_REL_THRESHOLD = 2;

struct timespec timespecAddNanos(const struct timespec t, const int64_t addNs) {
    const lldiv_t r = lldiv(t.tv_nsec + addNs, kOneSecondNs);

    struct timespec tm;
    tm.tv_sec = t.tv_sec + r.quot;
    tm.tv_nsec = r.rem;

    return tm;
}

int64_t timespec2nanos(const struct timespec t) {
    return kOneSecondNs * t.tv_sec + t.tv_nsec;
}

const char* pixelFormatToStr(const PixelFormat fmt, char* buf, int bufSz) {
    switch (fmt) {
    case PixelFormat::UNSPECIFIED: return "UNSPECIFIED";
    case PixelFormat::IMPLEMENTATION_DEFINED: return "IMPLEMENTATION_DEFINED";
    case PixelFormat::YCBCR_420_888: return "YCBCR_420_888";
    case PixelFormat::RGBA_8888: return "RGBA_8888";
    case PixelFormat::BLOB: return "BLOB";
    default:
        snprintf(buf, bufSz, "0x%x", static_cast<uint32_t>(fmt));
        return buf;
    }
}

void notifyError(ICameraDeviceCallback* cb,
                 const int32_t frameNumber,
                 const int32_t errorStreamId,
                 const ErrorCode err) {
    using aidl::android::hardware::camera::device::NotifyMsg;
    using aidl::android::hardware::camera::device::ErrorMsg;
    using NotifyMsgTag = NotifyMsg::Tag;

    NotifyMsg msg;

    {
        ErrorMsg errorMsg;
        errorMsg.frameNumber = frameNumber;
        errorMsg.errorStreamId = errorStreamId;
        errorMsg.errorCode = err;
        msg.set<NotifyMsgTag::error>(errorMsg);
    }

    cb->notify({msg});
}

void notifyShutter(ICameraDeviceCallback* cb,
                   const int32_t frameNumber,
                   const int64_t timestamp) {
    using aidl::android::hardware::camera::device::NotifyMsg;
    using aidl::android::hardware::camera::device::ShutterMsg;
    using NotifyMsgTag = NotifyMsg::Tag;
    //ALOGD("%s frameNumber:%d timestamp=%" PRId64" ",__FUNCTION__,frameNumber,(long)timestamp);
    NotifyMsg msg;

    {
        ShutterMsg shutterMsg;
        shutterMsg.frameNumber = frameNumber;
        shutterMsg.timestamp = timestamp;
        msg.set<NotifyMsgTag::shutter>(shutterMsg);
    }

    cb->notify({msg});
}

CaptureResult makeCaptureResult(const int frameNumber,
                                CameraMetadata metadata,
                                std::vector<StreamBuffer> outputBuffers) {
    CaptureResult cr;
    cr.frameNumber = frameNumber;
    cr.result = std::move(metadata);
    cr.outputBuffers = std::move(outputBuffers);
    cr.inputBuffer.streamId = -1;
    cr.inputBuffer.bufferId = 0;
    cr.partialResult = cr.result.metadata.empty() ? 0 : 1;
    //ALOGD("%s cr.frameNumber:%d,  cr.outputBuffers:%d cr.partialResult:%d",__FUNCTION__,
    // cr.frameNumber,
    // //(int)cr.result,
    // cr.outputBuffers.size(),
    // cr.partialResult
    // );
    return cr;
}

bool convertFromAidl(const CameraMetadata& src, const camera_metadata_t** dst) {
    const std::vector<uint8_t>& metadata = src.metadata;
    if (metadata.empty()) {
        // Special case for null metadata
        *dst = nullptr;
        return true;
    }

    const uint8_t* data = metadata.data();
    // check that the size of CameraMetadata match underlying camera_metadata_t
    if (get_camera_metadata_size((camera_metadata_t*)data) != metadata.size()) {
        ALOGE("%s: input CameraMetadata is corrupt!", __FUNCTION__);
        return false;
    }
    *dst = (camera_metadata_t*)data;
    return true;
}
void convertToAidl(const camera_metadata_t* src, CameraMetadata* dest) {
    if (src == nullptr) {
        return;
    }

    size_t size = get_camera_metadata_size(src);
    auto* src_start = (uint8_t*)src;
    uint8_t* src_end = src_start + size;
    dest->metadata.assign(src_start, src_end);
}

void convertFromAidl(const Stream &src, Camera3Stream* dst) {
    dst->mId = src.id;
    dst->stream_type = (int) src.streamType;
    dst->width = src.width;
    dst->height = src.height;
    dst->format = (int) src.format;
    dst->data_space = (android_dataspace_t) src.dataSpace;
    dst->rotation = (int) src.rotation;
    dst->usage = (uint32_t) src.usage;
    // Fields to be filled by HAL (max_buffers, priv) are initialized to 0
    dst->max_buffers = 0;
    dst->priv = 0;
    return;
}

}  // namespace

HandleImporter CameraDeviceSession::sHandleImporter;
buffer_handle_t CameraDeviceSession::sEmptyBuffer = nullptr;

CameraDeviceSession::CameraDeviceSession(
        std::shared_ptr<CameraDevice> parent,
        std::shared_ptr<ICameraDeviceCallback> cb,
        hw::HwCamera& hwCamera)
         : camera3_callback_ops({&sProcessCaptureResult, &sNotify, nullptr, nullptr})
         , mParent(std::move(parent))
         , mCb(std::move(cb))
         , mHwCamera(hwCamera)
         , mDevice(mHwCamera.getDevice())
         , mDeviceVersion(mDevice->common.version)
         , mFreeBufEarly(shouldFreeBufEarly())
         , mRequestQueue(kMsgQueueSize, false)
         , mResultQueue(kMsgQueueSize, false)
         , mIsAELockAvailable(false)
         , mNumPartialResults(1)
         , mSensorExposureTimeNs(kDefaultSensorExposureTimeNs)
          {
    LOG_ALWAYS_FATAL_IF(!mRequestQueue.isValid());
    LOG_ALWAYS_FATAL_IF(!mResultQueue.isValid());
    //mCaptureThread = std::thread(&CameraDeviceSession::captureThreadLoop, this);
    mDelayedCaptureThread = std::thread(&CameraDeviceSession::delayedCaptureThreadLoop, this);
    // mDevice = mHwCamera.getDevice();
    // mDeviceVersion =mDevice->common.version;
    camera_metadata_entry partialResultsCount =
            mDeviceInfo.find(ANDROID_REQUEST_PARTIAL_RESULT_COUNT);
    if (partialResultsCount.count > 0) {
        mNumPartialResults = partialResultsCount.data.i32[0];
    }

    camera_metadata_entry aeLockAvailableEntry = mDeviceInfo.find(
            ANDROID_CONTROL_AE_LOCK_AVAILABLE);
    if (aeLockAvailableEntry.count > 0) {
        mIsAELockAvailable = (aeLockAvailableEntry.data.u8[0] ==
                ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE);
    }

    // Determine whether we need to derive sensitivity boost values for older devices.
    // If post-RAW sensitivity boost range is listed, so should post-raw sensitivity control
    // be listed (as the default value 100)
    if (mDeviceInfo.exists(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE)) {
        mDerivePostRawSensKey = true;
    }

    mInitFail = initialize();
}

bool CameraDeviceSession::initialize(){
        /** Initialize device with callback functions */
    ATRACE_BEGIN("camera3->initialize");
    status_t res = mDevice->ops->initialize(mDevice, this);
    ATRACE_END();
    if (res != OK) {
        ALOGE("%s: Unable to initialize HAL device: %s (%d)",
                __FUNCTION__, strerror(-res), res);
        mDevice->common.close(&mDevice->common);
        mClosed = true;
        return true;
    }

    // "ro.camera" properties are no longer supported on vendor side.
    //  Support a fall back for the fmq size override that uses "ro.vendor.camera"
    //  properties.
    int32_t reqFMQSize = property_get_int32("ro.vendor.camera.req.fmq.size", /*default*/-1);
    if (reqFMQSize < 0) {
        reqFMQSize = property_get_int32("ro.camera.req.fmq.size", /*default*/-1);
        if (reqFMQSize < 0) {
            reqFMQSize = CAMERA_REQUEST_METADATA_QUEUE_SIZE;
        } else {
            ALOGV("%s: request FMQ size overridden to %d", __FUNCTION__, reqFMQSize);
        }
    } else {
        ALOGV("%s: request FMQ size overridden to %d via fallback property", __FUNCTION__,
                reqFMQSize);
    }

    mRequestMetadataQueue = std::make_unique<RequestMetadataQueue>(
            static_cast<size_t>(reqFMQSize),
            false /* non blocking */);
    if (!mRequestMetadataQueue->isValid()) {
        ALOGE("%s: invalid request fmq", __FUNCTION__);
        return true;
    }

    // "ro.camera" properties are no longer supported on vendor side.
    //  Support a fall back for the fmq size override that uses "ro.vendor.camera"
    //  properties.
    int32_t resFMQSize = property_get_int32("ro.vendor.camera.res.fmq.size", /*default*/-1);
    if (resFMQSize < 0) {
        resFMQSize = property_get_int32("ro.camera.res.fmq.size", /*default*/-1);
        if (resFMQSize < 0) {
            resFMQSize = CAMERA_RESULT_METADATA_QUEUE_SIZE;
        } else {
            ALOGV("%s: result FMQ size overridden to %d", __FUNCTION__, resFMQSize);
        }
    } else {
        ALOGV("%s: result FMQ size overridden to %d via fallback property", __FUNCTION__,
                resFMQSize);
    }

    mResultMetadataQueue = std::make_shared<RequestMetadataQueue>(
            static_cast<size_t>(resFMQSize),
            false /* non blocking */);
    if (!mResultMetadataQueue->isValid()) {
        ALOGE("%s: invalid result fmq", __FUNCTION__);
        return true;
    }

    return false;
}
bool CameraDeviceSession::shouldFreeBufEarly() {
    return property_get_bool("ro.vendor.camera.free_buf_early", 0) == 1;
}
CameraDeviceSession::~CameraDeviceSession() {
    closeImpl();

    // mCaptureRequests.cancel();
    mDelayedCaptureResults.cancel();
    //mCaptureThread.join();
    mDelayedCaptureThread.join();
}

ScopedAStatus CameraDeviceSession::close() {
    closeImpl();
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDeviceSession::configureStreams(
        const StreamConfiguration& cfg,
        std::vector<HalStream>* halStreamsOut) {

    camera3_stream_configuration_t stream_list{};
    std::vector<camera3_stream_t*> streams;
    stream_list.operation_mode = static_cast<uint32_t>(cfg.operationMode);
    stream_list.num_streams = cfg.streams.size();
    streams.resize(stream_list.num_streams);
    stream_list.streams = streams.data();
    for (uint32_t i = 0; i < stream_list.num_streams; i++) {
        int id = cfg.streams[i].id;

        if (mStreamMap.count(id) == 0) {
            Camera3Stream* stream = new Camera3Stream;
            streams[i] = (camera3_stream_t*)stream;

            stream->stream_type = (int) cfg.streams[i].streamType;
            stream->width = cfg.streams[i].width;
            stream->height = cfg.streams[i].height;
            stream->format =(int)  cfg.streams[i].format;
            ALOGD("%s %d format:%d (%dx%d)",__FUNCTION__,id , stream->format,stream->width,stream->height);
            stream->rotation =  (int)cfg.streams[i].rotation;
            stream->usage = (uint32_t) cfg.streams[i].usage;
            stream->data_space = static_cast<android_dataspace_t> (cfg.streams[i].dataSpace);
            stream->max_buffers = 0;
            stream->priv = 0;

            mStreamMap[id] = *stream;
            mStreamMap[id].mId = id;
            mCirculatingBuffers.emplace(stream->mId, CirculatingBuffers{});
        } else {
            if (mStreamMap[id].stream_type !=
                    (int) cfg.streams[i].streamType ||
                    mStreamMap[id].width != cfg.streams[i].width ||
                    mStreamMap[id].height != cfg.streams[i].height ||
                    mStreamMap[id].format != (int) cfg.streams[i].format ||
                    mStreamMap[id].data_space !=static_cast<android_dataspace_t> (
                                    cfg.streams[i].dataSpace)) {
                ALOGE("%s: stream %d configuration changed!", __FUNCTION__, id);
                return toScopedAStatus(FAILURE(Status::INTERNAL_ERROR));
            }
            mStreamMap[id].rotation = (int) cfg.streams[i].rotation;
            mStreamMap[id].usage = (uint32_t) cfg.streams[i].usage;
        }
        streams[i] = &mStreamMap[id];

    }

    ALOGD("%s:%s:%d cfg={ "
          ".streams.size=%zu, .operationMode=%u, .cfg.sessionParams.size()=%zu, "
          " .streamConfigCounter=%d, .multiResolutionInputImage=%s }",
          kClass, __func__, __LINE__,
          cfg.streams.size(), static_cast<uint32_t>(cfg.operationMode),
          cfg.sessionParams.metadata.size(), cfg.streamConfigCounter,
          (cfg.multiResolutionInputImage ? "true" : "false"));

    if (mFreeBufEarly)
    {
        // Remove buffers of deleted streams
        for(auto it = mStreamMap.begin(); it != mStreamMap.end(); it++) {
            int id = it->first;
            bool found = false;
            for (const auto& stream : cfg.streams) {
                if (id == stream.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Unmap all buffers of deleted stream
                cleanupBuffersLocked(id);
            }
        }
    }

    for (const auto& s : cfg.streams) {
        const uint32_t dataspaceBits = static_cast<uint32_t>(s.dataSpace);
        const uint32_t dataspaceLow = dataspaceBits & 0xFFFF;
        const uint32_t dataspaceS =
            (dataspaceBits & static_cast<uint32_t>(Dataspace::STANDARD_MASK)) >>
            static_cast<uint32_t>(Dataspace::STANDARD_SHIFT);
        const uint32_t dataspaceT =
            (dataspaceBits & static_cast<uint32_t>(Dataspace::TRANSFER_MASK)) >>
            static_cast<uint32_t>(Dataspace::TRANSFER_SHIFT);
        const uint32_t dataspaceR =
            (dataspaceBits & static_cast<uint32_t>(Dataspace::RANGE_MASK)) >>
            static_cast<uint32_t>(Dataspace::RANGE_SHIFT);

        char pixelFormatStrBuf[16];

        ALOGD("%s:%s:%d stream={ .id=%d, "
              ".streamType=%u, .width=%d, .height=%d, .format=%s, .usage=0x%" PRIx64 ", "
              ".dataSpace={ .low=0x%x, .s=%u, .t=%u, .r=%u }, .rotation=%u, .physicalCameraId='%s', .bufferSize=%d, "
              ".groupId=%d, .dynamicRangeProfile=0x%x }", kClass, __func__, __LINE__,
              s.id, static_cast<unsigned>(s.streamType), s.width, s.height,
              pixelFormatToStr(s.format, pixelFormatStrBuf, sizeof(pixelFormatStrBuf)),
              static_cast<uint64_t>(s.usage),
              dataspaceLow, dataspaceS, dataspaceT, dataspaceR,
              static_cast<unsigned>(s.rotation),
              s.physicalCameraId.c_str(), s.bufferSize, s.groupId,
              static_cast<unsigned>(s.dynamicRangeProfile)
        );
    }

    auto [status, halStreams] = configureStreamsStatic(cfg, mHwCamera);
    if (status != Status::OK) {
        return toScopedAStatus(status);
    }

    for (const auto& s : cfg.streams) {
        if((int)s.useCase > 0 && s.format == PixelFormat::YCBCR_420_888){
            return toScopedAStatus(FAILURE(Status::ILLEGAL_ARGUMENT));
        }
    }
    ATRACE_BEGIN("camera3->configure_streams");
    status_t ret = mDevice->ops->configure_streams(mDevice, &stream_list);
    ATRACE_END();

    // delete unused streams, note we do this after adding new streams to ensure new stream
    // will not have the same address as deleted stream, and HAL has a chance to reference
    // the to be deleted stream in configure_streams call
    for(auto it = mStreamMap.begin(); it != mStreamMap.end();) {
        int id = it->first;
        bool found = false;
        for (const auto& stream : cfg.streams) {
            if (id == stream.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Unmap all buffers of deleted stream
            // in case the configuration call succeeds and HAL
            // is able to release the corresponding resources too.
            if (!mFreeBufEarly) {
                cleanupBuffersLocked(id);
            }
            it = mStreamMap.erase(it);
        } else {
            ++it;
        }
    }

    const size_t nStreams = cfg.streams.size();
    LOG_ALWAYS_FATAL_IF(halStreams.size() != nStreams);
    // if(cfg.sessionParams.metadata.size() ==0 ){
    //     ALOGE("%s: sessionParams.metadata size invalid!", __FUNCTION__);
    //     return toScopedAStatus(FAILURE(Status::ILLEGAL_ARGUMENT));
    // }
    if (mHwCamera.configure(cfg.sessionParams, nStreams,
                            cfg.streams.data(), halStreams.data())) {
        mStreamBufferCache.clearStreamInfo();
        for (uint32_t i = 0; i < stream_list.num_streams; i++) {
            camera3_stream_t* stream = streams[i];
            halStreams[i].producerUsage =  static_cast<BufferUsage>(stream->usage |
                          RK_GRALLOC_USAGE_RANGE_FULL | RK_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601);
            ALOGD("stream:%d %dx%d priv:%p",i,stream->width,stream->height,stream->priv);
        }

        *halStreamsOut = std::move(halStreams);
        mFirstRequest = true;
        return ScopedAStatus::ok();
    } else {
        return toScopedAStatus(FAILURE(Status::INTERNAL_ERROR));
    }
}

ScopedAStatus CameraDeviceSession::constructDefaultRequestSettings(
        const RequestTemplate tpl,
        CameraMetadata* metadata) {
#if 0
    auto maybeMetadata = serializeCameraMetadataMap(
        mParent->constructDefaultRequestSettings(tpl));

    if (maybeMetadata) {
        *metadata = std::move(maybeMetadata.value());
        return ScopedAStatus::ok();
    } else {
        return toScopedAStatus(Status::INTERNAL_ERROR);
    }
#endif
    const camera_metadata_t *rawRequest;
    int type = (int) tpl;
    ATRACE_BEGIN("camera3->construct_default_request_settings");
    rawRequest = mDevice->ops->construct_default_request_settings(mDevice, (int) type);
    ATRACE_END();
    if (rawRequest == nullptr) {
        ALOGI("%s: template %d is not supported on this camera device",
                __FUNCTION__, type);
        return toScopedAStatus(Status::ILLEGAL_ARGUMENT);
    } else {
        mOverridenRequest.clear();
        mOverridenRequest.append(rawRequest);
        // Derive some new keys for backward compatibility
        if (mDerivePostRawSensKey && !mOverridenRequest.exists(
                ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST)) {
            int32_t defaultBoost[1] = {100};
            mOverridenRequest.update(
                    ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
                    defaultBoost, 1);
        }
        const camera_metadata_t *metaBuffer =
                mOverridenRequest.getAndLock();
        convertToAidl(metaBuffer, metadata);
        mOverridenRequest.unlock(metaBuffer);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDeviceSession::flush() {
    ALOGE("%s",__FUNCTION__);
    flushImpl(std::chrono::steady_clock::now());
    // Flush is always supported on device 3.1 or later
    status_t ret = mDevice->ops->flush(mDevice);
    if (ret != OK) {
        return toScopedAStatus(Status::INTERNAL_ERROR);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDeviceSession::getCaptureRequestMetadataQueue(
        MQDescriptor<int8_t, SynchronizedReadWrite>* desc) {
    *desc = mRequestQueue.dupeDesc();
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDeviceSession::getCaptureResultMetadataQueue(
        MQDescriptor<int8_t, SynchronizedReadWrite>* desc) {
    *desc = mResultQueue.dupeDesc();
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDeviceSession::isReconfigurationRequired(
        const CameraMetadata& /*oldParams*/,
        const CameraMetadata& /*newParams*/,
        bool* resultOut) {
    *resultOut = false;
    return ScopedAStatus::ok();
}
bool CameraDeviceSession::preProcessConfigurationLocked(
        const StreamConfiguration& requestedConfiguration,
        camera3_stream_configuration_t *stream_list /*out*/,
        std::vector<camera3_stream_t*> *streams /*out*/) {

    if ((stream_list == nullptr) || (streams == nullptr)) {
        return false;
    }

    stream_list->operation_mode = (uint32_t) requestedConfiguration.operationMode;
    stream_list->num_streams = requestedConfiguration.streams.size();
    streams->resize(stream_list->num_streams);
    stream_list->streams = streams->data();

    for (uint32_t i = 0; i < stream_list->num_streams; i++) {
        int id = requestedConfiguration.streams[i].id;

        if (mStreamMap.count(id) == 0) {
            Camera3Stream stream;
            convertFromAidl(requestedConfiguration.streams[i], &stream);
            mStreamMap[id] = stream;
            mStreamMap[id].data_space = mapToLegacyDataspace(
                    mStreamMap[id].data_space);
            mCirculatingBuffers.emplace(stream.mId, CirculatingBuffers{});
        } else {
            // width/height/format must not change, but usage/rotation might need to change
            if (mStreamMap[id].stream_type !=
                    (int) requestedConfiguration.streams[i].streamType ||
                    mStreamMap[id].width != requestedConfiguration.streams[i].width ||
                    mStreamMap[id].height != requestedConfiguration.streams[i].height ||
                    mStreamMap[id].format != (int) requestedConfiguration.streams[i].format ||
                    mStreamMap[id].data_space !=
                            mapToLegacyDataspace( static_cast<android_dataspace_t> (
                                    requestedConfiguration.streams[i].dataSpace))) {
                ALOGE("%s: stream %d configuration changed!", __FUNCTION__, id);
                return false;
            }
            mStreamMap[id].rotation = (int) requestedConfiguration.streams[i].rotation;
            mStreamMap[id].usage = (uint32_t) requestedConfiguration.streams[i].usage;
        }
        (*streams)[i] = &mStreamMap[id];
    }

    if (mFreeBufEarly) {
        // Remove buffers of deleted streams
        for(auto it = mStreamMap.begin(); it != mStreamMap.end(); it++) {
            int id = it->first;
            bool found = false;
            for (const auto& stream : requestedConfiguration.streams) {
                if (id == stream.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Unmap all buffers of deleted stream
                cleanupBuffersLocked(id);
            }
        }
    }

    return true;
}

void CameraDeviceSession::postProcessConfigurationLocked(
        const StreamConfiguration& requestedConfiguration) {
    // delete unused streams, note we do this after adding new streams to ensure new stream
    // will not have the same address as deleted stream, and HAL has a chance to reference
    // the to be deleted stream in configure_streams call
    for(auto it = mStreamMap.begin(); it != mStreamMap.end();) {
        int id = it->first;
        bool found = false;
        for (const auto& stream : requestedConfiguration.streams) {
            if (id == stream.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Unmap all buffers of deleted stream
            // in case the configuration call succeeds and HAL
            // is able to release the corresponding resources too.
            if (!mFreeBufEarly) {
                cleanupBuffersLocked(id);
            }
            it = mStreamMap.erase(it);
        } else {
            ++it;
        }
    }

    // Track video streams
    mVideoStreamIds.clear();
    for (const auto& stream : requestedConfiguration.streams) {
        if (stream.streamType == StreamType::OUTPUT &&
            (uint32_t)stream.usage &(uint32_t)
                graphics::common::V1_0::BufferUsage::VIDEO_ENCODER) {
            mVideoStreamIds.push_back(stream.id);
        }
    }
}

ScopedAStatus CameraDeviceSession::processCaptureRequest(
        const std::vector<CaptureRequest>& requests,
        const std::vector<BufferCache>& cachesToRemove,
        int32_t* countOut) {
    updateBufferCaches(cachesToRemove);
    for (const BufferCache& bc : cachesToRemove) {
        mStreamBufferCache.remove(bc.bufferId);
    }

    int count = 0;
    for (const CaptureRequest& r : requests) {
        const Status s = processOneCaptureRequest(r);
        if (s == Status::OK) {
            ++count;
        } else {
            *countOut = count;
            return toScopedAStatus(s);
        }
    }


    *countOut = count;
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDeviceSession::signalStreamFlush(
        const std::vector<int32_t>& /*streamIds*/,
        const int32_t /*streamConfigCounter*/) {
    return toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
}

ScopedAStatus CameraDeviceSession::switchToOffline(
        const std::vector<int32_t>& /*streamsToKeep*/,
        CameraOfflineSessionInfo* /*offlineSessionInfo*/,
        std::shared_ptr<ICameraOfflineSession>* /*session*/) {
    return toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
}

ScopedAStatus CameraDeviceSession::repeatingRequestEnd(
        const int32_t /*frameNumber*/,
        const std::vector<int32_t>& /*streamIds*/) {
    return ScopedAStatus::ok();
}

bool CameraDeviceSession::isStreamCombinationSupported(const StreamConfiguration& cfg,
                                                       hw::HwCamera& hwCamera) {
    const auto [status, unused] = configureStreamsStatic(cfg, hwCamera);
    return status == Status::OK;
}

void CameraDeviceSession::closeImpl() {
    flushImpl(std::chrono::steady_clock::now());

        {
            Mutex::Autolock _l(mInflightLock);
            if (!mInflightBuffers.empty()) {
                ALOGE("%s: trying to close while there are still %zu inflight buffers!",
                        __FUNCTION__, mInflightBuffers.size());
            }
            if (!mInflightAETriggerOverrides.empty()) {
                ALOGE("%s: trying to close while there are still %zu inflight "
                        "trigger overrides!", __FUNCTION__,
                        mInflightAETriggerOverrides.size());
            }
            if (!mInflightRawBoostPresent.empty()) {
                ALOGE("%s: trying to close while there are still %zu inflight "
                        " RAW boost overrides!", __FUNCTION__,
                        mInflightRawBoostPresent.size());
            }

        }
    ATRACE_BEGIN("camera3->close");
    mDevice->common.close(&mDevice->common);
    ATRACE_END();
    mHwCamera.close();
     // free all imported buffers
    Mutex::Autolock _l(mInflightLock);
    for(auto& pair : mCirculatingBuffers) {
        CirculatingBuffers& buffers = pair.second;
        for (auto& p2 : buffers) {
            sHandleImporter.freeBuffer(p2.second);
        }
        buffers.clear();
    }
    mCirculatingBuffers.clear();
    for(auto [streamId,bufferMap]: mMapReqOutputBuffers){
	for(auto [bufferId,buffer]: bufferMap){
	    ALOGD("free output streamId:%d,bufferId:%d",streamId,bufferId);
	    native_handle_t* nh= (native_handle_t*)(buffer);
	    native_handle_delete(nh);
	}
    }
    mMapReqOutputBuffers.clear();
    for(auto [streamId,bufferMap]: mMapReqInputBuffers){
	for(auto [bufferId,buffer]: bufferMap){
	    ALOGD("free input streamId:%d,bufferId:%d",streamId,bufferId);
	    native_handle_t* nh= (native_handle_t*)(buffer);
	    native_handle_delete(nh);
	}
    }
    mMapReqInputBuffers.clear();
}

void CameraDeviceSession::flushImpl(const std::chrono::steady_clock::time_point start) {
    mFlushing = true;
    waitFlushingDone(start);
    mFlushing = false;
}

int CameraDeviceSession::waitFlushingDone(const std::chrono::steady_clock::time_point start) {
    std::unique_lock<std::mutex> lock(mNumBuffersInFlightMtx);
    if (mNumBuffersInFlight == 0) {
        return 0;
    }

    using namespace std::chrono_literals;
    constexpr int kRecommendedDeadlineMs = 100;
    constexpr int kFatalDeadlineMs = 1000;
    const auto fatalDeadline = start + (1ms * kFatalDeadlineMs);

    const auto checkIfNoBuffersInFlight = [this](){ return mNumBuffersInFlight == 0; };

    if (mNoBuffersInFlight.wait_until(lock, fatalDeadline, checkIfNoBuffersInFlight)) {
        const int waitedForMs = (std::chrono::steady_clock::now() - start) / 1ms;

        if (waitedForMs > kRecommendedDeadlineMs) {
            ALOGW("%s:%s:%d: flushing took %dms, Android "
                  "recommends %dms latency and requires no more than %dms",
                  kClass, __func__, __LINE__, waitedForMs, kRecommendedDeadlineMs,
                  kFatalDeadlineMs);
        }
        return waitedForMs;
    } else {
        LOG_ALWAYS_FATAL("%s:%s:%d: %zu buffers are still in "
                         "flight after %dms of waiting, some buffers might have "
                         "leaked", kClass, __func__, __LINE__, mNumBuffersInFlight,
                         kFatalDeadlineMs);
    }
}

std::pair<Status, std::vector<HalStream>>
CameraDeviceSession::configureStreamsStatic(const StreamConfiguration& cfg,
                                            hw::HwCamera& hwCamera) {
    if (cfg.multiResolutionInputImage) {
        return {FAILURE(Status::OPERATION_NOT_SUPPORTED), {}};
    }

    const size_t streamsSize = cfg.streams.size();
    if (!streamsSize) {
        return {FAILURE(Status::ILLEGAL_ARGUMENT), {}};
    }

    std::vector<HalStream> halStreams;
    halStreams.reserve(streamsSize);

    for (const auto& s : cfg.streams) {
        if (s.streamType == StreamType::INPUT) {
            return {FAILURE(Status::OPERATION_NOT_SUPPORTED), {}};
        }

        if (s.width <= 0) {
            return {FAILURE(Status::ILLEGAL_ARGUMENT), {}};
        }

        if (s.height <= 0) {
            return {FAILURE(Status::ILLEGAL_ARGUMENT), {}};
        }

        if (s.rotation != StreamRotation::ROTATION_0) {
            return {FAILURE(Status::ILLEGAL_ARGUMENT), {}};
        }

        if (s.bufferSize < 0) {
            return {FAILURE(Status::ILLEGAL_ARGUMENT), {}};
        }



        HalStream hs;
        std::tie(hs.overrideFormat, hs.producerUsage,
                 hs.overrideDataSpace, hs.maxBuffers) =
            hwCamera.overrideStreamParams(s.format, s.usage, s.dataSpace);

        if (hs.maxBuffers <= 0) {
            switch (hs.maxBuffers) {
            case hw::HwCamera::kErrorBadFormat:
                ALOGE("%s:%s:%d unexpected format=0x%" PRIx32,
                      kClass, __func__, __LINE__, static_cast<uint32_t>(s.format));
                return {Status::ILLEGAL_ARGUMENT, {}};

            case hw::HwCamera::kErrorBadUsage:
                ALOGE("%s:%s:%d unexpected usage=0x%" PRIx64
                      " for format=0x%" PRIx32 " and dataSpace=0x%" PRIx32,
                      kClass, __func__, __LINE__, static_cast<uint64_t>(s.usage),
                      static_cast<uint32_t>(s.format),
                      static_cast<uint32_t>(s.dataSpace));
                return {Status::ILLEGAL_ARGUMENT, {}};

            case hw::HwCamera::kErrorBadDataspace:
                ALOGE("%s:%s:%d unexpected dataSpace=0x%" PRIx32
                      " for format=0x%" PRIx32 " and usage=0x%" PRIx64,
                      kClass, __func__, __LINE__, static_cast<uint32_t>(s.dataSpace),
                      static_cast<uint32_t>(s.format),
                      static_cast<uint64_t>(s.usage));
                return {Status::ILLEGAL_ARGUMENT, {}};

            default:
                ALOGE("%s:%s:%d something is not right for format=0x%" PRIx32
                      " usage=0x%" PRIx64 " and dataSpace=0x%" PRIx32,
                      kClass, __func__, __LINE__, static_cast<uint32_t>(s.format),
                      static_cast<uint64_t>(s.usage),
                      static_cast<uint32_t>(s.dataSpace));
                return {Status::ILLEGAL_ARGUMENT, {}};
            }
        }

        hs.id = s.id;
        hs.consumerUsage = static_cast<BufferUsage>(0);
        hs.physicalCameraId = s.physicalCameraId;
        hs.supportOffline = false;

        halStreams.push_back(std::move(hs));
    }

    return {Status::OK, std::move(halStreams)};
}

Status CameraDeviceSession::processOneCaptureRequest(const CaptureRequest& request) {
    // ALOGD("%s,request.frameNumber:%d",__FUNCTION__,request.frameNumber);
    // If inputBuffer is valid, the request is for reprocessing
    if (!isAidlNativeHandleEmpty(request.inputBuffer.buffer)) {
        return FAILURE(Status::OPERATION_NOT_SUPPORTED);
    }

    if (request.inputWidth || request.inputHeight) {
        return FAILURE(Status::OPERATION_NOT_SUPPORTED);
    }

    if (!request.physicalCameraSettings.empty()) {
        return FAILURE(Status::OPERATION_NOT_SUPPORTED);
    }

    const size_t outputBuffersSize = request.outputBuffers.size();

    if (outputBuffersSize == 0) {
        ALOGE("%s: capture request must have at least one output buffer!", __FUNCTION__);
        return FAILURE(Status::ILLEGAL_ARGUMENT);
    }

    for (size_t i = 0; i < outputBuffersSize; ++i) {
        if (request.outputBuffers[i].bufferId <= 0)
        {
            ALOGE("%s invalid output buffer bufferId:%d",__FUNCTION__,(int)request.outputBuffers[i].bufferId);
            return FAILURE(Status::ILLEGAL_ARGUMENT);
        }
    }


    HwCaptureRequest hwReq;
    camera3_capture_request_t halRequest;

    bool converted = true;

    if (request.fmqSettingsSize < 0) {
        return FAILURE(Status::ILLEGAL_ARGUMENT);
    } else if (request.fmqSettingsSize > 0) {
        CameraMetadata tmp;
        tmp.metadata.resize(request.fmqSettingsSize);

        CameraMetadata settingsFmq;  // settings from FMQ
        settingsFmq.metadata.resize(request.fmqSettingsSize);

        mRequestMetadataQueue->read(settingsFmq.metadata.data(), request.fmqSettingsSize);

        if (mRequestQueue.read(
                reinterpret_cast<int8_t*>(tmp.metadata.data()),
                request.fmqSettingsSize)) {
            hwReq.metadataUpdate = std::move(tmp);
            converted = convertFromAidl(hwReq.metadataUpdate, &halRequest.settings);
        } else {
            return FAILURE(Status::INTERNAL_ERROR);
        }
    } else if (!request.settings.metadata.empty()) {
        hwReq.metadataUpdate = request.settings;
        converted = convertFromAidl(hwReq.metadataUpdate, &halRequest.settings);
    }

    if (!converted) {
        ALOGE("%s: capture request settings metadata is corrupt!", __FUNCTION__);
        return FAILURE(Status::INTERNAL_ERROR);
    }

    if(mFirstRequest && halRequest.settings == nullptr){
        ALOGE("%s: capture request settings must not be null for first request!", __FUNCTION__);
        return FAILURE(Status::ILLEGAL_ARGUMENT);
    }
    std::vector<buffer_handle_t*> allBufPtrs;
    std::vector<int> allFences;
    bool hasInputBuf = (request.inputBuffer.streamId != -1 &&
            request.inputBuffer.bufferId != 0);
    size_t numOutputBufs = request.outputBuffers.size();
    size_t numBufs = numOutputBufs + (hasInputBuf ? 1 : 0);

    if (numOutputBufs == 0) {
        ALOGE("%s: capture request must have at least one output buffer!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    Status status = importRequest(request, allBufPtrs, allFences);
    if (status != Status::OK) {
        return status;
    }

    std::vector<camera3_stream_buffer_t> outHalBufs;
    outHalBufs.resize(outputBuffersSize);
    hwReq.buffers.resize(outputBuffersSize);

    for (size_t i = 0; i < outputBuffersSize; ++i) {
        // ALOGD(" request.outputBuffers[%d].bufferId:%d streamId:%d",i, request.outputBuffers[i].bufferId,
        // request.outputBuffers[i].streamId);
        hwReq.buffers[i] = mStreamBufferCache.update(request.outputBuffers[i]);
    }
    // ALOGD("%s mNumBuffersInFlightMtx mNumBuffersInFlight:%d request.frameNumber:%d",__FUNCTION__,mNumBuffersInFlight,request.frameNumber);
    {
        std::lock_guard<std::mutex> guard(mNumBuffersInFlightMtx);
        mNumBuffersInFlight += outputBuffersSize;
    }
    // ALOGD("%s mNumBuffersInFlightMtx done mNumBuffersInFlight:%d request.frameNumber:%d",__FUNCTION__,mNumBuffersInFlight,request.frameNumber);

    bool aeCancelTriggerNeeded = false;
    ::android::hardware::camera::common::V1_0::helper::CameraMetadata settingsOverride;
    {
        Mutex::Autolock _l(mInflightLock);
        if (hasInputBuf) {
            // auto key = std::make_pair(request.inputBuffer.streamId, request.frameNumber);
            // auto& bufCache = mInflightBuffers[key] = camera3_stream_buffer_t{};
            // convertFromHidl(
            //         allBufPtrs[numOutputBufs], request.inputBuffer.status,
            //         &mStreamMap[request.inputBuffer.streamId], allFences[numOutputBufs],
            //         &bufCache);
            // halRequest.input_buffer = &bufCache;
        } else {
            halRequest.input_buffer = nullptr;
        }

        halRequest.num_output_buffers = numOutputBufs;
        for (size_t i = 0; i < numOutputBufs; i++) {
            auto key = std::make_pair(request.outputBuffers[i].streamId, request.frameNumber);
            //auto  csb =  hwReq.buffers[i];

            //const native_handle_t* handle = allBufPtrs[i];//csb->getBuffer();
            camera3_stream_buffer_t bufCache =  mInflightBuffers[key] = camera3_stream_buffer_t();
            bufCache.stream = &mStreamMap[request.outputBuffers[i].streamId];
            bufCache.buffer =  allBufPtrs[i];
            bufCache.status = (int) request.outputBuffers[i].status;
            bufCache.acquire_fence = allFences[i];//csb->getAcquireFence();
            bufCache.release_fence = -1; // meant for HAL to fill in

            
            // convertFromHidl(
            //         allBufPtrs[i], request.outputBuffers[i].status,
            //         &mStreamMap[request.outputBuffers[i].streamId], allFences[i],
            //         &bufCache);
            outHalBufs[i] = bufCache;
        }
        halRequest.output_buffers = outHalBufs.data();

        AETriggerCancelOverride triggerOverride;
        aeCancelTriggerNeeded = handleAePrecaptureCancelRequestLocked(
                halRequest, &settingsOverride /*out*/, &triggerOverride/*out*/);
        if (aeCancelTriggerNeeded) {
            mInflightAETriggerOverrides[halRequest.frame_number] =
                    triggerOverride;
            halRequest.settings = settingsOverride.getAndLock();
        }
    }
    halRequest.num_physcam_settings = 0;

    hwReq.frameNumber = request.frameNumber;
    halRequest.frame_number = request.frameNumber;
    {
        Mutex::Autolock _l(mInflightRequestLock);
        mInflightRequest[request.frameNumber] = hwReq;
    }
    // ALOGD("%s process_capture_request",__FUNCTION__);
    ATRACE_ASYNC_BEGIN("frame capture", request.frameNumber);
    ATRACE_BEGIN("camera3->process_capture_request");
    status_t ret = mDevice->ops->process_capture_request(mDevice, &halRequest);
    ATRACE_END();
    // ALOGD("%s process_capture_request done",__FUNCTION__);
    if (aeCancelTriggerNeeded) {
        settingsOverride.unlock(halRequest.settings);
    }
    if (ret != OK) {
        Mutex::Autolock _l(mInflightLock);
        ALOGE("%s: HAL process_capture_request call failed!", __FUNCTION__);

        cleanupInflightFences(allFences, numBufs);
        if (hasInputBuf) {
            auto key = std::make_pair(request.inputBuffer.streamId, request.frameNumber);
            mInflightBuffers.erase(key);
        }
        for (size_t i = 0; i < numOutputBufs; i++) {
            auto key = std::make_pair(request.outputBuffers[i].streamId, request.frameNumber);
            mInflightBuffers.erase(key);
        }
        if (aeCancelTriggerNeeded) {
            mInflightAETriggerOverrides.erase(request.frameNumber);
        }
        return FAILURE(Status::INTERNAL_ERROR);
    }

    // mFirstRequest = false;
    // return Status::OK;
    if (mCaptureRequests.put(&hwReq)) {
        mFirstRequest = false;
        // ALOGD("%s put hwReq.frameNumber:%d hwReq.buffers:%d",__FUNCTION__,hwReq.frameNumber,hwReq.buffers.size());
        return Status::OK;
    } else {
        disposeCaptureRequest(std::move(hwReq));
        return FAILURE(Status::INTERNAL_ERROR);
    }
}

void CameraDeviceSession::captureThreadLoop() {
    setThreadPriority(SP_FOREGROUND, ANDROID_PRIORITY_VIDEO);

    struct timespec nextFrameT;
    clock_gettime(CLOCK_MONOTONIC, &nextFrameT);
    while (true) {
        std::optional<HwCaptureRequest> maybeReq = mCaptureRequests.get();
        if (maybeReq.has_value()) {
            HwCaptureRequest& req = maybeReq.value();
            if (mFlushing) {
                disposeCaptureRequest(std::move(req));
            } else {
                nextFrameT = captureOneFrame(nextFrameT, std::move(req));
            }
        } else {
            break;
        }
    }
}

struct timespec CameraDeviceSession::captureOneFrame(struct timespec nextFrameT,
                                                     HwCaptureRequest req) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (std::make_pair(now.tv_sec, now.tv_nsec) <
            std::make_pair(nextFrameT.tv_sec, nextFrameT.tv_nsec)) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextFrameT, nullptr);
    } else {
        nextFrameT = now;
    }

    const int32_t frameNumber = req.frameNumber;

    const int64_t shutterTimestampNs = timespec2nanos(nextFrameT);

    notifyShutter(&*mCb, frameNumber, shutterTimestampNs);
ALOGD("%s, frameNumber:%d",__FUNCTION__,frameNumber);
    auto [frameDurationNs, metadata, outputBuffers, delayedOutputBuffers] =
        mHwCamera.processCaptureRequest(std::move(req.metadataUpdate),
                                        {req.buffers.begin(), req.buffers.end()});

    for (hw::DelayedStreamBuffer& dsb : delayedOutputBuffers) {
        DelayedCaptureResult dcr;
        dcr.delayedBuffer = std::move(dsb);
        dcr.frameNumber = frameNumber;
        if (!mDelayedCaptureResults.put(&dcr)) {
            // `delayedBuffer(false)` only releases the buffer (fast).
            outputBuffers.push_back(dcr.delayedBuffer(false));
        }
    }

    metadataSetShutterTimestamp(&metadata, shutterTimestampNs);
    consumeCaptureResult(makeCaptureResult(frameNumber,
        std::move(metadata), std::move(outputBuffers)));

    if (frameDurationNs > 0) {
        nextFrameT = timespecAddNanos(nextFrameT, frameDurationNs);
    } else {
        notifyError(&*mCb, frameNumber, -1, ErrorCode::ERROR_DEVICE);
    }

    return nextFrameT;
}

void CameraDeviceSession::delayedCaptureThreadLoop() {
    while (true) {
        std::optional<DelayedCaptureResult> maybeDCR = mDelayedCaptureResults.get();
        if (maybeDCR.has_value()) {
            const DelayedCaptureResult& dcr = maybeDCR.value();

            // `dcr.delayedBuffer(true)` is expected to be slow, so we do not
            // produce too much IPC traffic here. This also returns buffes to
            // the framework earlier to reuse in capture requests.
            std::vector<StreamBuffer> outputBuffers(1);
            outputBuffers.front() = dcr.delayedBuffer(!mFlushing);
            consumeCaptureResult(makeCaptureResult(dcr.frameNumber,
                {}, std::move(outputBuffers)));
        } else {
            break;
        }
    }
}

void CameraDeviceSession::disposeCaptureRequest(HwCaptureRequest req) {
    notifyError(&*mCb, req.frameNumber, -1, ErrorCode::ERROR_REQUEST);

    const size_t reqBuffersSize = req.buffers.size();

    {
        std::vector<StreamBuffer> outputBuffers(reqBuffersSize);

        for (size_t i = 0; i < reqBuffersSize; ++i) {
            CachedStreamBuffer* csb = req.buffers[i];
            LOG_ALWAYS_FATAL_IF(!csb);  // otherwise mNumBuffersInFlight will be hard
            outputBuffers[i] = csb->finish(false);
        }

        std::vector<CaptureResult> crs(1);
        crs.front() = makeCaptureResult(req.frameNumber, {},
                                        std::move(outputBuffers));

        std::lock_guard<std::mutex> guard(mResultQueueMutex);
        mCb->processCaptureResult(std::move(crs));
    }
    ALOGD("%s notifyBuffersReturned",__FUNCTION__);
    notifyBuffersReturned(reqBuffersSize);
}

void CameraDeviceSession::consumeCaptureResult(CaptureResult cr) {
    const size_t numBuffers = cr.outputBuffers.size();

    {
        std::lock_guard<std::mutex> guard(mResultQueueMutex);
        const size_t metadataSize = cr.result.metadata.size();
        if ((metadataSize > 0) && mResultQueue.write(
                reinterpret_cast<int8_t*>(cr.result.metadata.data()),
                metadataSize)) {
            cr.fmqResultSize = metadataSize;
            cr.result.metadata.clear();
        }

        std::vector<CaptureResult> crs(1);
        crs.front() = std::move(cr);
        mCb->processCaptureResult(std::move(crs));
    }
    //ALOGD("%s frameNumber:%d notifyBuffersReturned numBuffers:%d mNumBuffersInFlight:%d",__FUNCTION__,cr.frameNumber,numBuffers,mNumBuffersInFlight);
    notifyBuffersReturned(numBuffers);
    //ALOGD("%s frameNumber:%d notifyBuffersReturned numBuffers:%d mNumBuffersInFlight:%d done",__FUNCTION__,cr.frameNumber,numBuffers,mNumBuffersInFlight);
}

void CameraDeviceSession::notifyBuffersReturned(const size_t numBuffersToReturn) {
    std::lock_guard<std::mutex> guard(mNumBuffersInFlightMtx);
    // LOG_ALWAYS_FATAL_IF(mNumBuffersInFlight < numBuffersToReturn,
    //                     "mNumBuffersInFlight=%zu numBuffersToReturn=%zu",
    //                     mNumBuffersInFlight, numBuffersToReturn);
    if (mNumBuffersInFlight < numBuffersToReturn )
    {
        ALOGD("%s mNumBuffersInFlight=%zu < numBuffersToReturn=%zu reset mNumBuffersInFlight to Zero",__FUNCTION__,
        mNumBuffersInFlight, numBuffersToReturn);
        mNumBuffersInFlight = 0;
    }else{
        mNumBuffersInFlight -= numBuffersToReturn;
    }

    if (mNumBuffersInFlight == 0) {
        mNoBuffersInFlight.notify_all();
    }
}

uint64_t CameraDeviceSession::getCapResultBufferId(const buffer_handle_t&, int) {
    // No need to fill in bufferId by default
    return BUFFER_ID_NO_BUFFER;
}

status_t CameraDeviceSession::constructCaptureResult(CaptureResult& result,
                                                 const camera3_capture_result *hal_result) {
    uint32_t frameNumber = hal_result->frame_number;
    bool hasInputBuf = (hal_result->input_buffer != nullptr);
    size_t numOutputBufs = hal_result->num_output_buffers;
    size_t numBufs = numOutputBufs + (hasInputBuf ? 1 : 0);
    if (numBufs > 0) {
        Mutex::Autolock _l(mInflightLock);
        if (hasInputBuf) {
            int streamId = static_cast<Camera3Stream*>(hal_result->input_buffer->stream)->mId;
            // validate if buffer is inflight
            auto key = std::make_pair(streamId, frameNumber);
            if (mInflightBuffers.count(key) != 1) {
                ALOGE("%s: input buffer for stream %d frame %d is not inflight!",
                        __FUNCTION__, streamId, frameNumber);
                return -EINVAL;
            }
        }

        for (size_t i = 0; i < numOutputBufs; i++) {
            int streamId = static_cast<Camera3Stream*>(hal_result->output_buffers[i].stream)->mId;
            // validate if buffer is inflight
            auto key = std::make_pair(streamId, frameNumber);
            if (mInflightBuffers.count(key) != 1) {
                ALOGE("%s: output buffer for stream %d frame %d is not inflight!",
                        __FUNCTION__, streamId, frameNumber);
                return -EINVAL;
            }
        }
    }
    // We don't need to validate/import fences here since we will be passing them to camera service
    // within the scope of this function
    result.frameNumber = frameNumber;
    result.fmqResultSize = 0;
    result.partialResult = hal_result->partial_result;
    convertToAidl(hal_result->result, &result.result);
    if (nullptr != hal_result->result) {
        bool resultOverriden = false;
        Mutex::Autolock _l(mInflightLock);

        // Derive some new keys for backward compatibility
        if (mDerivePostRawSensKey) {
            camera_metadata_ro_entry entry;
            if (find_camera_metadata_ro_entry(hal_result->result,
                    ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST, &entry) == 0) {
                mInflightRawBoostPresent[frameNumber] = true;
            } else {
                auto entry = mInflightRawBoostPresent.find(frameNumber);
                if (mInflightRawBoostPresent.end() == entry) {
                    mInflightRawBoostPresent[frameNumber] = false;
                }
            }

            if ((hal_result->partial_result == mNumPartialResults)) {
                if (!mInflightRawBoostPresent[frameNumber]) {
                    if (!resultOverriden) {
                        mOverridenResult.clear();
                        mOverridenResult.append(hal_result->result);
                        resultOverriden = true;
                    }
                    int32_t defaultBoost[1] = {100};
                    mOverridenResult.update(
                            ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
                            defaultBoost, 1);
                }

                mInflightRawBoostPresent.erase(frameNumber);
            }
        }

        auto entry = mInflightAETriggerOverrides.find(frameNumber);
        if (mInflightAETriggerOverrides.end() != entry) {
            if (!resultOverriden) {
                mOverridenResult.clear();
                mOverridenResult.append(hal_result->result);
                resultOverriden = true;
            }
            overrideResultForPrecaptureCancelLocked(entry->second,
                    &mOverridenResult);
            if (hal_result->partial_result == mNumPartialResults) {
                mInflightAETriggerOverrides.erase(frameNumber);
            }
        }

        if (resultOverriden) {
            const camera_metadata_t *metaBuffer =
                    mOverridenResult.getAndLock();
            convertToAidl(metaBuffer, &result.result);
            mOverridenResult.unlock(metaBuffer);
        }
    }
    if (hasInputBuf) {
        result.inputBuffer.streamId =
                static_cast<Camera3Stream*>(hal_result->input_buffer->stream)->mId;
        result.inputBuffer.buffer = NativeHandle();
        result.inputBuffer.status = (BufferStatus) hal_result->input_buffer->status;
        // skip acquire fence since it's no use to camera service
        if (hal_result->input_buffer->release_fence != -1) {
            result.inputBuffer.releaseFence.fds.resize(1);
            result.inputBuffer.releaseFence.fds.at(0).set( hal_result->input_buffer->release_fence);
        } else {
            result.inputBuffer.releaseFence = NativeHandle();
        }
    } else {
        result.inputBuffer.streamId = -1;
    }

    result.outputBuffers.resize(numOutputBufs);
    for (size_t i = 0; i < numOutputBufs; i++) {
        result.outputBuffers[i].streamId =
                static_cast<Camera3Stream*>(hal_result->output_buffers[i].stream)->mId;
        result.outputBuffers[i].buffer = NativeHandle();;
        if (hal_result->output_buffers[i].buffer != nullptr) {
            result.outputBuffers[i].bufferId = getCapResultBufferId(
                    *(hal_result->output_buffers[i].buffer),
                    result.outputBuffers[i].streamId);
        } else {
            result.outputBuffers[i].bufferId = 0;
        }

        result.outputBuffers[i].status = (BufferStatus) hal_result->output_buffers[i].status;
        // skip acquire fence since it's of no use to camera service
        if (hal_result->output_buffers[i].release_fence != -1) {
            result.outputBuffers[i].releaseFence.fds.resize(1);
            result.outputBuffers[i].releaseFence.fds.at(0).set(hal_result->output_buffers[i].release_fence);
        } else {
            result.outputBuffers[i].releaseFence = NativeHandle();
        }
    }

    // Free inflight record/fences.
    // Do this before call back to camera service because camera service might jump to
    // configure_streams right after the processCaptureResult call so we need to finish
    // updating inflight queues first

    if (numBufs > 0) {
        Mutex::Autolock _l(mInflightLock);
        if (hasInputBuf) {
            int streamId = static_cast<Camera3Stream*>(hal_result->input_buffer->stream)->mId;
            auto key = std::make_pair(streamId, frameNumber);
            if (mInflightBuffers.count(key)) {
                mInflightBuffers.erase(key);
            }
        }

        for (size_t i = 0; i < numOutputBufs; i++) {
            int streamId = static_cast<Camera3Stream*>(hal_result->output_buffers[i].stream)->mId;
            auto key = std::make_pair(streamId, frameNumber);
            if (mInflightBuffers.count(key)) {
                mInflightBuffers.erase(key);
            }
        }

        if (mInflightBuffers.empty()) {
            ALOGV("%s: inflight buffer queue is now empty!", __FUNCTION__);
        }
    }
    return OK;
}
void CameraDeviceSession::processCaptureResult(
        const camera3_callback_ops *cb,
        const camera3_capture_result *hal_result){
    CameraMetadata metadata;
    ::android::hardware::camera::common::V1_0::helper::CameraMetadata settingsTmp;
    camera_metadata_t* partialMetadata =
                reinterpret_cast<camera_metadata_t*>((void*)hal_result->result);
    int streamId = hal_result->num_output_buffers> 0
    ? static_cast<Camera3Stream*>(hal_result->output_buffers[0].stream)->mId : -1;
    int format = streamId >= 0 ?mStreamMap[streamId].format: -1;

    if(hal_result->result!=nullptr){
        camera_metadata_ro_entry exposureTimeResult;
        exposureTimeResult.tag = ANDROID_SENSOR_EXPOSURE_TIME;
        find_camera_metadata_ro_entry(hal_result->result, ANDROID_SENSOR_EXPOSURE_TIME, &exposureTimeResult);
        Mutex::Autolock _l(mInflightExposureTimeLock);
        int64_t exposureTimeNs =mInflightExposureTimeNs[hal_result->frame_number];
        if(hal_result->frame_number == 1){
            settingsTmp = partialMetadata;
            exposureTimeNs = kDefaultSensorExposureTimeNs;
            settingsTmp.update(ANDROID_SENSOR_EXPOSURE_TIME, &exposureTimeNs, 1);
            partialMetadata = const_cast<camera_metadata_t*>(settingsTmp.getAndLock());
            metadata = metadataCompactRaw(partialMetadata);
            settingsTmp.unlock(partialMetadata);
            if (exposureTimeResult.count && exposureTimeResult.data.i64[0] > 0) {
                mSensorExposureTimeNs =   exposureTimeResult.data.i64[0];
            }
        }else{
            settingsTmp = partialMetadata;
            if (exposureTimeResult.count && exposureTimeResult.data.i64[0] > 0 &&
                exposureTimeResult.data.i64[0] !=mSensorExposureTimeNs) {
                mSensorExposureTimeNs = exposureTimeResult.data.i64[0];
                settingsTmp.update(ANDROID_SENSOR_EXPOSURE_TIME, &mSensorExposureTimeNs, 1);
            }
            partialMetadata = const_cast<camera_metadata_t*>(settingsTmp.getAndLock());
            metadata = metadataCompactRaw(partialMetadata);
            settingsTmp.unlock(partialMetadata);
        }
        if (mInflightExposureTimeNs.count(hal_result->frame_number)) {
            mInflightExposureTimeNs.erase(hal_result->frame_number);
        }
    }

    Mutex::Autolock _l(mInflightRequestLock);
    HwCaptureRequest& req = mInflightRequest[hal_result->frame_number];

    std::vector<StreamBuffer> outputBuffers = mHwCamera.getOutputStreamBuffer(req,streamId);
    req.doneBufferNumber+= outputBuffers.size();
    // ALOGD("%s outputBuffers:%d ,hal_result->num_output_buffers:%d req.doneBufferNumber:%d",__FUNCTION__,outputBuffers.size(),hal_result->num_output_buffers,req.doneBufferNumber);
    if (format == 33)
    {
        for (size_t i = 0; i < hal_result->num_output_buffers; i++) {
            if (hal_result->output_buffers[i].release_fence != -1) {
                if(format == 33){
                    ALOGD("streamId:%d sync_wait jpeg fd:%d frame_number:%d",streamId,hal_result->output_buffers[i].release_fence,hal_result->frame_number);
                    sync_wait(hal_result->output_buffers[i].release_fence, -1);
                    ALOGD("sync_wait done fd:%d frame_number:%d",hal_result->output_buffers[i].release_fence,hal_result->frame_number);
                }
            }
        }
        consumeCaptureResult(makeCaptureResult(hal_result->frame_number,
                std::move(metadata), std::move(outputBuffers)));
    }else{
        consumeCaptureResult(makeCaptureResult(hal_result->frame_number,
                std::move(metadata), std::move(outputBuffers)));
    }

    Mutex::Autolock _lc(mInflightLock);
    for (size_t i = 0; i < hal_result->num_output_buffers; i++) {
        if (hal_result->output_buffers[i].release_fence != -1) {
            native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
            handle->data[0] = hal_result->output_buffers[i].release_fence;
            // ALOGD("%s format:%d native_handle_close fd:%d frame_number:%d",
            // __FUNCTION__,format,handle->data[0],hal_result->frame_number);
            native_handle_close(handle);
            native_handle_delete(handle);
        }

        auto key = std::make_pair(streamId, hal_result->frame_number);
        if (mInflightBuffers.count(key)) {
            mInflightBuffers.erase(key);
        }
    }
    // ALOGD("%s hal_result->frame_number:%d doneBufferNumber:%d req.buffers.size():%d",__FUNCTION__,
    //     hal_result->frame_number,
    //     req.doneBufferNumber,
    //     req.buffers.size());
    if (req.doneBufferNumber == req.buffers.size())
    {
        mInflightRequest.erase(hal_result->frame_number);
    }

}


// Static helper method to copy/shrink capture result metadata sent by HAL
void CameraDeviceSession::sShrinkCaptureResult(
        camera3_capture_result* dst, const camera3_capture_result* src,
        std::vector<::android::hardware::camera::common::V1_0::helper::CameraMetadata>* mds,
        std::vector<const camera_metadata_t*>* physCamMdArray,
        bool handlePhysCam) {
    *dst = *src;
    // Reserve maximum number of entries to avoid metadata re-allocation.
    mds->reserve(1 + (handlePhysCam ? src->num_physcam_metadata : 0));
    if (sShouldShrink(src->result)) {
        mds->emplace_back(sCreateCompactCopy(src->result));
        dst->result = mds->back().getAndLock();
    }

    if (handlePhysCam) {
        // First determine if we need to create new camera_metadata_t* array
        bool needShrink = false;
        for (uint32_t i = 0; i < src->num_physcam_metadata; i++) {
            if (sShouldShrink(src->physcam_metadata[i])) {
                needShrink = true;
            }
        }

        if (!needShrink) return;

        physCamMdArray->reserve(src->num_physcam_metadata);
        dst->physcam_metadata = physCamMdArray->data();
        for (uint32_t i = 0; i < src->num_physcam_metadata; i++) {
            if (sShouldShrink(src->physcam_metadata[i])) {
                mds->emplace_back(sCreateCompactCopy(src->physcam_metadata[i]));
                dst->physcam_metadata[i] = mds->back().getAndLock();
            } else {
                dst->physcam_metadata[i] = src->physcam_metadata[i];
            }
        }
    }
}

bool CameraDeviceSession::sShouldShrink(const camera_metadata_t* md) {
    size_t compactSize = get_camera_metadata_compact_size(md);
    size_t totalSize = get_camera_metadata_size(md);
    if (totalSize >= compactSize + METADATA_SHRINK_ABS_THRESHOLD &&
            totalSize >= compactSize * METADATA_SHRINK_REL_THRESHOLD) {
        ALOGV("Camera metadata should be shrunk from %zu to %zu", totalSize, compactSize);
        return true;
    }
    return false;
}

camera_metadata_t* CameraDeviceSession::sCreateCompactCopy(const camera_metadata_t* src) {
    size_t compactSize = get_camera_metadata_compact_size(src);
    void* buffer = calloc(1, compactSize);
    if (buffer == nullptr) {
        ALOGE("%s: Allocating %zu bytes failed", __FUNCTION__, compactSize);
    }
    return copy_camera_metadata(buffer, compactSize, src);
}

/**
 * Static callback forwarding methods from HAL to instance
 */
void CameraDeviceSession::sProcessCaptureResult(
        const camera3_callback_ops *cb,
        const camera3_capture_result *hal_result) {
    // ALOGD("%s,%d",__FUNCTION__,__LINE__);
    CameraDeviceSession *d =
            const_cast<CameraDeviceSession*>(static_cast<const CameraDeviceSession*>(cb));
    

    CaptureResult result = {};
    camera3_capture_result shadowResult;
    bool handlePhysCam = false;//(d->mDeviceVersion >= CAMERA_DEVICE_API_VERSION_3_5);
    std::vector<::android::hardware::camera::common::V1_0::helper::CameraMetadata> compactMds;
    std::vector<const camera_metadata_t*> physCamMdArray;
    sShrinkCaptureResult(&shadowResult, hal_result, &compactMds, &physCamMdArray, handlePhysCam);
    d->processCaptureResult(cb, &shadowResult);
}

/**
 * Override result metadata for cancelling AE precapture trigger applied in
 * handleAePrecaptureCancelRequestLocked().
 */
void CameraDeviceSession::overrideResultForPrecaptureCancelLocked(
        const AETriggerCancelOverride &aeTriggerCancelOverride,
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata *settings /*out*/) {
    if (aeTriggerCancelOverride.applyAeLock) {
        // Only devices <= v3.2 should have this override
        assert(mDeviceVersion <= CAMERA_DEVICE_API_VERSION_3_2);
        settings->update(ANDROID_CONTROL_AE_LOCK, &aeTriggerCancelOverride.aeLock, 1);
    }

    if (aeTriggerCancelOverride.applyAePrecaptureTrigger) {
        // Only devices <= v3.2 should have this override
        assert(mDeviceVersion <= CAMERA_DEVICE_API_VERSION_3_2);
        settings->update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                &aeTriggerCancelOverride.aePrecaptureTrigger, 1);
    }
}

Status CameraDeviceSession::importBuffer(int32_t streamId,
        uint64_t bufId, buffer_handle_t buf,
        /*out*/buffer_handle_t** outBufPtr,
        bool allowEmptyBuf) {

    if (buf == nullptr && bufId == BUFFER_ID_NO_BUFFER) {
        if (allowEmptyBuf) {
            *outBufPtr = &sEmptyBuffer;
            return Status::OK;
        } else {
            ALOGE("%s: bufferId %" PRIu64 " has null buffer handle!", __FUNCTION__, bufId);
            return FAILURE(Status::ILLEGAL_ARGUMENT);
        }
    }

    Mutex::Autolock _l(mInflightLock);
    CirculatingBuffers& cbs = mCirculatingBuffers[streamId];
    if (cbs.count(bufId) == 0) {
        // Register a newly seen buffer
        buffer_handle_t importedBuf = buf;
        sHandleImporter.importBuffer(importedBuf);
        if (importedBuf == nullptr) {
            ALOGE("%s: output buffer for stream %d is invalid!", __FUNCTION__, streamId);
            return FAILURE(Status::INTERNAL_ERROR);
        } else {
            cbs[bufId] = importedBuf;
        }
    }
    *outBufPtr = &cbs[bufId];
    return Status::OK;
}

Status CameraDeviceSession::importRequest(
        const CaptureRequest& request,
        std::vector<buffer_handle_t*>& allBufPtrs,
        std::vector<int>& allFences) {
    return importRequestImpl(request, allBufPtrs, allFences);
}

Status CameraDeviceSession::importRequestImpl(
        const CaptureRequest& request,
        std::vector<buffer_handle_t*>& allBufPtrs,
        std::vector<int>& allFences,
        bool allowEmptyBuf) {
    bool hasInputBuf = (request.inputBuffer.streamId != -1 &&
            request.inputBuffer.bufferId != 0);
    size_t numOutputBufs = request.outputBuffers.size();
    size_t numBufs = numOutputBufs + (hasInputBuf ? 1 : 0);
    // Validate all I/O buffers
    std::vector<buffer_handle_t> allBufs;
    std::vector<uint64_t> allBufIds;
    allBufs.resize(numBufs);
    allBufIds.resize(numBufs);
    allBufPtrs.resize(numBufs);
    allFences.resize(numBufs);
    std::vector<int32_t> streamIds(numBufs);

    for (size_t i = 0; i < numOutputBufs; i++) {
	std::unordered_map<int,buffer_handle_t> streamBufs =  mMapReqOutputBuffers[request.outputBuffers[i].streamId];
	buffer_handle_t buf = streamBufs[request.outputBuffers[i].bufferId] ;
	if(buf != nullptr){
		allBufs[i]  = buf;
		//ALOGV("cached strimeId:%d,bufId:%d",request.outputBuffers[i].streamId,request.outputBuffers[i].bufferId);
	}else{
		ALOGD("new output strimeId:%d,bufId:%d",request.outputBuffers[i].streamId,request.outputBuffers[i].bufferId);
		allBufs[i]  = ::android::makeFromAidl(request.outputBuffers[i].buffer);
		streamBufs[request.outputBuffers[i].bufferId]  = allBufs[i] ;
		mMapReqOutputBuffers[request.outputBuffers[i].streamId] = streamBufs;
	}
        allBufIds[i] = request.outputBuffers[i].bufferId;
        allBufPtrs[i] = &allBufs[i];
        streamIds[i] = request.outputBuffers[i].streamId;
    }
    if (hasInputBuf) {
	std::unordered_map<int,buffer_handle_t> streamBufs =  mMapReqInputBuffers[request.inputBuffer.streamId];
	buffer_handle_t buf = streamBufs[request.inputBuffer.bufferId] ;
	if(buf != nullptr){
		allBufs[numOutputBufs]  = buf;
		//ALOGV("cached strimeId:%d,bufId:%d",request.inputBuffer.streamId,request.inputBuffer.bufferId);
	}else{
		ALOGD("new input strimeId:%d,bufId:%d",request.inputBuffer.streamId,request.inputBuffer.bufferId);
		allBufs[numOutputBufs]  = ::android::makeFromAidl(request.inputBuffer.buffer);
		streamBufs[request.inputBuffer.bufferId]  = allBufs[numOutputBufs] ;
		mMapReqInputBuffers[request.inputBuffer.streamId] = streamBufs;
	}
        allBufIds[numOutputBufs] = request.inputBuffer.bufferId;
        allBufPtrs[numOutputBufs] = &allBufs[numOutputBufs];
        streamIds[numOutputBufs] = request.inputBuffer.streamId;
    }

    for (size_t i = 0; i < numBufs; i++) {
        Status st = importBuffer(
                streamIds[i], allBufIds[i], allBufs[i], &allBufPtrs[i],
                // Disallow empty buf for input stream, otherwise follow
                // the allowEmptyBuf argument.
                (hasInputBuf && i == numOutputBufs) ? false : allowEmptyBuf);
        if (st != Status::OK) {
            // Detailed error logs printed in importBuffer
            return st;
        }
    }

    // All buffers are imported. Now validate output buffer acquire fences
    for (size_t i = 0; i < numOutputBufs; i++) {
        buffer_handle_t h = ::android::makeFromAidl(request.outputBuffers[i].acquireFence);
        if (!sHandleImporter.importFence(h, allFences[i])) {
            ALOGE("%s: output buffer %zu acquire fence is invalid", __FUNCTION__, i);
            cleanupInflightFences(allFences, i);
            return Status::INTERNAL_ERROR;
        }
	native_handle_t* nh= (native_handle_t*)(h);
	native_handle_delete(nh);
    }

    // Validate input buffer acquire fences
    if (hasInputBuf) {
        buffer_handle_t h = ::android::makeFromAidl(request.inputBuffer.acquireFence);
        if (!sHandleImporter.importFence(h, allFences[numOutputBufs])) {
            ALOGE("%s: input buffer acquire fence is invalid", __FUNCTION__);
            cleanupInflightFences(allFences, numOutputBufs);
            return Status::INTERNAL_ERROR;
        }
	native_handle_t* nh= (native_handle_t*)(h);
	native_handle_delete(nh);
    }
    return Status::OK;
}

void CameraDeviceSession::cleanupInflightFences(
        std::vector<int>& allFences, size_t numFences) {
    for (size_t j = 0; j < numFences; j++) {
        sHandleImporter.closeFence(allFences[j]);
    }
}

// Needs to get called after acquiring 'mInflightLock'
void CameraDeviceSession::cleanupBuffersLocked(int id) {
    auto cbsIt = mCirculatingBuffers.find(id);
    if (cbsIt == mCirculatingBuffers.end()) {
        return;
    }
    for (auto& pair : mCirculatingBuffers.at(id)) {
        sHandleImporter.freeBuffer(pair.second);
    }
    mCirculatingBuffers[id].clear();
    mCirculatingBuffers.erase(id);
}


void CameraDeviceSession::updateBufferCaches(const std::vector<BufferCache>& cachesToRemove) {
    // ALOGD("%s cachesToRemove.size:%d",__FUNCTION__,cachesToRemove.size());
    Mutex::Autolock _l(mInflightLock);
    for (auto& cache : cachesToRemove) {
        auto cbsIt = mCirculatingBuffers.find(cache.streamId);
        if (cbsIt == mCirculatingBuffers.end()) {
            // The stream could have been removed
            continue;
        }
        CirculatingBuffers& cbs = cbsIt->second;
        auto it = cbs.find(cache.bufferId);
        if (it != cbs.end()) {
            sHandleImporter.freeBuffer(it->second);
            cbs.erase(it);
        } else {
            ALOGE("%s: stream %d buffer %" PRIu64 " is not cached",
                    __FUNCTION__, cache.streamId, cache.bufferId);
        }
    }
}

/**
 * Map Android N dataspace definitions back to Android M definitions, for
 * use with HALv3.3 or older.
 *
 * Only map where correspondences exist, and otherwise preserve the value.
 */
android_dataspace CameraDeviceSession::mapToLegacyDataspace(
        android_dataspace dataSpace) const {
    if (mDeviceVersion <= CAMERA_DEVICE_API_VERSION_3_3) {
        switch (dataSpace) {
            case HAL_DATASPACE_V0_SRGB_LINEAR:
                return HAL_DATASPACE_SRGB_LINEAR;
            case HAL_DATASPACE_V0_SRGB:
                return HAL_DATASPACE_SRGB;
            case HAL_DATASPACE_V0_JFIF:
                return HAL_DATASPACE_JFIF;
            case HAL_DATASPACE_V0_BT601_625:
                return HAL_DATASPACE_BT601_625;
            case HAL_DATASPACE_V0_BT601_525:
                return HAL_DATASPACE_BT601_525;
            case HAL_DATASPACE_V0_BT709:
                return HAL_DATASPACE_BT709;
            default:
                return dataSpace;
        }
    }

   return dataSpace;
}

/**
 * For devices <= CAMERA_DEVICE_API_VERSION_3_2, AE_PRECAPTURE_TRIGGER_CANCEL is not supported so
 * we need to override AE_PRECAPTURE_TRIGGER_CANCEL to AE_PRECAPTURE_TRIGGER_IDLE and AE_LOCK_OFF
 * to AE_LOCK_ON to start cancelling AE precapture. If AE lock is not available, it still overrides
 * AE_PRECAPTURE_TRIGGER_CANCEL to AE_PRECAPTURE_TRIGGER_IDLE but doesn't add AE_LOCK_ON to the
 * request.
 */
bool CameraDeviceSession::handleAePrecaptureCancelRequestLocked(
        const camera3_capture_request_t &halRequest,
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata *settings /*out*/,
         AETriggerCancelOverride *override /*out*/) {
    if ((mDeviceVersion > CAMERA_DEVICE_API_VERSION_3_2) ||
            (nullptr == halRequest.settings) || (nullptr == settings) ||
            (0 == get_camera_metadata_entry_count(halRequest.settings))) {
        return false;
    }

    settings->clear();
    settings->append(halRequest.settings);
    camera_metadata_entry_t aePrecaptureTrigger =
            settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
    if (aePrecaptureTrigger.count > 0 &&
            aePrecaptureTrigger.data.u8[0] ==
                    ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL) {
        // Always override CANCEL to IDLE
        uint8_t aePrecaptureTrigger =
                ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
        settings->update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                &aePrecaptureTrigger, 1);
        *override = { false, ANDROID_CONTROL_AE_LOCK_OFF,
                true, ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL };

        if (mIsAELockAvailable == true) {
            camera_metadata_entry_t aeLock = settings->find(
                    ANDROID_CONTROL_AE_LOCK);
            if (aeLock.count == 0 || aeLock.data.u8[0] ==
                    ANDROID_CONTROL_AE_LOCK_OFF) {
                uint8_t aeLock = ANDROID_CONTROL_AE_LOCK_ON;
                settings->update(ANDROID_CONTROL_AE_LOCK, &aeLock, 1);
                override->applyAeLock = true;
                override->aeLock = ANDROID_CONTROL_AE_LOCK_OFF;
            }
        }

        return true;
    }

    return false;
}

void CameraDeviceSession::notify(NotifyMsg msg){
    long frameNumber = 0;
    using Tag = aidl::android::hardware::camera::device::NotifyMsg::Tag;
    switch (msg.getTag()) {
        case Tag::error:
            ALOGD("%s frameNumber:%d ",__FUNCTION__,msg.get<Tag::error>().frameNumber);
            frameNumber = msg.get<Tag::error>().frameNumber;
            break;
        case Tag::shutter:
            // ALOGD("%s frameNumber:%d timestamp=%" PRId64" ",__FUNCTION__,msg.get<Tag::shutter>().frameNumber,(long)msg.get<Tag::shutter>().timestamp);
            frameNumber = msg.get<Tag::shutter>().frameNumber;
            break;
    }

    mCb->notify({msg});

    Mutex::Autolock _l(mInflightRequestLock);
    HwCaptureRequest  req = mInflightRequest[frameNumber];

}
void CameraDeviceSession::sNotify(
        const camera3_callback_ops *cb,
        const camera3_notify_msg *msg) {
// ALOGD("%s,%d",__FUNCTION__,__LINE__);
    CameraDeviceSession *d =
            const_cast<CameraDeviceSession*>(static_cast<const CameraDeviceSession*>(cb));
    switch (msg->type) {
        case CAMERA3_MSG_ERROR:
            {
                // The camera3_stream_t* must be the same as what wrapper HAL passed to conventional
                // HAL, or the ID lookup will return garbage. Caller should validate the ID here is
                // indeed one of active stream IDs
                Camera3Stream* stream = static_cast<Camera3Stream*>(
                        msg->message.error.error_stream);
                
                if (stream != nullptr) {
                    if (d->mStreamMap.count(stream->mId) != 1) {
                        ALOGE("%s: unknown stream ID %d reports an error!",
                                __FUNCTION__, stream->mId);
                        return;
                    }
                }
                NotifyMsg error;
                error.set<NotifyMsg::Tag::error>(ErrorMsg{.frameNumber = static_cast<int32_t>(msg->message.error.frame_number),
                                                  .errorStreamId = (stream != nullptr) ? stream->mId : -1,
                                                  .errorCode = (ErrorCode) msg->message.error.error_code});
                switch ((ErrorCode)msg->message.error.error_code)
                {
                    case ErrorCode::ERROR_DEVICE:
                    case ErrorCode::ERROR_REQUEST:
                    case ErrorCode::ERROR_RESULT: {
                        Mutex::Autolock _l(d->mInflightLock);
                        auto entry = d->mInflightAETriggerOverrides.find(
                                msg->message.error.frame_number);
                        if (d->mInflightAETriggerOverrides.end() != entry) {
                            d->mInflightAETriggerOverrides.erase(
                                    msg->message.error.frame_number);
                        }

                        auto boostEntry = d->mInflightRawBoostPresent.find(
                                msg->message.error.frame_number);
                        if (d->mInflightRawBoostPresent.end() != boostEntry) {
                            d->mInflightRawBoostPresent.erase(
                                    msg->message.error.frame_number);
                        }
                    }
                        break;
                    case ErrorCode::ERROR_BUFFER:
                    default:
                        break;
                }
                d->notify(error);
            }
            break;
        case CAMERA3_MSG_SHUTTER:
            {
                NotifyMsg shutter;
                int32_t frameNumber = static_cast<int32_t>(msg->message.shutter.frame_number);
                int64_t timestamp = static_cast<int64_t>(msg->message.shutter.timestamp);
                int64_t readoutTimestamp = static_cast<int64_t>(msg->message.shutter.timestamp);
                int64_t exposureTime = kDefaultSensorExposureTimeNs;
                if(frameNumber > 1){
                    exposureTime = d->getSensorExposureTime();
                }
                readoutTimestamp += exposureTime;

                shutter.set<NotifyMsg::Tag::shutter>(
                        ShutterMsg{
                            .frameNumber = frameNumber,
                            .timestamp = timestamp,
                            .readoutTimestamp = readoutTimestamp});
                d->notify(shutter);
            }
            break;
        default:
            ALOGE("%s: AIDL type converion failed. Unknown msg type 0x%x",
                    __FUNCTION__, msg->type);
    }
}


}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

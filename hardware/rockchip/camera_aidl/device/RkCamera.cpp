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

#define FAILURE_DEBUG_PREFIX "RkCamera"

#include <inttypes.h>
#include <cstdlib>

#include <log/log.h>
#include <system/camera_metadata.h>
#include <linux/videodev2.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>

#include "debug.h"
//#include "jpeg.h"
#include "metadata_utils.h"
#include "RkCamera.h"


namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {
namespace hw {

using base::unique_fd;

namespace {
constexpr char kClass[] = "RkCamera";

constexpr int kMinFPS = 2;
constexpr int kMedFPS = 15;
constexpr int kMaxFPS = 30;
constexpr int64_t kOneSecondNs = 1000000000;

constexpr int64_t kMinFrameDurationNs = kOneSecondNs / kMaxFPS;
constexpr int64_t kMaxFrameDurationNs = kOneSecondNs / kMinFPS;
constexpr int64_t kDefaultFrameDurationNs = kOneSecondNs / kMedFPS;

constexpr int64_t kMinSensorExposureTimeNs = kOneSecondNs / 20000;
constexpr int64_t kMaxSensorExposureTimeNs = kOneSecondNs / 2;
constexpr int64_t kDefaultSensorExposureTimeNs = kOneSecondNs / 100;

constexpr int32_t kMinSensorSensitivity = 25;
constexpr int32_t kMaxSensorSensitivity = 1600;
constexpr int32_t kDefaultSensorSensitivity = 200;

constexpr float   kMinAperture = 1.4;
constexpr float   kMaxAperture = 16.0;
constexpr float   kDefaultAperture = 4.0;

constexpr int32_t kDefaultJpegQuality = 85;

constexpr BufferUsage usageOr(const BufferUsage a, const BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

constexpr bool usageTest(const BufferUsage a, const BufferUsage b) {
    return (static_cast<uint64_t>(a) & static_cast<uint64_t>(b)) != 0;
}

}  // namespace

RkCamera::RkCamera(sp<CameraModule> module,std::string cameraId/*const Parameters& params*/)
        : mModule(module),mCameraId(cameraId),/*mParams(params)
        , */mAFStateMachine(200, 1, 2) {
        }
std::string RkCamera::getCameraId() const{
    return mCameraId;
}
std::tuple<PixelFormat, BufferUsage, Dataspace, int32_t>
RkCamera::overrideStreamParams(const PixelFormat format,
                                 const BufferUsage usage,
                                 const Dataspace dataspace) const {
    constexpr BufferUsage kExtraUsage = usageOr(BufferUsage::CAMERA_OUTPUT,
                                                BufferUsage::CPU_WRITE_OFTEN);

    switch (format) {
    case PixelFormat::IMPLEMENTATION_DEFINED:
        if (usageTest(usage, BufferUsage::VIDEO_ENCODER)) {
            return {PixelFormat::YCBCR_420_888, usageOr(usage, kExtraUsage),
                    Dataspace::JFIF, 8};
        } else {
            return {PixelFormat::IMPLEMENTATION_DEFINED, usageOr(usage, kExtraUsage),
                    Dataspace::JFIF, 4};
        }

    case PixelFormat::YCBCR_420_888:
        return {PixelFormat::YCBCR_420_888, usageOr(usage, kExtraUsage),
                Dataspace::JFIF, usageTest(usage, BufferUsage::VIDEO_ENCODER) ? 8 : 4};

    // case PixelFormat::RGBA_8888:
    //     return {PixelFormat::RGBA_8888, usageOr(usage, kExtraUsage),
    //             Dataspace::UNKNOWN, usageTest(usage, BufferUsage::VIDEO_ENCODER) ? 8 : 4};

    case PixelFormat::BLOB:
        switch (dataspace) {
        case Dataspace::JFIF:
            return {PixelFormat::BLOB, usageOr(usage, kExtraUsage),
                    Dataspace::JFIF, 4};  // JPEG
        default:
            return {format, usage, dataspace, FAILURE(kErrorBadDataspace)};
        }

    default:
        return {format, usage, dataspace, FAILURE(kErrorBadFormat)};
    }
}

bool RkCamera::configure(const CameraMetadata& sessionParams,
                           size_t nStreams,
                           const Stream* streams,
                           const HalStream* halStreams) {
    //applyMetadata(sessionParams);

    mStreamInfoCache.clear();
    for (; nStreams > 0; --nStreams, ++streams, ++halStreams) {
        const int32_t id = streams->id;
        LOG_ALWAYS_FATAL_IF(halStreams->id != id);
        StreamInfo& si = mStreamInfoCache[id];
        si.size.width = streams->width;
        si.size.height = streams->height;
        si.pixelFormat = halStreams->overrideFormat;
        si.blobBufferSize = streams->bufferSize;
    }

    return true;
}

void RkCamera::close() {
    mStreamInfoCache.clear();

}

std::vector<StreamBuffer> RkCamera::getOutputStreamBuffer(HwCaptureRequest req,int streamId){
    std::vector<StreamBuffer> outputBuffers;
    int csbsSize= req.buffers.size();
    outputBuffers.reserve(csbsSize);
    for (size_t i = 0; i < csbsSize; ++i) {
        CachedStreamBuffer* csb = req.buffers[i];
        LOG_ALWAYS_FATAL_IF(!csb);  // otherwise mNumBuffersInFlight will be hard

        const StreamInfo* si = csb->getStreamInfo<StreamInfo>();
        if (!si) {
            const auto sii = mStreamInfoCache.find(csb->getStreamId());
            if (sii == mStreamInfoCache.end()) {
                ALOGE("%s:%s:%d could not find stream=%d in the cache",
                      kClass, __func__, __LINE__, csb->getStreamId());
            } else {
                si = &sii->second;
                csb->setStreamInfo(si);
            }
        }

        if (si && streamId == csb->getStreamId()) {
            outputBuffers.push_back(csb->finish(true));
        }
    }
    //ALOGD("%s frameNumber:%d outputBuffers:%d",__FUNCTION__,req.frameNumber,outputBuffers.size());
    return outputBuffers;
}

std::tuple<int64_t, CameraMetadata,
           std::vector<StreamBuffer>, std::vector<DelayedStreamBuffer>>
RkCamera::processCaptureRequest(CameraMetadata metadataUpdate,
                                  Span<CachedStreamBuffer*> csbs) {
    CameraMetadata resultMetadata = metadataUpdate.metadata.empty() ?
        updateCaptureResultMetadata() :
        applyMetadata(std::move(metadataUpdate));

    const size_t csbsSize = csbs.size();
    std::vector<StreamBuffer> outputBuffers;
    std::vector<DelayedStreamBuffer> delayedOutputBuffers;
    outputBuffers.reserve(csbsSize);

    for (size_t i = 0; i < csbsSize; ++i) {
        CachedStreamBuffer* csb = csbs[i];
        LOG_ALWAYS_FATAL_IF(!csb);  // otherwise mNumBuffersInFlight will be hard

        const StreamInfo* si = csb->getStreamInfo<StreamInfo>();
        if (!si) {
            const auto sii = mStreamInfoCache.find(csb->getStreamId());
            if (sii == mStreamInfoCache.end()) {
                ALOGE("%s:%s:%d could not find stream=%d in the cache",
                      kClass, __func__, __LINE__, csb->getStreamId());
            } else {
                si = &sii->second;
                csb->setStreamInfo(si);
            }
        }

        if (si) {
            captureFrame(*si, csb, &outputBuffers, &delayedOutputBuffers);
        } else {
            outputBuffers.push_back(csb->finish(false));
        }
    }

    return make_tuple(( mFrameDurationNs /*: FAILURE(-1)*/),
                      std::move(resultMetadata), std::move(outputBuffers),
                      std::move(delayedOutputBuffers));
}

void RkCamera::captureFrame(const StreamInfo& si,
                              CachedStreamBuffer* csb,
                              std::vector<StreamBuffer>* outputBuffers,
                              std::vector<DelayedStreamBuffer>* delayedOutputBuffers) const {
    switch (si.pixelFormat) {
    case PixelFormat::IMPLEMENTATION_DEFINED:
        ALOGD("%s si.pixelFormat:%d IMPLEMENTATION_DEFINED",__FUNCTION__,si.pixelFormat);
        outputBuffers->push_back(csb->finish(captureFrameYUV(si, csb)));
        break;
    case PixelFormat::YCBCR_420_888:
        ALOGD("%s si.pixelFormat:%d YCBCR_420_888",__FUNCTION__,si.pixelFormat);
        outputBuffers->push_back(csb->finish(captureFrameYUV(si, csb)));
        break;

    case PixelFormat::RGBA_8888:
        ALOGD("%s si.pixelFormat:%d RGBA_8888",__FUNCTION__,si.pixelFormat);
        outputBuffers->push_back(csb->finish(captureFrameRGBA(si, csb)));
        break;

    case PixelFormat::BLOB:
        ALOGD("%s si.pixelFormat:%d BLOB",__FUNCTION__,si.pixelFormat);
        delayedOutputBuffers->push_back(captureFrameJpeg(si, csb));
        break;

    default:
        ALOGE("%s:%s:%d: unexpected pixelFormat=0x%" PRIx32,
              kClass, __func__, __LINE__,
              static_cast<uint32_t>(si.pixelFormat));
        outputBuffers->push_back(csb->finish(false));
        break;
    }
}

bool RkCamera::captureFrameYUV(const StreamInfo& si,
                                 CachedStreamBuffer* csb) const {
    const native_handle_t* native_handle = csb->getBuffer();
    buffer_handle_t* buffer_handle = (buffer_handle_t*)&native_handle;
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();
    int tempFrameWidth  = 1920;
    int tempFrameHeight = 1080;
    void *pointer = nullptr;
    //ALOGD("lockYCbCr");
    android::Rect bounds(android::ui::Size(tempFrameWidth, tempFrameHeight));
    if(mapper.lock(
            const_cast<native_handle_t*>(native_handle),
            static_cast<uint64_t>(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER),
            bounds,
            &pointer)!= NO_ERROR){
                ALOGE("%s lock failed %s",__FUNCTION__,strerror(errno));
        return FAILURE(false);
    }


        // {
        //     FILE* fp =NULL;
        //     char filename[128];
        //     filename[0] = 0x00;
        //     sprintf(filename, "/data/camera/dump_1920x1080_nv12",
        //         tempFrameWidth, tempFrameHeight);
        //     fp = fopen(filename, "r+");
        //     if (fp != NULL) {
        //         int size = fread((char*)pointer,1,tempFrameWidth*tempFrameHeight*1.5,fp);
        //         fclose(fp);
        //         ALOGD("read success yuv data to %s size:%d",filename, size);
        //     } else {
        //         ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
        //     }
        // }
#define DUMP_YUV
#ifdef DUMP_YUV
                    {
                        static int frameCount = 0;
                        if(++frameCount > 5 && frameCount<10){
                            FILE* fp =NULL;
                            char filename[128];
                            filename[0] = 0x00;
                            sprintf(filename, "/data/camera/camera_dump_%dx%d_%d.yuv",
                                    tempFrameWidth, tempFrameHeight, frameCount);
                            fp = fopen(filename, "wb+");
                            if (fp != NULL) {
                                fwrite((char*)pointer, 1, tempFrameWidth*tempFrameHeight*1.5, fp);
                                fclose(fp);
                                ALOGI("Write success YUV data to %s",filename);
                            } else {
                                ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
                            }
                        }
                    }
#endif

    mapper.unlock(native_handle);
    return true;
}

bool RkCamera::captureFrameRGBA(const StreamInfo& si,
                                  CachedStreamBuffer* csb) const {
    ALOGD("%s",__FUNCTION__);
    return false;
}

DelayedStreamBuffer RkCamera::captureFrameJpeg(const StreamInfo& si,
                                                 CachedStreamBuffer* csb) const {
    const native_handle_t* const image = captureFrameForCompressing(
        si.size, PixelFormat::YCBCR_420_888, V4L2_PIX_FMT_YUV420);

    const Rect<uint16_t> imageSize = si.size;
    const uint32_t jpegBufferSize = si.blobBufferSize;
    const int64_t frameDurationNs = mFrameDurationNs;
    CameraMetadata metadata = mCaptureResultMetadata;

    return [csb, image, imageSize, metadata = std::move(metadata), jpegBufferSize,
            frameDurationNs](const bool ok) -> StreamBuffer {
        StreamBuffer sb;
        if (ok && image && csb->waitAcquireFence(frameDurationNs / 1000000)) {
            android_ycbcr imageYcbcr;
            if (GraphicBufferMapper::get().lockYCbCr(
                    image, static_cast<uint32_t>(BufferUsage::CPU_READ_OFTEN),
                    {imageSize.width, imageSize.height}, &imageYcbcr) == NO_ERROR) {
                sb = csb->finish(compressJpeg(imageSize, imageYcbcr, metadata,
                                              csb->getBuffer(), jpegBufferSize));
                LOG_ALWAYS_FATAL_IF(GraphicBufferMapper::get().unlock(image) != NO_ERROR);
            } else {
                sb = csb->finish(FAILURE(false));
            }
        } else {
            sb = csb->finish(false);
        }

        if (image) {
            GraphicBufferAllocator::get().free(image);
        }

        return sb;
    };
}

const native_handle_t* RkCamera::captureFrameForCompressing(
        const Rect<uint16_t> dim,
        const PixelFormat bufferFormat,
        const uint32_t qemuFormat) const {
    constexpr BufferUsage kUsage = usageOr(BufferUsage::CAMERA_OUTPUT,
                                           BufferUsage::CPU_READ_OFTEN);

    GraphicBufferAllocator& gba = GraphicBufferAllocator::get();
    const native_handle_t* image = nullptr;
    uint32_t stride;

    if (gba.allocate(dim.width, dim.height, static_cast<int>(bufferFormat), 1,
                     static_cast<uint64_t>(kUsage), &image, &stride,
                     "RkCamera") != NO_ERROR) {
        return FAILURE(nullptr);
    }
    return image;
}

bool RkCamera::queryFrame(const Rect<uint16_t> dim,
                            const uint32_t pixelFormat,
                            const float exposureComp,
                            const uint64_t dataOffset) const {
    constexpr float scaleR = 1;
    constexpr float scaleG = 1;
    constexpr float scaleB = 1;

    return true;
}

float RkCamera::calculateExposureComp(const int64_t exposureNs,
                                        const int sensorSensitivity,
                                        const float aperture) {
    return (double(exposureNs) * sensorSensitivity
                * kDefaultAperture * kDefaultAperture) /
           (double(kDefaultSensorExposureTimeNs) * kDefaultSensorSensitivity
                * aperture * aperture);
}

CameraMetadata RkCamera::applyMetadata(const CameraMetadata& metadata) {
    const camera_metadata_t* const raw =
        reinterpret_cast<const camera_metadata_t*>(metadata.metadata.data());
    camera_metadata_ro_entry_t entry;

    mFrameDurationNs = getFrameDuration(raw, kDefaultFrameDurationNs,
                                        kMinFrameDurationNs, kMaxFrameDurationNs);

    if (find_camera_metadata_ro_entry(raw, ANDROID_SENSOR_EXPOSURE_TIME, &entry)) {
        mSensorExposureDurationNs = std::min(mFrameDurationNs, kDefaultSensorExposureTimeNs);
    } else {
        mSensorExposureDurationNs = entry.data.i64[0];
    }

    if (find_camera_metadata_ro_entry(raw, ANDROID_SENSOR_SENSITIVITY, &entry)) {
        mSensorSensitivity = kDefaultSensorSensitivity;
    } else {
        mSensorSensitivity = entry.data.i32[0];
    }

    if (find_camera_metadata_ro_entry(raw, ANDROID_LENS_APERTURE, &entry)) {
        mAperture = kDefaultAperture;
    } else {
        mAperture = entry.data.f[0];
    }

    const camera_metadata_enum_android_control_af_mode_t afMode =
        find_camera_metadata_ro_entry(raw, ANDROID_CONTROL_AF_MODE, &entry) ?
            ANDROID_CONTROL_AF_MODE_OFF :
            static_cast<camera_metadata_enum_android_control_af_mode_t>(entry.data.i32[0]);

    const camera_metadata_enum_android_control_af_trigger_t afTrigger =
        find_camera_metadata_ro_entry(raw, ANDROID_CONTROL_AF_TRIGGER, &entry) ?
            ANDROID_CONTROL_AF_TRIGGER_IDLE :
            static_cast<camera_metadata_enum_android_control_af_trigger_t>(entry.data.i32[0]);

    const auto af = mAFStateMachine(afMode, afTrigger);

    mExposureComp = calculateExposureComp(mSensorExposureDurationNs,
                                          mSensorSensitivity, mAperture);

    CameraMetadataMap m = parseCameraMetadataMap(metadata);

    m[ANDROID_CONTROL_AE_STATE] = ANDROID_CONTROL_AE_STATE_CONVERGED;
    m[ANDROID_CONTROL_AF_STATE] = af.first;
    m[ANDROID_CONTROL_AWB_STATE] = ANDROID_CONTROL_AWB_STATE_CONVERGED;
    m[ANDROID_FLASH_STATE] = ANDROID_FLASH_STATE_UNAVAILABLE;
    m[ANDROID_LENS_APERTURE] = mAperture;
    m[ANDROID_LENS_FOCUS_DISTANCE] = af.second;
    m[ANDROID_LENS_STATE] = ANDROID_LENS_STATE_STATIONARY;
    m[ANDROID_REQUEST_PIPELINE_DEPTH] = uint8_t(4);
    m[ANDROID_SENSOR_FRAME_DURATION] = mFrameDurationNs;
    m[ANDROID_SENSOR_EXPOSURE_TIME] = mSensorExposureDurationNs;
    m[ANDROID_SENSOR_SENSITIVITY] = mSensorSensitivity;
    m[ANDROID_SENSOR_TIMESTAMP] = int64_t(0);
    m[ANDROID_SENSOR_ROLLING_SHUTTER_SKEW] = kMinSensorExposureTimeNs;
    m[ANDROID_STATISTICS_SCENE_FLICKER] = ANDROID_STATISTICS_SCENE_FLICKER_NONE;

    std::optional<CameraMetadata> maybeSerialized =
        serializeCameraMetadataMap(m);

    if (maybeSerialized) {
        mCaptureResultMetadata = std::move(maybeSerialized.value());
    }

    {   // reset ANDROID_CONTROL_AF_TRIGGER to IDLE
        camera_metadata_t* const raw =
            reinterpret_cast<camera_metadata_t*>(mCaptureResultMetadata.metadata.data());

        camera_metadata_ro_entry_t entry;
        const auto newTriggerValue = ANDROID_CONTROL_AF_TRIGGER_IDLE;

        if (find_camera_metadata_ro_entry(raw, ANDROID_CONTROL_AF_TRIGGER, &entry)) {
            return mCaptureResultMetadata;
        } else if (entry.data.i32[0] == newTriggerValue) {
            return mCaptureResultMetadata;
        } else {
            CameraMetadata result = mCaptureResultMetadata;

            if (update_camera_metadata_entry(raw, entry.index, &newTriggerValue, 1, nullptr)) {
                ALOGW("%s:%s:%d: update_camera_metadata_entry(ANDROID_CONTROL_AF_TRIGGER) "
                      "failed", kClass, __func__, __LINE__);
            }

            return result;
        }
    }
}

CameraMetadata RkCamera::updateCaptureResultMetadata() {
    camera_metadata_t* const raw =
        reinterpret_cast<camera_metadata_t*>(mCaptureResultMetadata.metadata.data());

    const auto af = mAFStateMachine();

    camera_metadata_ro_entry_t entry;

    if (find_camera_metadata_ro_entry(raw, ANDROID_CONTROL_AF_STATE, &entry)) {
        ALOGW("%s:%s:%d: find_camera_metadata_ro_entry(ANDROID_CONTROL_AF_STATE) failed",
              kClass, __func__, __LINE__);
    } else if (update_camera_metadata_entry(raw, entry.index, &af.first, 1, nullptr)) {
        ALOGW("%s:%s:%d: update_camera_metadata_entry(ANDROID_CONTROL_AF_STATE) failed",
              kClass, __func__, __LINE__);
    }

    if (find_camera_metadata_ro_entry(raw, ANDROID_LENS_FOCUS_DISTANCE, &entry)) {
        ALOGW("%s:%s:%d: find_camera_metadata_ro_entry(ANDROID_LENS_FOCUS_DISTANCE) failed",
              kClass, __func__, __LINE__);
    } else if (update_camera_metadata_entry(raw, entry.index, &af.second, 1, nullptr)) {
        ALOGW("%s:%s:%d: update_camera_metadata_entry(ANDROID_LENS_FOCUS_DISTANCE) failed",
              kClass, __func__, __LINE__);
    }

    return metadataCompact(mCaptureResultMetadata);
}

////////////////////////////////////////////////////////////////////////////////

Span<const std::pair<int32_t, int32_t>> RkCamera::getTargetFpsRanges() const {
    // ordered to satisfy testPreviewFpsRangeByCamera
    static const std::pair<int32_t, int32_t> targetFpsRanges[] = {
        {kMinFPS, kMedFPS},
        {kMedFPS, kMedFPS},
        {kMinFPS, kMaxFPS},
        {kMaxFPS, kMaxFPS},
    };

    return targetFpsRanges;
}

Span<const Rect<uint16_t>> RkCamera::getAvailableThumbnailSizes() const {
    return {mParams.availableThumbnailResolutions.begin(),
            mParams.availableThumbnailResolutions.end()};
}

bool RkCamera::isBackFacing() const {
    return mParams.isBackFacing;
}

Span<const float> RkCamera::getAvailableApertures() const {
    static const float availableApertures[] = {
        1.4, 2.0, 2.8, 4.0, 5.6, 8.0, 11.0, 16.0
    };

    return availableApertures;
}

std::tuple<int32_t, int32_t, int32_t> RkCamera::getMaxNumOutputStreams() const {
    return {
        0,  // raw
        2,  // processed
        1,  // jpeg
    };
}

Span<const PixelFormat> RkCamera::getSupportedPixelFormats() const {
    static const PixelFormat supportedPixelFormats[] = {
        PixelFormat::IMPLEMENTATION_DEFINED,
        PixelFormat::YCBCR_420_888,
        PixelFormat::RGBA_8888,
        PixelFormat::BLOB,
    };

    return {supportedPixelFormats};
}

int64_t RkCamera::getMinFrameDurationNs() const {
    return kMinFrameDurationNs;
}

Rect<uint16_t> RkCamera::getSensorSize() const {
    return mParams.sensorSize;
}

std::pair<int32_t, int32_t> RkCamera::getSensorSensitivityRange() const {
    return {kMinSensorSensitivity, kMaxSensorSensitivity};
}

std::pair<int64_t, int64_t> RkCamera::getSensorExposureTimeRange() const {
    return {kMinSensorExposureTimeNs, kMaxSensorExposureTimeNs};
}

int64_t RkCamera::getSensorMaxFrameDuration() const {
    return kMaxSensorExposureTimeNs;
}

Span<const Rect<uint16_t>> RkCamera::getSupportedResolutions() const {
    return {mParams.supportedResolutions.begin(), mParams.supportedResolutions.end()};
}

std::pair<int32_t, int32_t> RkCamera::getDefaultTargetFpsRange(const RequestTemplate tpl) const {
    switch (tpl) {
    case RequestTemplate::PREVIEW:
    case RequestTemplate::VIDEO_RECORD:
    case RequestTemplate::VIDEO_SNAPSHOT:
        return {kMaxFPS, kMaxFPS};

    default:
        return {kMinFPS, kMaxFPS};
    }
}

float RkCamera::getDefaultAperture() const {
    return kDefaultAperture;
}

int64_t RkCamera::getDefaultSensorExpTime() const {
    return kDefaultSensorExposureTimeNs;
}

int64_t RkCamera::getDefaultSensorFrameDuration() const {
    return kMinFrameDurationNs;
}

int32_t RkCamera::getDefaultSensorSensitivity() const {
    return kDefaultSensorSensitivity;
}
sp<CameraModule> RkCamera::getModule() const {
    return mModule;
}
camera3_device_t* RkCamera::getDevice() const {
    return mDevice;
}
void RkCamera::setDevice(camera3_device_t* device) {
    mDevice =  device;
}

}  // namespace hw
}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

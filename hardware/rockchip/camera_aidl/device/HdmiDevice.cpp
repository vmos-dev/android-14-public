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

#define LOG_TAG "HdmiDev"
//#define LOG_NDEBUG 0
#undef NDEBUG //ALL
#include <log/log.h>

#include "HdmiDevice.h"

#include <aidl/android/hardware/camera/common/Status.h>
#include <convert.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <regex>
#include <set>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using ::aidl::android::hardware::camera::common::Status;

namespace {
// Only support MJPEG for now as it seems to be the one supports higher fps
// Other formats to consider in the future:
// * V4L2_PIX_FMT_YVU420 (== YV12)
// * V4L2_PIX_FMT_YVYU (YVYU: can be converted to YV12 or other YUV420_888 formats)
const std::array<uint32_t, /*size*/ 4> kSupportedFourCCs{
        {V4L2_PIX_FMT_BGR24,V4L2_PIX_FMT_NV24,V4L2_PIX_FMT_NV16, V4L2_PIX_FMT_NV12}};  // double braces required in C++11

constexpr int MAX_RETRY = 5;                  // Allow retry v4l2 open failures a few times.
constexpr int OPEN_RETRY_SLEEP_US = 100'000;  // 100ms * MAX_RETRY = 0.5 seconds

const std::regex kDevicePathRE("/dev/video([0-9]+)");
}  // namespace

std::string HdmiDevice::kDeviceVersion = "1.1";

HdmiDevice::HdmiDevice(const std::string& devicePath,
                                           const HdmiConfig& config)
    : mCameraId("-1"), mDevicePath(devicePath), mCfg(config) {
    std::smatch sm;
    if (std::regex_match(mDevicePath, sm, kDevicePathRE)) {
        mCameraId = std::to_string(mCfg.cameraIdOffset + std::stoi(sm[1]));
    } else {
        ALOGE("%s: device path match failed for %s", __FUNCTION__, mDevicePath.c_str());
    }
}

HdmiDevice::~HdmiDevice() {}

ndk::ScopedAStatus HdmiDevice::getCameraCharacteristics(CameraMetadata* _aidl_return) {
    Mutex::Autolock _l(mLock);
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    if (isInitFailedLocked()) {
        return fromStatus(Status::INTERNAL_ERROR);
    }

    const camera_metadata_t* rawMetadata = mCameraCharacteristics.getAndLock();
    convertToAidl(rawMetadata, _aidl_return);
    mCameraCharacteristics.unlock(rawMetadata);
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiDevice::getPhysicalCameraCharacteristics(const std::string&,
                                                                          CameraMetadata*) {
    ALOGE("%s: Physical camera functions are not supported for hdmis.", __FUNCTION__);
    return fromStatus(Status::ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus HdmiDevice::getResourceCost(CameraResourceCost* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    _aidl_return->resourceCost = 100;
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiDevice::isStreamCombinationSupported(
        const StreamConfiguration& in_streams, bool* _aidl_return) {
    if (isInitFailed()) {
        ALOGE("%s: camera %s. camera init failed!", __FUNCTION__, mCameraId.c_str());
        return fromStatus(Status::INTERNAL_ERROR);
    }
    Status s = HdmiDeviceSession::isStreamCombinationSupported(in_streams,
                                                                         mSupportedFormats, mCfg);
    *_aidl_return = s == Status::OK;
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiDevice::open(
        const std::shared_ptr<ICameraDeviceCallback>& in_callback,
        std::shared_ptr<ICameraDeviceSession>* _aidl_return) {
    if (_aidl_return == nullptr) {
        ALOGE("%s: cannot open camera %s. return session ptr is null!", __FUNCTION__,
              mCameraId.c_str());
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    Mutex::Autolock _l(mLock);
    if (isInitFailedLocked()) {
        ALOGE("%s: cannot open camera %s. camera init failed!", __FUNCTION__, mCameraId.c_str());
        return fromStatus(Status::INTERNAL_ERROR);
    }

    std::shared_ptr<HdmiDeviceSession> session;
    ALOGV("%s: Initializing device for camera %s", __FUNCTION__, mCameraId.c_str());
    session = mSession.lock();

    if (session != nullptr && !session->isClosed()) {
        ALOGE("%s: cannot open an already opened camera!", __FUNCTION__);
        return fromStatus(Status::CAMERA_IN_USE);
    }

    int numAttempt = 0;
    unique_fd fd(::open(mDevicePath.c_str(), O_RDWR));
    while (fd.get() < 0 && numAttempt < MAX_RETRY) {
        // Previous retry attempts failed. Retry opening the device at most MAX_RETRY times
        ALOGW("%s: v4l2 device %s open failed, wait 33ms and try again", __FUNCTION__,
              mDevicePath.c_str());
        usleep(OPEN_RETRY_SLEEP_US);  // sleep and try again
        fd.reset(::open(mDevicePath.c_str(), O_RDWR));
        numAttempt++;
    }

    if (fd.get() < 0) {
        ALOGE("%s: v4l2 device open %s failed: %s", __FUNCTION__, mDevicePath.c_str(),
              strerror(errno));
        return fromStatus(Status::INTERNAL_ERROR);
    }

    session = createSession(in_callback, mCfg, mSupportedFormats, mCroppingType,
                            mCameraCharacteristics, mCameraId, std::move(fd));
    if (session == nullptr) {
        ALOGE("%s: camera device session allocation failed", __FUNCTION__);
        return fromStatus(Status::INTERNAL_ERROR);
    }

    if (session->isInitFailed()) {
        ALOGE("%s: camera device session init failed", __FUNCTION__);
        return fromStatus(Status::INTERNAL_ERROR);
    }

    mSession = session;
    *_aidl_return = session;
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiDevice::openInjectionSession(
        const std::shared_ptr<ICameraDeviceCallback>&, std::shared_ptr<ICameraInjectionSession>*) {
    return fromStatus(Status::OPERATION_NOT_SUPPORTED);
}

ndk::ScopedAStatus HdmiDevice::setTorchMode(bool) {
    return fromStatus(Status::OPERATION_NOT_SUPPORTED);
}

ndk::ScopedAStatus HdmiDevice::turnOnTorchWithStrengthLevel(int32_t) {
    return fromStatus(Status::OPERATION_NOT_SUPPORTED);
}

ndk::ScopedAStatus HdmiDevice::getTorchStrengthLevel(int32_t*) {
    return fromStatus(Status::OPERATION_NOT_SUPPORTED);
}

std::shared_ptr<HdmiDeviceSession> HdmiDevice::createSession(
        const std::shared_ptr<ICameraDeviceCallback>& cb, const HdmiConfig& cfg,
        const std::vector<SupportedV4L2Format>& sortedFormats, const CroppingType& croppingType,
        const common::V1_0::helper::CameraMetadata& chars, const std::string& cameraId,
        unique_fd v4l2Fd) {
    return ndk::SharedRefBase::make<HdmiDeviceSession>(
            cb, cfg, sortedFormats, croppingType, chars, cameraId, std::move(v4l2Fd));
}

bool HdmiDevice::isInitFailed() {
    Mutex::Autolock _l(mLock);
    return isInitFailedLocked();
}

bool HdmiDevice::isInitFailedLocked() {
    if (!mInitialized) {
        status_t ret = initCameraCharacteristics();
        if (ret != OK) {
            ALOGE("%s: init camera characteristics failed: errorno %d", __FUNCTION__, ret);
            mInitFailed = true;
        }
        mInitialized = true;
    }
    return mInitFailed;
}

void HdmiDevice::initSupportedFormatsLocked(int fd) {
    std::vector<SupportedV4L2Format> horizontalFmts =
            getCandidateSupportedFormatsLocked(fd, HORIZONTAL, mCfg.fpsLimits, mCfg.depthFpsLimits,
                                               mCfg.minStreamSize, mCfg.depthEnabled);
    std::vector<SupportedV4L2Format> verticalFmts =
            getCandidateSupportedFormatsLocked(fd, VERTICAL, mCfg.fpsLimits, mCfg.depthFpsLimits,
                                               mCfg.minStreamSize, mCfg.depthEnabled);

    size_t horiSize = horizontalFmts.size();
    size_t vertSize = verticalFmts.size();

    if (horiSize == 0 && vertSize == 0) {
        ALOGE("%s: cannot find suitable cropping type!", __FUNCTION__);
        return;
    }

    if (horiSize == 0) {
        mSupportedFormats = verticalFmts;
        mCroppingType = VERTICAL;
        return;
    } else if (vertSize == 0) {
        mSupportedFormats = horizontalFmts;
        mCroppingType = HORIZONTAL;
        return;
    }

    const auto& maxHoriSize = horizontalFmts[horizontalFmts.size() - 1];
    const auto& maxVertSize = verticalFmts[verticalFmts.size() - 1];

    // Try to keep the largest possible output size
    // When they are the same or ambiguous, pick the one support more sizes
    if (maxHoriSize.width == maxVertSize.width && maxHoriSize.height == maxVertSize.height) {
        if (horiSize > vertSize) {
            mSupportedFormats = horizontalFmts;
            mCroppingType = HORIZONTAL;
        } else {
            mSupportedFormats = verticalFmts;
            mCroppingType = VERTICAL;
        }
    } else if (maxHoriSize.width >= maxVertSize.width && maxHoriSize.height >= maxVertSize.height) {
        mSupportedFormats = horizontalFmts;
        mCroppingType = HORIZONTAL;
    } else if (maxHoriSize.width <= maxVertSize.width && maxHoriSize.height <= maxVertSize.height) {
        mSupportedFormats = verticalFmts;
        mCroppingType = VERTICAL;
    } else {
        if (horiSize > vertSize) {
            mSupportedFormats = horizontalFmts;
            mCroppingType = HORIZONTAL;
        } else {
            mSupportedFormats = verticalFmts;
            mCroppingType = VERTICAL;
        }
    }
}

status_t HdmiDevice::initCameraCharacteristics() {
    if (!mCameraCharacteristics.isEmpty()) {
        // Camera Characteristics previously initialized. Skip.
        return OK;
    }

    // init camera characteristics
    unique_fd fd(::open(mDevicePath.c_str(), O_RDWR));
    if (fd.get() < 0) {
        ALOGE("%s: v4l2 device open %s failed", __FUNCTION__, mDevicePath.c_str());
        return DEAD_OBJECT;
    }

    status_t ret;
    ret = initDefaultCharsKeys(&mCameraCharacteristics);
    if (ret != OK) {
        ALOGE("%s: init default characteristics key failed: errorno %d", __FUNCTION__, ret);
        mCameraCharacteristics.clear();
        return ret;
    }

    ret = initCameraControlsCharsKeys(fd.get(), &mCameraCharacteristics);
    if (ret != OK) {
        ALOGE("%s: init camera control characteristics key failed: errorno %d", __FUNCTION__, ret);
        mCameraCharacteristics.clear();
        return ret;
    }

    ret = initOutputCharsKeys(fd.get(), &mCameraCharacteristics);
    if (ret != OK) {
        ALOGE("%s: init output characteristics key failed: errorno %d", __FUNCTION__, ret);
        mCameraCharacteristics.clear();
        return ret;
    }

    ret = initAvailableCapabilities(&mCameraCharacteristics);
    if (ret != OK) {
        ALOGE("%s: init available capabilities key failed: errorno %d", __FUNCTION__, ret);
        mCameraCharacteristics.clear();
        return ret;
    }

    return OK;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define UPDATE(tag, data, size)                        \
    do {                                               \
        if (metadata->update((tag), (data), (size))) { \
            ALOGE("Update " #tag " failed!");          \
            return -EINVAL;                            \
        }                                              \
    } while (0)

status_t HdmiDevice::initAvailableCapabilities(
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* metadata) {
    if (mSupportedFormats.empty()) {
        ALOGE("%s: Supported formats list is empty", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    bool hasDepth = false;
    bool hasColor = false;
    for (const auto& fmt : mSupportedFormats) {
        switch (fmt.fourcc) {
            case V4L2_PIX_FMT_Z16:
                hasDepth = true;
                break;
            case V4L2_PIX_FMT_NV24:
                hasColor = true;
                break;
            case V4L2_PIX_FMT_H264:
                hasColor = true;
                break;
            default:
                ALOGW("%s: Unsupported format found", __FUNCTION__);
        }
    }

    std::vector<uint8_t> availableCapabilities;
    if (hasDepth) {
        availableCapabilities.push_back(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT);
    }
    if (hasColor) {
        availableCapabilities.push_back(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);
    }
    if (!availableCapabilities.empty()) {
        UPDATE(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, availableCapabilities.data(),
               availableCapabilities.size());
    }

    return OK;
}

status_t HdmiDevice::initDefaultCharsKeys(
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* metadata) {
    const uint8_t hardware_level = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL;
    UPDATE(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &hardware_level, 1);

    // android.colorCorrection
    const uint8_t availableAberrationModes[] = {ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF};
    UPDATE(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES, availableAberrationModes,
           ARRAY_SIZE(availableAberrationModes));

    // android.control
    const uint8_t antibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    UPDATE(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES, &antibandingMode, 1);

    const int32_t controlMaxRegions[] = {/*AE*/ 0, /*AWB*/ 0, /*AF*/ 0};
    UPDATE(ANDROID_CONTROL_MAX_REGIONS, controlMaxRegions, ARRAY_SIZE(controlMaxRegions));

    const uint8_t videoStabilizationMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    UPDATE(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES, &videoStabilizationMode, 1);

    const uint8_t awbAvailableMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    UPDATE(ANDROID_CONTROL_AWB_AVAILABLE_MODES, &awbAvailableMode, 1);

    const uint8_t aeAvailableMode = ANDROID_CONTROL_AE_MODE_ON;
    UPDATE(ANDROID_CONTROL_AE_AVAILABLE_MODES, &aeAvailableMode, 1);

    const uint8_t availableFffect = ANDROID_CONTROL_EFFECT_MODE_OFF;
    UPDATE(ANDROID_CONTROL_AVAILABLE_EFFECTS, &availableFffect, 1);

    const uint8_t controlAvailableModes[] = {ANDROID_CONTROL_MODE_OFF, ANDROID_CONTROL_MODE_AUTO};
    UPDATE(ANDROID_CONTROL_AVAILABLE_MODES, controlAvailableModes,
           ARRAY_SIZE(controlAvailableModes));

    // android.edge
    const uint8_t edgeMode = ANDROID_EDGE_MODE_OFF;
    UPDATE(ANDROID_EDGE_AVAILABLE_EDGE_MODES, &edgeMode, 1);

    // android.flash
    const uint8_t flashInfo = ANDROID_FLASH_INFO_AVAILABLE_FALSE;
    UPDATE(ANDROID_FLASH_INFO_AVAILABLE, &flashInfo, 1);

    // android.hotPixel
    const uint8_t hotPixelMode = ANDROID_HOT_PIXEL_MODE_OFF;
    UPDATE(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES, &hotPixelMode, 1);

    // android.jpeg
    const int32_t jpegAvailableThumbnailSizes[] = {0,   0,   176, 144, 240, 144, 256,
                                                   144, 240, 160, 256, 154, 240, 180};
    UPDATE(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES, jpegAvailableThumbnailSizes,
           ARRAY_SIZE(jpegAvailableThumbnailSizes));

    const int32_t jpegMaxSize = mCfg.maxJpegBufSize;
    UPDATE(ANDROID_JPEG_MAX_SIZE, &jpegMaxSize, 1);

    // android.lens
    const uint8_t focusDistanceCalibration =
            ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    UPDATE(ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION, &focusDistanceCalibration, 1);

    const uint8_t opticalStabilizationMode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
    UPDATE(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, &opticalStabilizationMode, 1);

    const uint8_t facing = ANDROID_LENS_FACING_EXTERNAL;
    UPDATE(ANDROID_LENS_FACING, &facing, 1);

    // android.noiseReduction
    const uint8_t noiseReductionMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
    UPDATE(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, &noiseReductionMode, 1);
    UPDATE(ANDROID_NOISE_REDUCTION_MODE, &noiseReductionMode, 1);

    const int32_t partialResultCount = 1;
    UPDATE(ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &partialResultCount, 1);

    // This means pipeline latency of X frame intervals. The maximum number is 4.
    const uint8_t requestPipelineMaxDepth = 4;
    UPDATE(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &requestPipelineMaxDepth, 1);

    // Three numbers represent the maximum numbers of different types of output
    // streams simultaneously. The types are raw sensor, processed (but not
    // stalling), and processed (but stalling). For usb limited mode, raw sensor
    // is not supported. Stalling stream is JPEG. Non-stalling streams are
    // YUV_420_888 or YV12.
    const int32_t requestMaxNumOutputStreams[] = {
            /*RAW*/ 0,
            /*Processed*/ HdmiDeviceSession::kMaxProcessedStream,
            /*Stall*/ HdmiDeviceSession::kMaxStallStream};
    UPDATE(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, requestMaxNumOutputStreams,
           ARRAY_SIZE(requestMaxNumOutputStreams));

    // Limited mode doesn't support reprocessing.
    const int32_t requestMaxNumInputStreams = 0;
    UPDATE(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, &requestMaxNumInputStreams, 1);

    // android.scaler
    // TODO: b/72263447 V4L2_CID_ZOOM_*
    const float scalerAvailableMaxDigitalZoom[] = {1};
    UPDATE(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, scalerAvailableMaxDigitalZoom,
           ARRAY_SIZE(scalerAvailableMaxDigitalZoom));

    const uint8_t croppingType = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;
    UPDATE(ANDROID_SCALER_CROPPING_TYPE, &croppingType, 1);

    const int32_t testPatternModes[] = {ANDROID_SENSOR_TEST_PATTERN_MODE_OFF,
                                        ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR};
    UPDATE(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES, testPatternModes,
           ARRAY_SIZE(testPatternModes));

    const uint8_t timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN;
    UPDATE(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE, &timestampSource, 1);

    // Orientation is a bit odd for hdmi, but consider it as the orientation
    // between the hdmi sensor (which is usually landscape) and the device's
    // natural display orientation. For devices with natural landscape display (ex: tablet/TV), the
    // orientation should be 0. For devices with natural portrait display (phone), the orientation
    // should be 270.
    const int32_t orientation = mCfg.orientation;
    UPDATE(ANDROID_SENSOR_ORIENTATION, &orientation, 1);

    // android.shading
    const uint8_t availableMode = ANDROID_SHADING_MODE_OFF;
    UPDATE(ANDROID_SHADING_AVAILABLE_MODES, &availableMode, 1);

    // android.statistics
    const uint8_t faceDetectMode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    UPDATE(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES, &faceDetectMode, 1);

    const int32_t maxFaceCount = 0;
    UPDATE(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, &maxFaceCount, 1);

    const uint8_t availableHotpixelMode = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
    UPDATE(ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES, &availableHotpixelMode, 1);

    const uint8_t lensShadingMapMode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
    UPDATE(ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES, &lensShadingMapMode, 1);

    // android.sync
    const int32_t maxLatency = ANDROID_SYNC_MAX_LATENCY_UNKNOWN;
    UPDATE(ANDROID_SYNC_MAX_LATENCY, &maxLatency, 1);

    /* Other sensor/RAW related keys:
     * android.sensor.info.colorFilterArrangement -> no need if we don't do RAW
     * android.sensor.info.physicalSize           -> not available
     * android.sensor.info.whiteLevel             -> not available/not needed
     * android.sensor.info.lensShadingApplied     -> not needed
     * android.sensor.info.preCorrectionActiveArraySize -> not available/not needed
     * android.sensor.blackLevelPattern           -> not available/not needed
     */

    const int32_t availableRequestKeys[] = {ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                                            ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                                            ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                                            ANDROID_CONTROL_AE_LOCK,
                                            ANDROID_CONTROL_AE_MODE,
                                            ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                                            ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                                            ANDROID_CONTROL_AF_MODE,
                                            ANDROID_CONTROL_AF_TRIGGER,
                                            ANDROID_CONTROL_AWB_LOCK,
                                            ANDROID_CONTROL_AWB_MODE,
                                            ANDROID_CONTROL_CAPTURE_INTENT,
                                            ANDROID_CONTROL_EFFECT_MODE,
                                            ANDROID_CONTROL_MODE,
                                            ANDROID_CONTROL_SCENE_MODE,
                                            ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                                            ANDROID_FLASH_MODE,
                                            ANDROID_JPEG_ORIENTATION,
                                            ANDROID_JPEG_QUALITY,
                                            ANDROID_JPEG_THUMBNAIL_QUALITY,
                                            ANDROID_JPEG_THUMBNAIL_SIZE,
                                            ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                                            ANDROID_NOISE_REDUCTION_MODE,
                                            ANDROID_SCALER_CROP_REGION,
                                            ANDROID_SENSOR_TEST_PATTERN_MODE,
                                            ANDROID_STATISTICS_FACE_DETECT_MODE,
                                            ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE};
    UPDATE(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, availableRequestKeys,
           ARRAY_SIZE(availableRequestKeys));

    const int32_t availableResultKeys[] = {ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                                           ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                                           ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                                           ANDROID_CONTROL_AE_LOCK,
                                           ANDROID_CONTROL_AE_MODE,
                                           ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                                           ANDROID_CONTROL_AE_STATE,
                                           ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                                           ANDROID_CONTROL_AF_MODE,
                                           ANDROID_CONTROL_AF_STATE,
                                           ANDROID_CONTROL_AF_TRIGGER,
                                           ANDROID_CONTROL_AWB_LOCK,
                                           ANDROID_CONTROL_AWB_MODE,
                                           ANDROID_CONTROL_AWB_STATE,
                                           ANDROID_CONTROL_CAPTURE_INTENT,
                                           ANDROID_CONTROL_EFFECT_MODE,
                                           ANDROID_CONTROL_MODE,
                                           ANDROID_CONTROL_SCENE_MODE,
                                           ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                                           ANDROID_FLASH_MODE,
                                           ANDROID_FLASH_STATE,
                                           ANDROID_JPEG_ORIENTATION,
                                           ANDROID_JPEG_QUALITY,
                                           ANDROID_JPEG_THUMBNAIL_QUALITY,
                                           ANDROID_JPEG_THUMBNAIL_SIZE,
                                           ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                                           ANDROID_NOISE_REDUCTION_MODE,
                                           ANDROID_REQUEST_PIPELINE_DEPTH,
                                           ANDROID_SCALER_CROP_REGION,
                                           ANDROID_SENSOR_TIMESTAMP,
                                           ANDROID_STATISTICS_FACE_DETECT_MODE,
                                           ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
                                           ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                                           ANDROID_STATISTICS_SCENE_FLICKER};
    UPDATE(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, availableResultKeys,
           ARRAY_SIZE(availableResultKeys));

    UPDATE(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS, AVAILABLE_CHARACTERISTICS_KEYS.data(),
           AVAILABLE_CHARACTERISTICS_KEYS.size());

    return OK;
}

status_t HdmiDevice::initCameraControlsCharsKeys(
        int, ::android::hardware::camera::common::V1_0::helper::CameraMetadata* metadata) {
    // android.sensor.info.sensitivityRange   -> V4L2_CID_ISO_SENSITIVITY
    // android.sensor.info.exposureTimeRange  -> V4L2_CID_EXPOSURE_ABSOLUTE
    // android.sensor.info.maxFrameDuration   -> TBD
    // android.lens.info.minimumFocusDistance -> V4L2_CID_FOCUS_ABSOLUTE
    // android.lens.info.hyperfocalDistance
    // android.lens.info.availableFocalLengths -> not available?

    // android.control
    // No AE compensation support for now.
    // TODO: V4L2_CID_EXPOSURE_BIAS
    const int32_t controlAeCompensationRange[] = {0, 0};
    UPDATE(ANDROID_CONTROL_AE_COMPENSATION_RANGE, controlAeCompensationRange,
           ARRAY_SIZE(controlAeCompensationRange));
    const camera_metadata_rational_t controlAeCompensationStep[] = {{0, 1}};
    UPDATE(ANDROID_CONTROL_AE_COMPENSATION_STEP, controlAeCompensationStep,
           ARRAY_SIZE(controlAeCompensationStep));

    // TODO: Check V4L2_CID_AUTO_FOCUS_*.
    const uint8_t afAvailableModes[] = {ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_MODE_OFF};
    UPDATE(ANDROID_CONTROL_AF_AVAILABLE_MODES, afAvailableModes, ARRAY_SIZE(afAvailableModes));

    // TODO: V4L2_CID_SCENE_MODE
    const uint8_t availableSceneMode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    UPDATE(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, &availableSceneMode, 1);

    // TODO: V4L2_CID_3A_LOCK
    const uint8_t aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE;
    UPDATE(ANDROID_CONTROL_AE_LOCK_AVAILABLE, &aeLockAvailable, 1);
    const uint8_t awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE;
    UPDATE(ANDROID_CONTROL_AWB_LOCK_AVAILABLE, &awbLockAvailable, 1);

    // TODO: V4L2_CID_ZOOM_*
    const float scalerAvailableMaxDigitalZoom[] = {1};
    UPDATE(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, scalerAvailableMaxDigitalZoom,
           ARRAY_SIZE(scalerAvailableMaxDigitalZoom));

    return OK;
}

status_t HdmiDevice::initOutputCharsKeys(
        int fd, ::android::hardware::camera::common::V1_0::helper::CameraMetadata* metadata) {
    initSupportedFormatsLocked(fd);
    if (mSupportedFormats.empty()) {
        ALOGE("%s: Init supported format list failed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    bool hasDepth = false;
    bool hasColor = false;
        bool hasColor_yuv = false;
    bool hasColor_nv12 = false;
    bool hasColor_h264 = false;

    // For V4L2_PIX_FMT_Z16
    std::array<int, /*size*/ 1> halDepthFormats{{HAL_PIXEL_FORMAT_Y16}};
    // For V4L2_PIX_FMT_NV24
    std::array<int, /*size*/ 3> halFormats{{HAL_PIXEL_FORMAT_BLOB, HAL_PIXEL_FORMAT_YCbCr_420_888,
                                            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED}};

    for (const auto& supportedFormat : mSupportedFormats) {
        switch (supportedFormat.fourcc) {
            case V4L2_PIX_FMT_Z16:
                hasDepth = true;
                break;
            case V4L2_PIX_FMT_NV24:
                hasColor_nv12 = true;
                break;
            case V4L2_PIX_FMT_YUYV:
                hasColor_yuv = true;
                break;
            case V4L2_PIX_FMT_NV12:
                hasColor_nv12 = true;
                break;
            case V4L2_PIX_FMT_H264:
                hasColor_h264 = true;
                break;
            case V4L2_PIX_FMT_NV16:
                hasColor_nv12 = true;
                break;
            case V4L2_PIX_FMT_BGR24 :
                hasColor_nv12 = true;
                break;
            default:
                ALOGW("%s: format %c%c%c%c is not supported!", __FUNCTION__,
                      supportedFormat.fourcc & 0xFF, (supportedFormat.fourcc >> 8) & 0xFF,
                      (supportedFormat.fourcc >> 16) & 0xFF, (supportedFormat.fourcc >> 24) & 0xFF);
        }
    }

    if (hasDepth) {
        status_t ret = initOutputCharsKeysByFormat(
                metadata, V4L2_PIX_FMT_Z16, halDepthFormats,
                ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS_OUTPUT,
                ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS,
                ANDROID_DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS,
                ANDROID_DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS);
        if (ret != OK) {
            ALOGE("%s: Unable to initialize depth format keys: %s", __FUNCTION__,
                  statusToString(ret).c_str());
            return ret;
        }
    }
    if (hasColor) {
        status_t ret =
                initOutputCharsKeysByFormat(metadata, V4L2_PIX_FMT_NV24, halFormats,
                                            ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
                                            ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                                            ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                                            ANDROID_SCALER_AVAILABLE_STALL_DURATIONS);
        if (ret != OK) {
            ALOGE("%s: Unable to initialize color format keys: %s", __FUNCTION__,
                  statusToString(ret).c_str());
            return ret;
        }
    } else if (hasColor_yuv) {
        initOutputCharsKeysByFormat(metadata, V4L2_PIX_FMT_YUYV, halFormats,
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                ANDROID_SCALER_AVAILABLE_STALL_DURATIONS);
    }
    if (hasColor_nv12) {
        initOutputCharsKeysByFormat(metadata, V4L2_PIX_FMT_NV12, halFormats,
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                ANDROID_SCALER_AVAILABLE_STALL_DURATIONS);
    }
    if (hasColor_h264) {
        initOutputCharsKeysByFormat(metadata, V4L2_PIX_FMT_H264, halFormats,
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                ANDROID_SCALER_AVAILABLE_STALL_DURATIONS);
    }

    status_t ret = calculateMinFps(metadata);
    if (ret != OK) {
        ALOGE("%s: Unable to update fps metadata: %s", __FUNCTION__, statusToString(ret).c_str());
        return ret;
    }

    SupportedV4L2Format maximumFormat{.width = 0, .height = 0};
    for (const auto& supportedFormat : mSupportedFormats) {
        if (supportedFormat.width >= maximumFormat.width &&
            supportedFormat.height >= maximumFormat.height) {
            maximumFormat = supportedFormat;
        }
    }
    int32_t activeArraySize[] = {0, 0, static_cast<int32_t>(maximumFormat.width),
                                 static_cast<int32_t>(maximumFormat.height)};
    UPDATE(ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE, activeArraySize,
           ARRAY_SIZE(activeArraySize));
    UPDATE(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, activeArraySize, ARRAY_SIZE(activeArraySize));

    int32_t pixelArraySize[] = {static_cast<int32_t>(maximumFormat.width),
                                static_cast<int32_t>(maximumFormat.height)};
    UPDATE(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArraySize, ARRAY_SIZE(pixelArraySize));
    return OK;
}


template <size_t SIZE>
status_t HdmiDevice::initOutputCharsKeysByFormat(
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* metadata,
        uint32_t fourcc, const std::array<int, SIZE>& halFormats, int streamConfigTag,
        int streamConfigurationKey, int minFrameDurationKey, int stallDurationKey) {
    if (mSupportedFormats.empty()) {
        ALOGE("%s: Init supported format list failed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    std::vector<int32_t> streamConfigurations;
    std::vector<int64_t> minFrameDurations;
    std::vector<int64_t> stallDurations;

    for (const auto& supportedFormat : mSupportedFormats) {
        if (supportedFormat.fourcc != fourcc) {
            // Skip 4CCs not meant for the halFormats
            continue;
        }
        for (const auto& format : halFormats) {
            streamConfigurations.push_back(format);
            streamConfigurations.push_back(supportedFormat.width);
            streamConfigurations.push_back(supportedFormat.height);
            streamConfigurations.push_back(streamConfigTag);
        }

        int64_t minFrameDuration = std::numeric_limits<int64_t>::max();
        for (const auto& fr : supportedFormat.frameRates) {
            // 1000000000LL < (2^32 - 1) and
            // fr.durationNumerator is uint32_t, so no overflow here
            int64_t frameDuration = 1000000000LL * fr.durationNumerator / fr.durationDenominator;
            if (frameDuration < minFrameDuration) {
                minFrameDuration = frameDuration;
            }
        }

        for (const auto& format : halFormats) {
            minFrameDurations.push_back(format);
            minFrameDurations.push_back(supportedFormat.width);
            minFrameDurations.push_back(supportedFormat.height);
            minFrameDurations.push_back(minFrameDuration);
        }

        // The stall duration is 0 for non-jpeg formats. For JPEG format, stall
        // duration can be 0 if JPEG is small. Here we choose 1 sec for JPEG.
        // TODO: b/72261675. Maybe set this dynamically
        for (const auto& format : halFormats) {
            const int64_t NS_TO_SECOND = 1E9;
            int64_t stall_duration = (format == HAL_PIXEL_FORMAT_BLOB) ? NS_TO_SECOND : 0;
            stallDurations.push_back(format);
            stallDurations.push_back(supportedFormat.width);
            stallDurations.push_back(supportedFormat.height);
            stallDurations.push_back(stall_duration);
        }
    }

    UPDATE(streamConfigurationKey, streamConfigurations.data(), streamConfigurations.size());

    UPDATE(minFrameDurationKey, minFrameDurations.data(), minFrameDurations.size());

    UPDATE(stallDurationKey, stallDurations.data(), stallDurations.size());

    return OK;
}

status_t HdmiDevice::calculateMinFps(
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* metadata) {
    std::set<int32_t> framerates;
    int32_t minFps = std::numeric_limits<int32_t>::max();

    for (const auto& supportedFormat : mSupportedFormats) {
        for (const auto& fr : supportedFormat.frameRates) {
            int32_t frameRateInt = static_cast<int32_t>(fr.getFramesPerSecond());
            if (minFps > frameRateInt) {
                minFps = frameRateInt;
            }
            framerates.insert(frameRateInt);
        }
    }

    std::vector<int32_t> fpsRanges;
    // FPS ranges
    for (const auto& framerate : framerates) {
        // Empirical: webcams often have close to 2x fps error and cannot support fixed fps range
        fpsRanges.push_back(framerate / 2);
        fpsRanges.push_back(framerate);
    }
    minFps /= 2;
    int64_t maxFrameDuration = 1000000000LL / minFps;

    UPDATE(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, fpsRanges.data(), fpsRanges.size());

    UPDATE(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &maxFrameDuration, 1);

    return OK;
}

#undef ARRAY_SIZE
#undef UPDATE

void HdmiDevice::getFrameRateList(int fd, double fpsUpperBound,
                                            SupportedV4L2Format* format) {
    format->frameRates.clear();

    v4l2_frmivalenum frameInterval{
            .index = 0,
            .pixel_format = format->fourcc,
            .width = static_cast<__u32>(format->width),
            .height = static_cast<__u32>(format->height),
    };

    for (frameInterval.index = 0;
         TEMP_FAILURE_RETRY(ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frameInterval)) == 0;
         ++frameInterval.index) {
        if (frameInterval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            if (frameInterval.discrete.numerator != 0) {
                SupportedV4L2Format::FrameRate fr = {frameInterval.discrete.numerator,
                                                     frameInterval.discrete.denominator};
                double framerate = fr.getFramesPerSecond();
                if (framerate > fpsUpperBound) {
                    continue;
                }
                ALOGV("index:%d, format:%c%c%c%c, w %d, h %d, framerate %f", frameInterval.index,
                      frameInterval.pixel_format & 0xFF, (frameInterval.pixel_format >> 8) & 0xFF,
                      (frameInterval.pixel_format >> 16) & 0xFF,
                      (frameInterval.pixel_format >> 24) & 0xFF, frameInterval.width,
                      frameInterval.height, framerate);
                format->frameRates.push_back(fr);
            }
        }
    }
    struct v4l2_capability capability;
    int ret_query = ioctl(fd, VIDIOC_QUERYCAP, &capability);
    if (ret_query < 0) {
        ALOGE("%s v4l2 QUERYCAP %s failed: %s", __FUNCTION__, strerror(errno));
    }

    if(strstr((const char*)capability.driver,"hdmi")){
        SupportedV4L2Format::FrameRate fr = {1,30};
        format->frameRates.push_back(fr);
    }

    if (format->frameRates.empty()) {
        ALOGE("%s: failed to get supported frame rates for format:%c%c%c%c w %d h %d", __FUNCTION__,
              frameInterval.pixel_format & 0xFF, (frameInterval.pixel_format >> 8) & 0xFF,
              (frameInterval.pixel_format >> 16) & 0xFF, (frameInterval.pixel_format >> 24) & 0xFF,
              frameInterval.width, frameInterval.height);
    }
}

void HdmiDevice::updateFpsBounds(
        int fd, CroppingType cropType,
        const std::vector<HdmiConfig::FpsLimitation>& fpsLimits,
        SupportedV4L2Format format, std::vector<SupportedV4L2Format>& outFmts) {
    double fpsUpperBound = -1.0;
    for (const auto& limit : fpsLimits) {
        if (cropType == VERTICAL) {
            if (format.width <= limit.size.width) {
                fpsUpperBound = limit.fpsUpperBound;
                break;
            }
        } else {  // HORIZONTAL
            if (format.height <= limit.size.height) {
                fpsUpperBound = limit.fpsUpperBound;
                break;
            }
        }
    }
    if (fpsUpperBound < 0.f) {
        return;
    }

    getFrameRateList(fd, fpsUpperBound, &format);
    if (!format.frameRates.empty()) {
        outFmts.push_back(format);
    }
}

std::vector<SupportedV4L2Format> HdmiDevice::getCandidateSupportedFormatsLocked(
        int fd, CroppingType cropType,
        const std::vector<HdmiConfig::FpsLimitation>& fpsLimits,
        const std::vector<HdmiConfig::FpsLimitation>& depthFpsLimits,
        const Size& minStreamSize, bool depthEnabled) {
    std::vector<SupportedV4L2Format> outFmts;
    struct v4l2_capability capability;
    int ret_query = ioctl(fd, VIDIOC_QUERYCAP, &capability);
    if (ret_query < 0) {
        ALOGE("%s v4l2 QUERYCAP %s failed: %s", __FUNCTION__, strerror(errno));
    }
    struct v4l2_fmtdesc fmtdesc {
        .index = 0, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE
    };
    if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    int ret = 0;
    while (ret == 0) {
        ret = TEMP_FAILURE_RETRY(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc));
        ALOGV("index:%d,ret:%d, format:%c%c%c%c", fmtdesc.index, ret, fmtdesc.pixelformat & 0xFF,
              (fmtdesc.pixelformat >> 8) & 0xFF, (fmtdesc.pixelformat >> 16) & 0xFF,
              (fmtdesc.pixelformat >> 24) & 0xFF);

        if (ret != 0 || (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED)) {
            // Skip if IOCTL failed, or if the format is emulated
            fmtdesc.index++;
            continue;
        }
        auto it =
                std::find(kSupportedFourCCs.begin(), kSupportedFourCCs.end(), fmtdesc.pixelformat);
        if (it == kSupportedFourCCs.end()) {
            fmtdesc.index++;
            continue;
        }
        // Found supported format
        v4l2_frmsizeenum frameSize{.index = 0, .pixel_format = fmtdesc.pixelformat};


        struct v4l2_dv_timings timings;

        if(TEMP_FAILURE_RETRY(ioctl(fd, VIDIOC_SUBDEV_QUERY_DV_TIMINGS, &timings)) == 0)
        {
            char fmtDesc[5]{0};
            sprintf(fmtDesc,"%c%c%c%c",
            fmtdesc.pixelformat & 0xFF,
            (fmtdesc.pixelformat >> 8) & 0xFF,
            (fmtdesc.pixelformat >> 16) & 0xFF,
            (fmtdesc.pixelformat >> 24) & 0xFF);
            ALOGV("hdmi index:%d,ret:%d, format:%s", fmtdesc.index, ret,fmtDesc);
            ALOGD("%s, hdmi I:%d, wxh:%dx%d", __func__,
            timings.bt.interlaced, timings.bt.width, timings.bt.height);

            ALOGV("add hdmi index:%d,ret:%d, format:%c%c%c%c", fmtdesc.index, ret,
            fmtdesc.pixelformat & 0xFF,
            (fmtdesc.pixelformat >> 8) & 0xFF,
            (fmtdesc.pixelformat >> 16) & 0xFF,
            (fmtdesc.pixelformat >> 24) & 0xFF);
            SupportedV4L2Format formatGet {
                .width = static_cast<int32_t>(timings.bt.width),
                .height = static_cast<int32_t>(timings.bt.height),
                .fourcc = fmtdesc.pixelformat
            };
            updateFpsBounds(fd, cropType, fpsLimits, formatGet, outFmts);

            SupportedV4L2Format format_640x480 {
                    .width = 640,
                    .height = 480,
                    .fourcc = fmtdesc.pixelformat
            };
            updateFpsBounds(fd, cropType, fpsLimits, format_640x480, outFmts);
            SupportedV4L2Format format_1920x1080 {
                    .width = 1920,
                    .height = 1080,
                    .fourcc = fmtdesc.pixelformat
            };
            updateFpsBounds(fd, cropType, fpsLimits, format_1920x1080, outFmts);
        }
        fmtdesc.index++;
    }

    trimSupportedFormats(cropType, &outFmts);
    return outFmts;
}

void HdmiDevice::trimSupportedFormats(CroppingType cropType,
                                                std::vector<SupportedV4L2Format>* pFmts) {
    std::vector<SupportedV4L2Format>& sortedFmts = *pFmts;
    if (cropType == VERTICAL) {
        std::sort(sortedFmts.begin(), sortedFmts.end(),
                  [](const SupportedV4L2Format& a, const SupportedV4L2Format& b) -> bool {
                      if (a.width == b.width) {
                          return a.height < b.height;
                      }
                      return a.width < b.width;
                  });
    } else {
        std::sort(sortedFmts.begin(), sortedFmts.end(),
                  [](const SupportedV4L2Format& a, const SupportedV4L2Format& b) -> bool {
                      if (a.height == b.height) {
                          return a.width < b.width;
                      }
                      return a.height < b.height;
                  });
    }

    if (sortedFmts.empty()) {
        ALOGE("%s: input format list is empty!", __FUNCTION__);
        return;
    }

    const auto& maxSize = sortedFmts[sortedFmts.size() - 1];
    float maxSizeAr = ASPECT_RATIO(maxSize);

    // Remove formats that has aspect ratio not croppable from largest size
    std::vector<SupportedV4L2Format> out;
    for (const auto& fmt : sortedFmts) {
        float ar = ASPECT_RATIO(fmt);
        if (isAspectRatioClose(ar, maxSizeAr)) {
            out.push_back(fmt);
        } else if (cropType == HORIZONTAL && ar < maxSizeAr) {
            out.push_back(fmt);
        } else if (cropType == VERTICAL && ar > maxSizeAr) {
            out.push_back(fmt);
        } else {
            ALOGV("%s: size (%d,%d) is removed due to unable to crop %s from (%d,%d)", __FUNCTION__,
                  fmt.width, fmt.height, cropType == VERTICAL ? "vertically" : "horizontally",
                  maxSize.width, maxSize.height);
        }
    }
    sortedFmts = out;
}

binder_status_t HdmiDevice::dump(int fd, const char** args, uint32_t numArgs) {
    std::shared_ptr<HdmiDeviceSession> session = mSession.lock();
    if (session == nullptr) {
        dprintf(fd, "No active camera device session instance\n");
        return STATUS_OK;
    }

    return session->dump(fd, args, numArgs);
}

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
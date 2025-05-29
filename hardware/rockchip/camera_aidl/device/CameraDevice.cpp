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

#define FAILURE_DEBUG_PREFIX "CameraDevice"

#include <charconv>
#include <string_view>

#include <system/camera_metadata.h>
#include <system/graphics.h>

#include "CameraDevice.h"
#include "CameraDeviceSession.h"
#include "debug.h"

#include <utils/Trace.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {
namespace {
constexpr char kCameraIdPrefix[] = "device@1.0/internal/";

const uint32_t kExtraResultKeys[] = {
    ANDROID_CONTROL_AE_STATE,
    ANDROID_CONTROL_AF_STATE,
    ANDROID_CONTROL_AWB_STATE,
    ANDROID_FLASH_STATE,
    ANDROID_LENS_FOCUS_DISTANCE,
    ANDROID_LENS_STATE,
    ANDROID_REQUEST_PIPELINE_DEPTH,
    ANDROID_SENSOR_TIMESTAMP, // populate with zero, CameraDeviceSession will put an actual value
    ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
    ANDROID_STATISTICS_SCENE_FLICKER,
};

std::vector<uint32_t> getSortedKeys(const CameraMetadataMap& m) {
    std::vector<uint32_t> keys;
    keys.reserve(m.size());
    for (const auto& [tag, unused] : m) {
        keys.push_back(tag);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

camera_metadata_enum_android_control_capture_intent_t
MapRequestTemplateToIntent(const RequestTemplate tpl) {
    switch (tpl) {
    case RequestTemplate::PREVIEW:
        return ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
    case RequestTemplate::STILL_CAPTURE:
        return ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
    case RequestTemplate::VIDEO_RECORD:
        return ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
    case RequestTemplate::VIDEO_SNAPSHOT:
        return ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
    case RequestTemplate::ZERO_SHUTTER_LAG:
        return ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
    case RequestTemplate::MANUAL:
        return ANDROID_CONTROL_CAPTURE_INTENT_MANUAL;
    default:
        return ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
    }
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

Status getStatus(int status) {
    switch (status) {
        case 0: return Status::OK;
        case -ENOSYS: return Status::OPERATION_NOT_SUPPORTED;
        case -EBUSY : return Status::CAMERA_IN_USE;
        case -EUSERS: return Status::MAX_CAMERAS_IN_USE;
        case -ENODEV: return Status::INTERNAL_ERROR;
        case -EINVAL: return Status::ILLEGAL_ARGUMENT;
        default:
            ALOGE("%s: unknown HAL status code %d", __FUNCTION__, status);
            return Status::INTERNAL_ERROR;
    }
}


}  // namespace

using aidl::android::hardware::camera::common::Status;
using hw::HwCameraFactoryProduct;

CameraDevice::CameraDevice(HwCameraFactoryProduct hwCamera,std::string cameraId)
        : mCameraId(cameraId),mHwCamera(std::move(hwCamera)) {

    mCameraIdInt = atoi(mCameraId.c_str());
    // Should not reach here as provider also validate ID
    if (mCameraIdInt < 0) {
        ALOGE("%s: Invalid camera id: %s", __FUNCTION__, mCameraId.c_str());
        mInitFail = true;
    } else if (mCameraIdInt >= mHwCamera->getModule()->getNumberOfCameras()) {
        ALOGI("%s: Adding a new camera id: %s", __FUNCTION__, mCameraId.c_str());
    }
}

CameraDevice::~CameraDevice() {}

ScopedAStatus CameraDevice::getCameraCharacteristics(CameraMetadata* metadata) {

    //Module 2.1+ codepath.
    struct camera_info info;
    int ret = mHwCamera->getModule()->getCameraInfo(mCameraIdInt, &info);
    if (ret == OK) {
        convertToAidl(info.static_camera_characteristics, metadata);
        return ScopedAStatus::ok();
    } else {
        ALOGE("%s: get camera info failed!", __FUNCTION__);
        return toScopedAStatus(Status::INTERNAL_ERROR);
    }
}

ScopedAStatus CameraDevice::getPhysicalCameraCharacteristics(
        const std::string& /*physicalCameraId*/, CameraMetadata* /*metadata*/) {
    //return toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
    return toScopedAStatus(FAILURE(Status::ILLEGAL_ARGUMENT));
}

ScopedAStatus CameraDevice::getResourceCost(CameraResourceCost* resCost) {
    struct camera_info info;
    int ret = mHwCamera->getModule()->getCameraInfo(mCameraIdInt, &info);
    if (ret == OK) {
        resCost->resourceCost = info.resource_cost;
        ALOGD("%s info.resource_cost:%d",__FUNCTION__,resCost->resourceCost);
    }
#if 0
    std::vector<std::string> conflicting_devices;


    int ret = mHwCamera->getModule()->getCameraInfo(mCameraIdInt, &info);
    if (ret == OK) {
        cost = info.resource_cost;
        for (size_t i = 0; i < info.conflicting_devices_length; i++) {
            std::string cameraId(info.conflicting_devices[i]);
            for (const auto& pair : mCameraDeviceNames) {
                if (cameraId == pair.first) {
                    conflicting_devices.push_back(pair.second);
                }
            }
        }
    } else {
        status = Status::INTERNAL_ERROR;
    }

    if (status == Status::OK) {
        resCost->resourceCost = cost;
        resCost->conflictingDevices.resize(conflicting_devices.size());
        for (size_t i = 0; i < conflicting_devices.size(); i++) {
            resCost->conflictingDevices[i] = conflicting_devices[i];
            ALOGV("CamDevice %s is conflicting with camDevice %s",
                    mCameraId.c_str(), resCost->conflictingDevices[i].c_str());
        }
    }
#endif
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDevice::isStreamCombinationSupported(
        const StreamConfiguration& cfg, bool* support) {
    *support = CameraDeviceSession::isStreamCombinationSupported(cfg, *mHwCamera);
    for (const auto& s : cfg.streams) {
        if((int)s.useCase > 0 && s.format == PixelFormat::YCBCR_420_888){
            *support = false;
            return ScopedAStatus::ok();
        }
    }
    ALOGE("%s,%d",__FUNCTION__,*support);
    return ScopedAStatus::ok();
}

Status CameraDevice::getAidlStatus(int status) {

    ALOGD("%s: HAl returned status: %d!", __FUNCTION__, status);
    switch (status) {
        case 0: return Status::OK;
        case -ENOSYS: return Status::OPERATION_NOT_SUPPORTED;
        case -EBUSY : return Status::CAMERA_IN_USE;
        case -EUSERS: return Status::MAX_CAMERAS_IN_USE;
        case -ENODEV: return Status::INTERNAL_ERROR;
        case -EINVAL: return Status::ILLEGAL_ARGUMENT;
        default:
            ALOGE("%s: unknown HAL status code %d", __FUNCTION__, status);
            return Status::INTERNAL_ERROR;
    }
}

ScopedAStatus CameraDevice::open(const std::shared_ptr<ICameraDeviceCallback>& callback,
        std::shared_ptr<ICameraDeviceSession>* session) {

    /** Open HAL device */
    status_t res;
    camera3_device_t *device;

    ATRACE_BEGIN("camera3->open");
    res = mHwCamera->getModule()->open(mCameraId.c_str(),
            reinterpret_cast<hw_device_t**>(&device));
    ATRACE_END();

    if (res != OK) {
        ALOGE("%s: cannot open camera %s!", __FUNCTION__, mCameraId.c_str());
        return toScopedAStatus(getAidlStatus(res));
    }
    mHwCamera->setDevice(device);
    *session = ndk::SharedRefBase::make<CameraDeviceSession>(
        mSelf.lock(), callback, *mHwCamera);
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDevice::openInjectionSession(
        const std::shared_ptr<ICameraDeviceCallback>& /*callback*/,
        std::shared_ptr<ICameraInjectionSession>* /*session*/) {
    return toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
}

ScopedAStatus CameraDevice::setTorchMode(const bool enable) {
     if (!mHwCamera->getModule()->isSetTorchModeSupported()) {
        toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
    }

    int ret =  mHwCamera->getModule()->setTorchMode(mCameraId.c_str(), enable);
    Status status = getStatus(ret);
    if (Status::OK != status)
    {
        return toScopedAStatus(status);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus CameraDevice::turnOnTorchWithStrengthLevel(const int32_t /*strength*/) {
    return toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
}

ScopedAStatus CameraDevice::getTorchStrengthLevel(int32_t* /*strength*/) {
    return toScopedAStatus(FAILURE(Status::OPERATION_NOT_SUPPORTED));
}

CameraMetadataMap CameraDevice::constructDefaultRequestSettings(const RequestTemplate tpl) const {
    using namespace std::literals;
    const auto sensorSize = mHwCamera->getSensorSize();
    const std::pair<int32_t, int32_t> fpsRange = mHwCamera->getDefaultTargetFpsRange(tpl);

    CameraMetadataMap m;

    m[ANDROID_COLOR_CORRECTION_ABERRATION_MODE] =
        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF;
    m[ANDROID_CONTROL_AE_ANTIBANDING_MODE] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    m[ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION] = int32_t(0);
    m[ANDROID_CONTROL_AE_LOCK] = ANDROID_CONTROL_AE_LOCK_OFF;
    m[ANDROID_CONTROL_AE_MODE] = (tpl == RequestTemplate::MANUAL) ?
        ANDROID_CONTROL_AE_MODE_OFF : ANDROID_CONTROL_AE_MODE_ON;
    m[ANDROID_CONTROL_AE_TARGET_FPS_RANGE]
        .add<int32_t>(fpsRange.first).add<int32_t>(fpsRange.second);
    m[ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER] = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
    m[ANDROID_CONTROL_AF_MODE] = (tpl == RequestTemplate::MANUAL) ?
        ANDROID_CONTROL_AF_MODE_OFF : ANDROID_CONTROL_AF_MODE_AUTO;
    m[ANDROID_CONTROL_AF_TRIGGER] = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    m[ANDROID_CONTROL_AWB_LOCK] = ANDROID_CONTROL_AWB_LOCK_OFF;
    m[ANDROID_CONTROL_AWB_MODE] = (tpl == RequestTemplate::MANUAL) ?
        ANDROID_CONTROL_AWB_MODE_OFF : ANDROID_CONTROL_AWB_MODE_AUTO;
    m[ANDROID_CONTROL_CAPTURE_INTENT] = MapRequestTemplateToIntent(tpl);
    m[ANDROID_CONTROL_EFFECT_MODE] = ANDROID_CONTROL_EFFECT_MODE_OFF;
    m[ANDROID_CONTROL_MODE] = (tpl == RequestTemplate::MANUAL) ?
        ANDROID_CONTROL_MODE_OFF : ANDROID_CONTROL_MODE_AUTO;
    m[ANDROID_CONTROL_SCENE_MODE] = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    m[ANDROID_CONTROL_VIDEO_STABILIZATION_MODE] = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    m[ANDROID_CONTROL_ZOOM_RATIO] = float(mHwCamera->getZoomRatioRange().first);

    m[ANDROID_EDGE_MODE] = ANDROID_EDGE_MODE_OFF;

    m[ANDROID_FLASH_MODE] = ANDROID_FLASH_MODE_OFF;

    m[ANDROID_HOT_PIXEL_MODE] = ANDROID_HOT_PIXEL_MODE_OFF;

    m[ANDROID_JPEG_ORIENTATION] = int32_t(0);
    m[ANDROID_JPEG_QUALITY] = uint8_t(85);
    m[ANDROID_JPEG_THUMBNAIL_QUALITY] = uint8_t(85);
    m[ANDROID_JPEG_THUMBNAIL_SIZE].add<int32_t>(0).add<int32_t>(0);

    m[ANDROID_LENS_APERTURE] = float(mHwCamera->getDefaultAperture());
    m[ANDROID_LENS_FOCAL_LENGTH] = float(mHwCamera->getDefaultFocalLength());
    m[ANDROID_LENS_FOCUS_DISTANCE] = float(mHwCamera->getMinimumFocusDistance());
    m[ANDROID_LENS_OPTICAL_STABILIZATION_MODE] =
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;

    m[ANDROID_NOISE_REDUCTION_MODE] = ANDROID_NOISE_REDUCTION_MODE_OFF;

    m[ANDROID_SENSOR_TEST_PATTERN_MODE] = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;

    m[ANDROID_REQUEST_FRAME_COUNT] = int32_t(0);
    m[ANDROID_REQUEST_ID] = int32_t(0);
    m[ANDROID_REQUEST_METADATA_MODE] = ANDROID_REQUEST_METADATA_MODE_FULL;
    m[ANDROID_REQUEST_TYPE] = ANDROID_REQUEST_TYPE_CAPTURE;

    m[ANDROID_SCALER_CROP_REGION]
        .add<int32_t>(0).add<int32_t>(0)
        .add<int32_t>(sensorSize.width - 1)
        .add<int32_t>(sensorSize.height - 1);

    m[ANDROID_SENSOR_EXPOSURE_TIME] = int64_t(mHwCamera->getDefaultSensorExpTime());
    m[ANDROID_SENSOR_FRAME_DURATION] = int64_t(mHwCamera->getDefaultSensorFrameDuration());
    m[ANDROID_SENSOR_SENSITIVITY] = int32_t(mHwCamera->getDefaultSensorSensitivity());

    m[ANDROID_STATISTICS_FACE_DETECT_MODE] = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    m[ANDROID_STATISTICS_SHARPNESS_MAP_MODE] = ANDROID_STATISTICS_SHARPNESS_MAP_MODE_OFF;
    m[ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE] = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
    m[ANDROID_STATISTICS_LENS_SHADING_MAP_MODE] = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;

    m[ANDROID_BLACK_LEVEL_LOCK] = ANDROID_BLACK_LEVEL_LOCK_OFF;
    m[ANDROID_DISTORTION_CORRECTION_MODE] = ANDROID_DISTORTION_CORRECTION_MODE_OFF;

    return m;
}

std::string CameraDevice::getPhysicalId(const int index) {
    char buf[sizeof(kCameraIdPrefix) + 8];
    snprintf(buf, sizeof(buf), "%s%d", kCameraIdPrefix, index);
    return buf;
}

std::optional<int> CameraDevice::parsePhysicalId(const std::string_view str) {
    if (str.size() < sizeof(kCameraIdPrefix)) {
        return FAILURE(std::nullopt);
    }

    if (memcmp(str.data(), kCameraIdPrefix, sizeof(kCameraIdPrefix) - 1) != 0) {
        return FAILURE(std::nullopt);
    }

    int index;
    const auto r = std::from_chars(&str[sizeof(kCameraIdPrefix) - 1],
                                   &*str.end(), index, 10);
    if (r.ec == std::errc()) {
        return index;
    } else {
        return FAILURE(std::nullopt);
    }
}

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

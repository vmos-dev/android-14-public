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

#define LOG_TAG "VirtualPrvdr"
// #define LOG_NDEBUG 0

#include "VirtualProvider.h"

#include <VirtualDevice.h>
#include <aidl/android/hardware/camera/common/Status.h>
#include <convert.h>
#include <cutils/properties.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <log/log.h>
#include <sys/inotify.h>
#include <regex>
#include <rockchip/hardware/hdmi/1.0/IHdmi.h>
namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {

using ::aidl::android::hardware::camera::common::Status;
using ::android::hardware::camera::device::implementation::VirtualDevice;
using ::android::hardware::camera::device::implementation::fromStatus;
using ::android::hardware::camera::external::common::VirtualConfig;
#define CLEAR(x) memset (&(x), 0, sizeof (x))
namespace {
// "device@<version>/virtual/<id>"
const std::regex kDeviceNameRE("device@([0-9]+\\.[0-9]+)/virtual/(.+)");
const int kMaxDevicePathLen = 256;
constexpr char kDevicePath[] = "/dev/";
constexpr char kPrefix[] = "video";
constexpr int kPrefixLen = sizeof(kPrefix) - 1;
constexpr int kDevicePrefixLen = sizeof(kDevicePath) + kPrefixLen - 1;

bool matchDeviceName(int cameraIdOffset, const std::string& deviceName, std::string* deviceVersion,
                     std::string* cameraDevicePath) {
    std::smatch sm;
    if (std::regex_match(deviceName, sm, kDeviceNameRE)) {
        if (deviceVersion != nullptr) {
            *deviceVersion = sm[1];
        }
        if (cameraDevicePath != nullptr) {
            *cameraDevicePath = std::to_string(std::stoi(sm[2]) - cameraIdOffset);
            ALOGD("%s deviceName:%s cameraDevicePath:%s cameraIdOffset:%d"
            ,__FUNCTION__
            ,deviceName.c_str()
            ,(*cameraDevicePath).c_str()
            ,cameraIdOffset);
        }
        return true;
    }
    return false;
}

}  // namespace

VirtualProvider::VirtualProvider() : mCfg(VirtualConfig::loadFromCfg()) {
    mHotPlugThread = std::make_shared<HotplugThread>(this);
    mHotPlugThread->run();
    deviceAdded("0");
    deviceAdded("1");
}

VirtualProvider::~VirtualProvider() {
    mHotPlugThread->requestExitAndWait();
}

ndk::ScopedAStatus VirtualProvider::setCallback(
        const std::shared_ptr<ICameraProviderCallback>& in_callback) {
    if (in_callback == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    {
        Mutex::Autolock _l(mLock);
        mCallback = in_callback;
    }

    for (const auto& pair : mCameraStatusMap) {
        mCallback->cameraDeviceStatusChange(pair.first, pair.second);
    }
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus VirtualProvider::getVendorTags(
        std::vector<VendorTagSection>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    // No vendor tag support for USB camera
    *_aidl_return = {};
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus VirtualProvider::getCameraIdList(std::vector<std::string>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    // Virtual HAL always report 0 camera, and extra cameras
    // are just reported via cameraDeviceStatusChange callbacks
    *_aidl_return = {};
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus VirtualProvider::getCameraDeviceInterface(
        const std::string& in_cameraDeviceName, std::shared_ptr<ICameraDevice>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    std::string cameraDevicePath, deviceVersion;
    bool match = matchDeviceName(mCfg.cameraIdOffset, in_cameraDeviceName, &deviceVersion,
                                 &cameraDevicePath);

    if (!match) {
        *_aidl_return = nullptr;
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    if (mCameraStatusMap.count(in_cameraDeviceName) == 0 ||
        mCameraStatusMap[in_cameraDeviceName] != CameraDeviceStatus::PRESENT) {
        *_aidl_return = nullptr;
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    ALOGV("Constructing virtual device");
    std::shared_ptr<VirtualDevice> deviceImpl =
            ndk::SharedRefBase::make<VirtualDevice>(cameraDevicePath, mCfg);
    if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
        ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraDevicePath.c_str());
        *_aidl_return = nullptr;
        return fromStatus(Status::INTERNAL_ERROR);
    }

    IF_ALOGV() {
        int interfaceVersion;
        deviceImpl->getInterfaceVersion(&interfaceVersion);
        ALOGV("%s: device interface version: %d", __FUNCTION__, interfaceVersion);
    }

    *_aidl_return = deviceImpl;
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus VirtualProvider::notifyDeviceStateChange(int64_t) {
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus VirtualProvider::getConcurrentCameraIds(
        std::vector<ConcurrentCameraIdCombination>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    *_aidl_return = {};
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus VirtualProvider::isConcurrentStreamCombinationSupported(
        const std::vector<CameraIdAndStreamCombination>&, bool* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    // No concurrent stream combinations are supported
    *_aidl_return = false;
    return fromStatus(Status::OK);
}

void VirtualProvider::addVritual(const char* devName) {
    ALOGV("%s: VirtualCam: adding %s to Virtual HAL!", __FUNCTION__, devName);
    Mutex::Autolock _l(mLock);
    std::string deviceName;
    std::string cameraId =
            std::to_string(mCfg.cameraIdOffset + std::atoi(devName));
    deviceName =
            std::string("device@") + VirtualDevice::kDeviceVersion + "/virtual/" + cameraId;
    mCameraStatusMap[deviceName] = CameraDeviceStatus::PRESENT;
    if (mCallback != nullptr) {
        mCallback->cameraDeviceStatusChange(deviceName, CameraDeviceStatus::PRESENT);
    }
}

void VirtualProvider::deviceAdded(const char* devName) {

    // See if we can initialize VirtualDevice correctly
    std::shared_ptr<VirtualDevice> deviceImpl =
            ndk::SharedRefBase::make<VirtualDevice>(devName, mCfg);
    if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
        ALOGW("%s: Attempt to init camera device %s failed!", __FUNCTION__, devName);
        return;
    }
    deviceImpl.reset();
    addVritual(devName);
}

void VirtualProvider::deviceRemoved(const char* devName) {
    Mutex::Autolock _l(mLock);
    std::string deviceName;
    std::string cameraId =
            std::to_string(mCfg.cameraIdOffset + std::atoi(devName + kDevicePrefixLen));

    deviceName =
            std::string("device@") + VirtualDevice::kDeviceVersion + "/virtual/" + cameraId;

    if (mCameraStatusMap.erase(deviceName) == 0) {
        // Unknown device, do not fire callback
        ALOGE("%s: cannot find camera device to remove %s", __FUNCTION__, devName);
        return;
    }

    if (mCallback != nullptr) {
        mCallback->cameraDeviceStatusChange(deviceName, CameraDeviceStatus::NOT_PRESENT);
    }
}

void VirtualProvider::updateAttachedCameras() {
    ALOGV("%s start scanning for existing devices", __FUNCTION__);

    // Find existing /dev/video* devices
    // DIR* devdir = opendir(kDevicePath);
    // if (devdir == nullptr) {
    //     ALOGE("%s: cannot open %s! Exiting threadloop", __FUNCTION__, kDevicePath);
    //     return;
    // }

    // struct dirent* de;
    // while ((de = readdir(devdir)) != nullptr) {
    //     // Find virtual v4l devices that's existing before we start watching and add them
    //     if (!strncmp(kPrefix, de->d_name, kPrefixLen)) {
    //         std::string deviceId(de->d_name + kPrefixLen);
    //         if (mCfg.mInternalDevices.count(deviceId) == 0) {
    //             ALOGV("Non-internal v4l device %s found", de->d_name);
    //             char v4l2DevicePath[kMaxDevicePathLen];
    //             snprintf(v4l2DevicePath, kMaxDevicePathLen, "%s%s", kDevicePath, de->d_name);
    //             deviceAdded(v4l2DevicePath);
    //         }
    //     }
    // }
    // closedir(devdir);
}

// Start VirtualProvider::HotplugThread functions

VirtualProvider::HotplugThread::HotplugThread(VirtualProvider* parent)
    : mParent(parent), mInternalDevices(parent->mCfg.mInternalDevices) {

    }

VirtualProvider::HotplugThread::~HotplugThread() {

}

bool VirtualProvider::HotplugThread::initialize() {
    // Update existing cameras
    mParent->updateAttachedCameras();


    mIsInitialized = true;
    return true;
}

bool VirtualProvider::HotplugThread::threadLoop() {
    // Initialize inotify descriptors if needed.
    if (!mIsInitialized && !initialize()) {
        return true;
    }

    // mParent->deviceAdded("1");

    return true;
}

// End VirtualProvider::HotplugThread functions

}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android

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

#define LOG_TAG "HdmiPrvdr"
// #define LOG_NDEBUG 0

#include "HdmiProvider.h"

#include <HdmiDevice.h>
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
using ::android::hardware::camera::device::implementation::HdmiDevice;
using ::android::hardware::camera::device::implementation::fromStatus;
using ::android::hardware::camera::external::common::HdmiConfig;
#define CLEAR(x) memset (&(x), 0, sizeof (x))
namespace {
// "device@<version>/hdmi/<id>"
const std::regex kDeviceNameRE("device@([0-9]+\\.[0-9]+)/hdmi/(.+)");
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
            *cameraDevicePath = "/dev/video" + std::to_string(std::stoi(sm[2]) - cameraIdOffset);
        }
        return true;
    }
    return false;
}
class IHdmiRxStatusCallbackImpl:public ::rockchip::hardware::hdmi::V1_0::IHdmiRxStatusCallback{
    public:
    IHdmiRxStatusCallbackImpl(int fd):mFd(fd){
        ALOGD("@%s,fd:%d",__FUNCTION__,mFd);
    }
    Return<void> getHdmiRxStatus(getHdmiRxStatus_cb _hidl_cb){
        rockchip::hardware::hdmi::V1_0::HdmiStatus status;
        struct v4l2_dv_timings timings;
        int err = ioctl(mFd, VIDIOC_SUBDEV_QUERY_DV_TIMINGS, &timings);
        if (err < 0) {
            ALOGD("get VIDIOC_SUBDEV_QUERY_DV_TIMINGS failed ,%d(%s)", errno, strerror(errno));
            _hidl_cb(status);
            return Void();
        }

        const struct v4l2_bt_timings *bt =&timings.bt;
        double tot_width, tot_height;
        tot_height = bt->height +
            bt->vfrontporch + bt->vsync + bt->vbackporch +
            bt->il_vfrontporch + bt->il_vsync + bt->il_vbackporch;
        tot_width = bt->width +
            bt->hfrontporch + bt->hsync + bt->hbackporch;
        ALOGD("%s:%dx%d, pixelclock:%lld Hz, %.2f fps", __func__,
        timings.bt.width, timings.bt.height,
        timings.bt.pixelclock,static_cast<double>(bt->pixelclock) /(tot_width * tot_height));
        status.width = timings.bt.width;
        status.height = timings.bt.height;
        status.fps = round(static_cast<double>(bt->pixelclock) /(tot_width * tot_height));
        status.status = 1;
        _hidl_cb(status);
        return Void();
    }
    private:
    int mFd;
};

}  // namespace

HdmiProvider::HdmiProvider() : mCfg(HdmiConfig::loadFromCfg()) {
    mHotPlugThread = std::make_shared<HotplugThread>(this);
    mHotPlugThread->run();
}

HdmiProvider::~HdmiProvider() {
    mHotPlugThread->requestExitAndWait();
}

ndk::ScopedAStatus HdmiProvider::setCallback(
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

ndk::ScopedAStatus HdmiProvider::getVendorTags(
        std::vector<VendorTagSection>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    // No vendor tag support for USB camera
    *_aidl_return = {};
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiProvider::getCameraIdList(std::vector<std::string>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    // Hdmi HAL always report 0 camera, and extra cameras
    // are just reported via cameraDeviceStatusChange callbacks
    *_aidl_return = {};
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiProvider::getCameraDeviceInterface(
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

    ALOGV("Constructing hdmi device");
    std::shared_ptr<HdmiDevice> deviceImpl =
            ndk::SharedRefBase::make<HdmiDevice>(cameraDevicePath, mCfg);
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

ndk::ScopedAStatus HdmiProvider::notifyDeviceStateChange(int64_t) {
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiProvider::getConcurrentCameraIds(
        std::vector<ConcurrentCameraIdCombination>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    *_aidl_return = {};
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus HdmiProvider::isConcurrentStreamCombinationSupported(
        const std::vector<CameraIdAndStreamCombination>&, bool* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    // No concurrent stream combinations are supported
    *_aidl_return = false;
    return fromStatus(Status::OK);
}

void HdmiProvider::addHdmi(const char* devName) {
    ALOGV("%s: ExtCam: adding %s to Hdmi HAL!", __FUNCTION__, devName);
    Mutex::Autolock _l(mLock);
    std::string deviceName;
    std::string cameraId =
            std::to_string(mCfg.cameraIdOffset + std::atoi(devName + kDevicePrefixLen));
    deviceName =
            std::string("device@") + HdmiDevice::kDeviceVersion + "/hdmi/" + cameraId;
    mCameraStatusMap[deviceName] = CameraDeviceStatus::PRESENT;
    if (mCallback != nullptr) {
        mCallback->cameraDeviceStatusChange(deviceName, CameraDeviceStatus::PRESENT);
    }
}

void HdmiProvider::deviceAdded(const char* devName) {
    {
        base::unique_fd fd(::open(devName, O_RDWR));
        if (fd.get() < 0) {
            ALOGE("%s open v4l2 device %s failed:%s", __FUNCTION__, devName, strerror(errno));
            return;
        }

        struct v4l2_capability capability;
        int ret = ioctl(fd.get(), VIDIOC_QUERYCAP, &capability);
        if (ret < 0) {
            ALOGE("%s v4l2 QUERYCAP %s failed", __FUNCTION__, devName);
            return;
        }

        if (!((capability.device_caps & V4L2_CAP_VIDEO_CAPTURE) || (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE))) {
            ALOGW("%s device %s does not support VIDEO_CAPTURE", __FUNCTION__, devName);
            return;
        }
        if(!strstr((const char*)capability.driver,"hdmi")){
            ALOGW("driver.find :%s skip",capability.driver);
            return ;
        }
        ALOGD("driver.find :%s",capability.driver);
        mDevName = devName;
    }

    // See if we can initialize HdmiDevice correctly
    std::shared_ptr<HdmiDevice> deviceImpl =
            ndk::SharedRefBase::make<HdmiDevice>(devName, mCfg);
    if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
        ALOGW("%s: Attempt to init camera device %s failed!", __FUNCTION__, devName);
        return;
    }
    deviceImpl.reset();
    addHdmi(devName);
}

void HdmiProvider::deviceRemoved(const char* devName) {
    Mutex::Autolock _l(mLock);
    std::string deviceName;
    std::string cameraId =
            std::to_string(mCfg.cameraIdOffset + std::atoi(devName + kDevicePrefixLen));

    deviceName =
            std::string("device@") + HdmiDevice::kDeviceVersion + "/hdmi/" + cameraId;

    if (mCameraStatusMap.erase(deviceName) == 0) {
        // Unknown device, do not fire callback
        ALOGE("%s: cannot find camera device to remove %s", __FUNCTION__, devName);
        return;
    }

    if (mCallback != nullptr) {
        mCallback->cameraDeviceStatusChange(deviceName, CameraDeviceStatus::NOT_PRESENT);
    }
}

void HdmiProvider::updateAttachedCameras() {
    ALOGV("%s start scanning for existing V4L2 devices", __FUNCTION__);

    // Find existing /dev/video* devices
    DIR* devdir = opendir(kDevicePath);
    if (devdir == nullptr) {
        ALOGE("%s: cannot open %s! Exiting threadloop", __FUNCTION__, kDevicePath);
        return;
    }

    struct dirent* de;
    while ((de = readdir(devdir)) != nullptr) {
        // Find hdmi v4l devices that's existing before we start watching and add them
        if (!strncmp(kPrefix, de->d_name, kPrefixLen)) {
            std::string deviceId(de->d_name + kPrefixLen);
            if (mCfg.mInternalDevices.count(deviceId) == 0) {
                ALOGV("Non-internal v4l device %s found", de->d_name);
                char v4l2DevicePath[kMaxDevicePathLen];
                snprintf(v4l2DevicePath, kMaxDevicePathLen, "%s%s", kDevicePath, de->d_name);
                deviceAdded(v4l2DevicePath);
            }
        }
    }
    closedir(devdir);
}

// Start HdmiProvider::HotplugThread functions

HdmiProvider::HotplugThread::HotplugThread(HdmiProvider* parent)
    : mParent(parent), mInternalDevices(parent->mCfg.mInternalDevices) {

    }

HdmiProvider::HotplugThread::~HotplugThread() {
    write(mPipeFd[1], "q", 1);
    close(mPipeFd[0]);
    close(mPipeFd[1]);
    // Clean up inotify descriptor if needed.
    if (mFd >= 0) {
        close(mFd);
    }
}

bool HdmiProvider::HotplugThread::initialize() {
    // Update existing cameras
    mParent->updateAttachedCameras();
    mDevName =  mParent->getDevName();
    pipe(mPipeFd);
    // ::open(devName, O_RDWR);
    ALOGD("open dev:%s",mDevName.c_str());
    mFd = ::open(mDevName.c_str(), O_RDWR);
    if (mFd < 0) {
        ALOGE("%s: inotify init failed! Exiting threadloop", __FUNCTION__);
        return false;
    }

    static IHdmiRxStatusCallbackImpl  IHdmiRxStatusCallbackImpl(mFd);
    sp<rockchip::hardware::hdmi::V1_0::IHdmi> client = rockchip::hardware::hdmi::V1_0::IHdmi::getService();
    if(client.get()!= nullptr){
        std::string cameraId =
            std::to_string(mParent->mCfg.cameraIdOffset + std::atoi(mDevName.c_str() + kDevicePrefixLen));
        ALOGD("foundHdmiDevice:%s, cameraIdOffset:%d",cameraId.c_str(),mParent->mCfg.cameraIdOffset);
        const ::android::sp<rockchip::hardware::hdmi::V1_0::IHdmiRxStatusCallback> cb = &IHdmiRxStatusCallbackImpl;
        client->foundHdmiDevice(::android::hardware::hidl_string(cameraId),cb);
    }

    struct v4l2_event_subscription sub;
    int ret(0);
    CLEAR(sub);
    sub.type = V4L2_EVENT_SOURCE_CHANGE;

    ret = ioctl(mFd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    if (ret < 0) {
        ALOGE("%s: error subscribing event %x: %s", __FUNCTION__, sub.type, strerror(errno));
        return false;
    }

    sub.type = V4L2_EVENT_CTRL;
    sub.id = V4L2_CID_DV_RX_POWER_PRESENT;
    ret = ioctl(mFd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    if (ret < 0) {
        ALOGE("%s: error subscribing event %x: %s", __FUNCTION__, sub.type, strerror(errno));
        return false;
    }

    mIsInitialized = true;
    return true;
}

bool HdmiProvider::HotplugThread::threadLoop() {
    // Initialize inotify descriptors if needed.
    if (!mIsInitialized && !initialize()) {
        return true;
    }

    struct pollfd fds[2];
    fds[0].fd = mPipeFd[0];
    fds[0].events = POLLIN;

    fds[1].fd = mFd;
    fds[1].events = POLLPRI;
    struct v4l2_event ev;
    CLEAR(ev);

    if (poll(fds, 2, 5000) < 0) {
        ALOGD("%d: poll failed: %s\n", mFd, strerror(errno));
        return false;
    }
    if (fds[0].revents & POLLIN) {
        ALOGD("%d: quit message received\n", mFd);
        return false;
    }
    if (fds[1].revents & POLLPRI) {
        if (ioctl(fds[1].fd, VIDIOC_DQEVENT, &ev) == 0) {
            switch (ev.type) {
                case V4L2_EVENT_SOURCE_CHANGE:
                    {
                        ALOGD("%d: V4L2_EVENT_SOURCE_CHANGE value:%d\n", mFd,1);
                        mParent->deviceAdded(mDevName.c_str());
                    }
                    break;
                case V4L2_EVENT_CTRL:{
                        struct v4l2_event_ctrl* ctrl =(struct v4l2_event_ctrl*) &(ev.u);
                        ALOGD("%d:  V4L2_EVENT_CTRL event value:%d \n", mFd,ctrl->value);
                        if(ctrl->value){
                            mParent->deviceAdded(mDevName.c_str());
                        }else{
                            mParent->deviceRemoved(mDevName.c_str());
                        }
                    }
                    break;
                default:
                    ALOGD("%d: unknown event :%d\n", mFd,ev.type);
                    break;
            }
        } else {
            ALOGD("%d: VIDIOC_DQEVENT failed: %s\n",mFd, strerror(errno));
        }
    }
    return true;
}

// End HdmiProvider::HotplugThread functions

}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android

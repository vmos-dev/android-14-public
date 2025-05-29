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

#include <inttypes.h>

#include <log/log.h>

#include "CameraProvider.h"
#include "CameraDevice.h"
#include "HwCamera.h"
#include "debug.h"
#include "VendorTagDescriptor.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {
namespace {
ndk::ScopedAStatus toScopedAStatus(const aidl::android::hardware::camera::common::Status s) {
    return ndk::ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(s));
}
}
using aidl::android::hardware::camera::common::Status;
using ::aidl::android::hardware::camera::common::CameraMetadataType;
using ::android::hardware::camera::common::V1_0::helper::VendorTagDescriptor;
//using android::hardware::device::implementation::CameraDevice;


CameraProvider::CameraProvider(const int deviceIdBase,
                               Span<const device::implementation::hw::HwCameraFactory> availableCameras)
        : camera_module_callbacks_t({sCameraDeviceStatusChange,
                                   sTorchModeStatusChange})
        ,mDeviceIdBase(deviceIdBase)
        , mAvailableCameras(availableCameras) {
    for (int i = 0; i < mAvailableCameras.size(); ++i) {
        auto hwCamera = mAvailableCameras[i]();
        if (hwCamera) {
            mModule =  hwCamera->getModule();
        }
    }
    if (mModule!=nullptr)
    {
        mModule->setCallbacks(this);
    }
}

CameraProvider::~CameraProvider() {}

ScopedAStatus CameraProvider::setCallback(
        const std::shared_ptr<ICameraProviderCallback>& callback) {
    if (callback == nullptr) {
        return toScopedAStatus(FAILURE(Status::ILLEGAL_ARGUMENT));
    }
    mCallback = callback;
    if (mModule!=nullptr)
    {
        mModule->setCallbacks(this);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus CameraProvider::getVendorTags(std::vector<VendorTagSection>* vts) {
    *vts = {};
    sp<VendorTagDescriptor> desc = VendorTagDescriptor::getGlobalVendorTagDescriptor();
    if (desc)
    {
        auto  sectionNames = desc->getAllSectionNames();
        size_t numSections = sectionNames->size();
        std::vector<std::vector<VendorTag>> tagsBySection(numSections);
        int tagCount = desc->getTagCount();
        std::vector<uint32_t> tags(tagCount);
        desc->getTagArray(tags.data());
        for (int i = 0; i < tagCount; i++) {
            VendorTag vt;
            vt.tagId = tags[i];
            vt.tagName = desc->getTagName(tags[i]);
            vt.tagType = (CameraMetadataType)desc->getTagType(tags[i]);
            ssize_t sectionIdx = desc->getSectionIndex(tags[i]);
            tagsBySection[sectionIdx].push_back(vt);
        }
        vts->resize(numSections);
        for (size_t s = 0; s < numSections; s++) {
            (*vts)[s].sectionName = (*sectionNames)[s].string();
            (*vts)[s].tags = tagsBySection[s];
        }
    }
    return ScopedAStatus::ok();
}

ScopedAStatus CameraProvider::getCameraIdList(std::vector<std::string>* camera_ids) {
    camera_ids->reserve(mAvailableCameras.size());

    for (int i = 0; i < mAvailableCameras.size(); ++i) {
        camera_ids->push_back(CameraDevice::getPhysicalId(mDeviceIdBase + i));
    }

    return ScopedAStatus::ok();
}

ScopedAStatus CameraProvider::getCameraDeviceInterface(
        const std::string& name,
        std::shared_ptr<ICameraDevice>* device) {
    const std::optional<int> maybeIndex = CameraDevice::parsePhysicalId(name);
    if (!maybeIndex) {
        return toScopedAStatus(FAILURE(Status::ILLEGAL_ARGUMENT));
    }

    const int index = maybeIndex.value() - mDeviceIdBase;
    if ((index >= 0) && (index < mAvailableCameras.size())) {
        auto hwCamera = mAvailableCameras[index]();
        if (hwCamera) {
            auto p = ndk::SharedRefBase::make<CameraDevice>(std::move(hwCamera),hwCamera->getCameraId());
            p->mSelf = p;
            *device = std::move(p);
            return ScopedAStatus::ok();
        } else {
            return toScopedAStatus(FAILURE(Status::INTERNAL_ERROR));
        }
    } else {
        return toScopedAStatus(FAILURE(Status::ILLEGAL_ARGUMENT));
    }
}

ScopedAStatus CameraProvider::notifyDeviceStateChange(const int64_t /*deviceState*/) {
    return ScopedAStatus::ok();
}

ScopedAStatus CameraProvider::getConcurrentCameraIds(
        std::vector<ConcurrentCameraIdCombination>* concurrentCameraIds) {
    *concurrentCameraIds = {};
    return ScopedAStatus::ok();
}

ScopedAStatus CameraProvider::isConcurrentStreamCombinationSupported(
        const std::vector<CameraIdAndStreamCombination>& /*configs*/,
        bool* support) {
    *support = false;
    return ScopedAStatus::ok();
}

/**
 * static callback forwarding methods from HAL to instance
 */
void CameraProvider::sCameraDeviceStatusChange(
        const struct camera_module_callbacks* callbacks,
        int camera_id,
        int new_status) {
    CameraProvider* cp = const_cast<CameraProvider*>(
            static_cast<const CameraProvider*>(callbacks));
    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    Mutex::Autolock _l(cp->mCbLock);
    if (cp->mCallback == nullptr) {
        // For camera connected before mCallback is set, the corresponding
        // addDeviceNames() would be called later in setCallbacks().
        return;
    }

    for (int i = 0; i < cp->mAvailableCameras.size(); ++i) {
        auto hwCamera = cp->mAvailableCameras[i]();
        if (hwCamera) {
          std::string id =  hwCamera->getCameraId();
          if (camera_id == atoi(id.c_str()) )
          {
            std::string cameraIdStr = CameraDevice::getPhysicalId(cp->mDeviceIdBase + camera_id);
            CameraDeviceStatus status = (CameraDeviceStatus)new_status;
            cp->mCallback->cameraDeviceStatusChange(cameraIdStr, status);
          }
        }
    }
}

void CameraProvider::sTorchModeStatusChange(
        const struct camera_module_callbacks* callbacks,
        const char* camera_id,
        int new_status) {
    CameraProvider* cp = const_cast<CameraProvider*>(
            static_cast<const CameraProvider*>(callbacks));

    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    Mutex::Autolock _l(cp->mCbLock);
    if (cp->mCallback != nullptr) {
        std::string cameraIdStr(camera_id);
        for (int i = 0; i < cp->mAvailableCameras.size(); ++i) {
            auto hwCamera = cp->mAvailableCameras[i]();
            if (hwCamera) {
                std::string id = hwCamera->getCameraId();
                if (cameraIdStr.compare(id) == 0)
                {
                    cameraIdStr = CameraDevice::getPhysicalId(cp->mDeviceIdBase + atoi(camera_id));
                    TorchModeStatus status = (TorchModeStatus) new_status;
                    cp->mCallback->torchModeStatusChange(cameraIdStr, status);
                }
            }
        }
    }
}


}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android

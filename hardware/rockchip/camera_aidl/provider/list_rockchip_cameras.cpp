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

#include <algorithm>
#include <charconv>
#include <numeric>
#include <string_view>
#include <math.h>

#include <log/log.h>

#include "debug.h"
#include "list_rockchip_cameras.h"
#include "RkCamera.h"

#include "VendorTagDescriptor.h"
#include "CameraMetadata.h"


const int kMaxCameraIdLen = 16;

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {
namespace hw {
namespace {

using ::android::hardware::camera::common::V1_0::helper::VendorTagDescriptor;
using android::hardware::camera::device::implementation::Rect;
using android::hardware::camera::device::implementation::hw::RkCamera;
using common::helper::CameraModule;


bool findToken(const std::string_view str, const std::string_view key, std::string_view* value) {
    size_t pos = 0;
    while (true) {
        pos = str.find(key, pos);
        if (pos == std::string_view::npos) {
            return false;
        } else if ((!pos || str[pos - 1] == ' ') &&
                (str.size() >= (pos + key.size() + 1)) &&
                (str[pos + key.size()] == '=')) {
            const size_t vbegin = pos + key.size() + 1;
            const size_t vend = str.find(' ', vbegin);
            if (vend == std::string_view::npos) {
                *value = str.substr(vbegin, str.size() - vbegin);
            } else {
                *value = str.substr(vbegin, vend - vbegin);
            }
            return true;
        } else {
            ++pos;
        }
    }
}

bool parseResolutions(const std::string_view str, std::vector<Rect<uint16_t>>* supportedResolutions) {
    const char* i = &*str.begin();
    const char* const end = &*str.end();
    if (i == end) {
        return FAILURE(false);
    }

    while (true) {
        Rect<uint16_t> resolution;
        std::from_chars_result r =  std::from_chars(i, end, resolution.width, 10);
        if (r.ec != std::errc()) {
            return FAILURE(false);
        }
        i = r.ptr;
        if (i == end) {
            return FAILURE(false);
        }
        if (*i != 'x') {
            return FAILURE(false);
        }
        r =  std::from_chars(i + 1, end, resolution.height, 10);
        if (r.ec != std::errc()) {
            return FAILURE(false);
        }
        i = r.ptr;

        if ((resolution.width > 0) && (resolution.height > 0)) {
            supportedResolutions->push_back(resolution);
        }

        if (i == end) {
            break;
        } else {
            if (*i == ',') {
                ++i;
            } else {
                return FAILURE(false);
            }
        }
    }

    return true;
}

Rect<uint16_t> calcThumbnailResolution(const double aspectRatio,
                                       const size_t targetArea) {
    // round to a multiple of 16, a tad down
    const uint16_t height =
        ((uint16_t(sqrt(targetArea / aspectRatio)) + 7) >> 4) << 4;

    // round width to be even
    const uint16_t width = (uint16_t(height * aspectRatio + 1) >> 1) << 1;

    return {width, height};
}

struct RectAreaComparator {
    bool operator()(Rect<uint16_t> lhs, Rect<uint16_t> rhs) const {
        const size_t lArea = lhs.area();
        const size_t rArea = rhs.area();

        if (lArea < rArea) {
            return true;
        } else if (lArea > rArea) {
            return false;
        } else {
            return lhs.width < rhs.width;
        }
    }
};

} // namespace


bool setUpVendorTags(sp<CameraModule> module){
    vendor_tag_ops_t vOps = vendor_tag_ops_t();

    // Check if vendor operations have been implemented
    if (!module->isVendorTagDefined()) {
        ALOGI("%s: No vendor tags defined for this device.", __FUNCTION__);
        return true;
    }

    module->getVendorTagOps(&vOps);

    // Ensure all vendor operations are present
    if (vOps.get_tag_count == nullptr || vOps.get_all_tags == nullptr ||
            vOps.get_section_name == nullptr || vOps.get_tag_name == nullptr ||
            vOps.get_tag_type == nullptr) {
        ALOGE("%s: Vendor tag operations not fully defined. Ignoring definitions."
               , __FUNCTION__);
        return false;
    }

    // Read all vendor tag definitions into a descriptor
    sp<VendorTagDescriptor> desc;
    status_t res;
    if ((res = VendorTagDescriptor::createDescriptorFromOps(&vOps, /*out*/desc))
            != OK) {
        ALOGE("%s: Could not generate descriptor from vendor tag operations,"
              "received error %s (%d). Camera clients will not be able to use"
              "vendor tags", __FUNCTION__, strerror(res), res);
        return false;
    }
    // Set the global descriptor to use with camera metadata
    VendorTagDescriptor::setAsGlobalVendorTagDescriptor(desc);
    // const SortedVector<String8>* sectionNames = desc->getAllSectionNames();
    // size_t numSections = sectionNames->size();
    // std::vector<std::vector<VendorTag>> tagsBySection(numSections);
    // int tagCount = desc->getTagCount();
    // std::vector<uint32_t> tags(tagCount);
    // desc->getTagArray(tags.data());
    // for (int i = 0; i < tagCount; i++) {
    //     VendorTag vt;
    //     vt.tagId = tags[i];
    //     vt.tagName = desc->getTagName(tags[i]);
    //     vt.tagType = (CameraMetadataType) desc->getTagType(tags[i]);
    //     ssize_t sectionIdx = desc->getSectionIndex(tags[i]);
    //     tagsBySection[sectionIdx].push_back(vt);
    // }
    return true;
}

bool listRkCameras(const std::function<void(HwCameraFactory)>& cameraSink) {
    using namespace std::literals;
    camera_module_t *rawModule;
    int err = hw_get_module(CAMERA_HARDWARE_MODULE_ID,
            (const hw_module_t **)&rawModule);
    if (err < 0) {
        ALOGE("Could not load camera HAL module: %d (%s)", err, strerror(-err));
        return true;
    }

    sp<CameraModule> module = new CameraModule(rawModule);
    err = module->init();
    if (err != OK) {
        ALOGE("Could not initialize camera HAL module: %d (%s)", err, strerror(-err));
        module.clear();
        return true;
    }
    ALOGD("Loaded \"%s\" camera module", module->getModuleName());
    VendorTagDescriptor::clearGlobalVendorTagDescriptor();

    if (!setUpVendorTags(module)) {
        ALOGE("%s: Vendor tag setup failed, will not be available.", __FUNCTION__);
    }
    int cameraNumber = module->getNumberOfCameras();
    ALOGD("cameraNumber:%d",cameraNumber);
    for (int i = 0; i < cameraNumber; i++) {
        struct camera_info info;
        auto rc = module->getCameraInfo(i, &info);
        if (rc != NO_ERROR) {
            ALOGE("%s: Camera info query failed!", __func__);
            module.clear();
            return true;
        }
        char cameraId[kMaxCameraIdLen];
        snprintf(cameraId, sizeof(cameraId), "%d", i);
        std::string cameraIdStr(cameraId);
        cameraSink([module = module,cameraIdStr]() {
            return std::make_unique<RkCamera>(module,cameraIdStr);
        });
    }

    return true;
}

}  // namespace hw
}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android

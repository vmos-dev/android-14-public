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

#include <log/log.h>
#include "HwCamera.h"
#include "list_rockchip_cameras.h"
#include "service_entry.h"
#include <cutils/properties.h>

int main() {
    using ::android::hardware::camera::provider::implementation::hw::HwCameraFactory;
    using ::android::hardware::camera::provider::implementation::hw::listRkCameras;
    using ::android::hardware::camera::provider::implementation::Span;
    using ::android::hardware::camera::provider::implementation::serviceEntry;

    std::vector<HwCameraFactory> availableCameras;
    {
        const auto cameraAppender = [&availableCameras](HwCameraFactory c){
            availableCameras.push_back(std::move(c));
        };

        listRkCameras(cameraAppender);
    }

    int32_t deviceIdBase = property_get_int32("persist.vendor.camera.idbase", /*default*/0);
#ifdef CAMERA_SUPPORT_VIRTUAL
    if(deviceIdBase == 0){
        property_set("persist.vendor.camera.idbase", "2");
        deviceIdBase = 2;
    }
#endif
    return serviceEntry(
        deviceIdBase,
        Span<const HwCameraFactory>(availableCameras.begin(),
                                    availableCameras.end()),
        4);
}

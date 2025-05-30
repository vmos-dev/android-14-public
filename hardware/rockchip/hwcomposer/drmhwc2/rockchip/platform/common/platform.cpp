/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "hwc-platform"

#include "platform.h"
#include "drmdevice.h"
#include "rockchip/platform/drmvop3326.h"
#include "rockchip/platform/drmvop3399.h"
#include "rockchip/platform/drmvop356x.h"
#include "rockchip/platform/drmvop3588.h"
#include "rockchip/platform/drmvop3528.h"
#include "rockchip/platform/drmvop3562.h"
#include "rockchip/platform/drmvop3576.h"

#include "rockchip/platform/drmhwc3326.h"
#include "rockchip/platform/drmhwc3399.h"
#include "rockchip/platform/drmhwc356x.h"
#include "rockchip/platform/drmhwc3588.h"
#include "rockchip/platform/drmhwc3528.h"
#include "rockchip/platform/drmhwc3562.h"
#include "rockchip/platform/drmhwc3576.h"

#include <log/log.h>

namespace android {

// Planner
std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *drm_device) {
  std::unique_ptr<Planner> planner(new Planner);
  switch(drm_device->getSocId()){
    case 0x3399:
      planner->AddStage<Vop3399>();
      break;
    case 0x3326:
      planner->AddStage<Vop3326>();
      break;
    case 0x3566:
    case 0x3568:
    // after ECO
    case 0x3566a:
    case 0x3568a:
      planner->AddStage<Vop356x>();
      break;
    case 0x3576:
      planner->AddStage<Vop3576>();
      break;
    case 0x3588:
      planner->AddStage<Vop3588>();
      break;
    case 0x3528:
      planner->AddStage<Vop3528>();
      break;
    case 0x3562:
      planner->AddStage<Vop3562>();
      break;
    default:
      HWC2_ALOGE("Cann't fina a suitable Planner Stage, soc_id=%x",drm_device->getSocId());
      break;
  }
  return planner;
}

std::tuple<int, std::vector<DrmCompositionPlane>> Planner::TryHwcPolicy(
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc,
    bool gles_policy) {
  std::vector<DrmCompositionPlane> composition;
  int ret = -1;
  // Go through the provisioning stages and provision planes
  for (auto &i : stages_) {
    if(i->SupportPlatform(crtc->get_soc_id())){
      ret = i->TryHwcPolicy(&composition, layers, plane_groups,  crtc, gles_policy);
      if (ret) {
        ALOGE("Failed provision stage with ret %d", ret);
        return std::make_tuple(ret, std::vector<DrmCompositionPlane>());
      }
    }
  }
  return std::make_tuple(ret, std::move(composition));
}

// HwcPlatform
std::unique_ptr<HwcPlatform> HwcPlatform::CreateInstance(DrmDevice *drm_device) {
  std::unique_ptr<HwcPlatform> hwcPlatform(new HwcPlatform);
  switch(drm_device->getSocId()){
    case 0x3399:
      hwcPlatform->AddStage<Hwc3399>();
      break;
    case 0x3326:
      hwcPlatform->AddStage<Hwc3326>();
      break;
    case 0x3566:
    // after ECO
    case 0x3566a:
    case 0x3568:
    // after ECO
    case 0x3568a:
      hwcPlatform->AddStage<Hwc356x>();
      break;
    case 0x3576:
      hwcPlatform->AddStage<Hwc3576>();
      break;
    case 0x3588:
      hwcPlatform->AddStage<Hwc3588>();
      break;
    case 0x3528:
      hwcPlatform->AddStage<Hwc3528>();
      break;
    case 0x3562:
      hwcPlatform->AddStage<Hwc3562>();
      break;
    default:
      HWC2_ALOGE("Cann't fina a suitable Planner Stage, soc_id=%x",drm_device->getSocId());
      break;
  }
  return hwcPlatform;
}

// TryAssignPlane, Assign hardware layer resources.
int HwcPlatform::TryAssignPlane(DrmDevice* drm){
  int ret = -1;
  if(stages_.size() == 0){
    HWC2_ALOGE("stages_ size=%zu TryAssignPlane fail!", stages_.size());
  }
  // Go through the provisioning stages and provision planes
  for (auto &i : stages_) {
    if(i->SupportPlatform(drm->getSocId())){
      ret = i->TryAssignPlane(drm);
      if (ret) {
        HWC2_ALOGE("Failed provision stage with ret %d", ret);
        return ret;
      }
    }
  }
  return ret;
}

}  // namespace android

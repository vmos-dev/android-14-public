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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-two"

#include "platform.h"
#include "rockchip/platform/drmhwc3399.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void Hwc3399::Init(){
}

bool Hwc3399::SupportPlatform(uint32_t soc_id){
  switch(soc_id){
    case 0x3399:
      return true;
    default:
      break;
  }
  return false;
}

int Hwc3399::assignPlaneByPossibleCrtcs(DrmDevice* drm){
  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  // First, assign active display plane_mask
  for (auto &conn : drm->connectors()) {
    if(conn->state() != DRM_MODE_CONNECTED)
      continue;
    int display_id = conn->display();
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        continue;
    }

    uint32_t crtc_mask = 1 << crtc->pipe();
    for(auto &plane_group : all_plane_group){
      uint64_t possible_crtcs = plane_group->possible_crtcs;
      if(crtc_mask & possible_crtcs){
        plane_group->set_current_crtc(crtc_mask, display_id);
      }
    }
  }
  for(auto &plane_group : all_plane_group){
    HWC2_ALOGI("name=%s cur_crtcs_mask=0x%x possible-display=%" PRIi64 ,
            plane_group->planes[0]->name(),plane_group->current_crtc_,plane_group->possible_display_);

  }
  return 0;
}

int Hwc3399::TryAssignPlane(DrmDevice* drm){
  int ret = -1;
  for (auto &conn : drm->connectors()) {
    if(conn->state() != DRM_MODE_CONNECTED)
      continue;
    int display_id = conn->display();
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
      ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
      continue;
    }
  }

  ret = assignPlaneByPossibleCrtcs(drm);

  return ret;
}
}


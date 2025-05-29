/*
 * Copyright (C) 2024 Rockchip Electronics Co.Ltd.
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
#include "rockchip/platform/drmhwc3576.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void Hwc3576::Init(){
}

bool Hwc3576::SupportPlatform(uint32_t soc_id){
  switch(soc_id){
    case 0x3576:
      return true;
    default:
      break;
  }
  return false;
}
enum DrmRK3576Port{
  DRM_RK3576_VP0 = 0,
  DRM_RK3576_VP1 = 1,
  DRM_RK3576_VP2 = 2,
};

enum DrmRK3576PortMask{
  DRM_RK3576_VP0_MASK = 1 << DRM_RK3576_VP0,
  DRM_RK3576_VP1_MASK = 1 << DRM_RK3576_VP1,
  DRM_RK3576_VP2_MASK = 1 << DRM_RK3576_VP2,
};

std::map<uint64_t, std::map<uint64_t, uint64_t>> gMapPlanePolicy = {
  { DRM_RK3576_VP0_MASK,
    {
      {DRM_RK3576_VP0, {PLANE_RK3576_ALL_CLUSTER_MASK|PLANE_RK3576_ALL_ESMART0_MASK|PLANE_RK3576_ALL_ESMART2_MASK}}
    }
  },
  { DRM_RK3576_VP1_MASK,
    {
      {DRM_RK3576_VP1, {PLANE_RK3576_ALL_CLUSTER_MASK|PLANE_RK3576_ALL_ESMART1_MASK|PLANE_RK3576_ALL_ESMART3_MASK}}
    }
  },
  { DRM_RK3576_VP2_MASK,
    {
      {DRM_RK3576_VP2, {PLANE_RK3576_ALL_ESMART_MASK}}
    }
  },
  { DRM_RK3576_VP0_MASK | DRM_RK3576_VP1_MASK,
    {
      {DRM_RK3576_VP0, {PLANE_RK3576_ALL_CLUSTER0_MASK|PLANE_RK3576_ALL_ESMART0_MASK|PLANE_RK3576_ALL_ESMART2_MASK}},
      {DRM_RK3576_VP1, {PLANE_RK3576_ALL_CLUSTER1_MASK|PLANE_RK3576_ALL_ESMART1_MASK|PLANE_RK3576_ALL_ESMART3_MASK}}
    }
  },
  {DRM_RK3576_VP0_MASK | DRM_RK3576_VP2_MASK,
    {
      {DRM_RK3576_VP0, PLANE_RK3576_ALL_CLUSTER_MASK|PLANE_RK3576_ALL_ESMART0_MASK|PLANE_RK3576_ALL_ESMART2_MASK},
      {DRM_RK3576_VP2, PLANE_RK3576_ALL_ESMART1_MASK|PLANE_RK3576_ALL_ESMART3_MASK}
    }
  },
  {DRM_RK3576_VP1_MASK | DRM_RK3576_VP2_MASK,
    {
      {DRM_RK3576_VP1, PLANE_RK3576_ALL_CLUSTER_MASK|PLANE_RK3576_ALL_ESMART1_MASK|PLANE_RK3576_ALL_ESMART3_MASK},
      {DRM_RK3576_VP2, PLANE_RK3576_ALL_ESMART0_MASK|PLANE_RK3576_ALL_ESMART2_MASK}
    }
  },
  {DRM_RK3576_VP0_MASK | DRM_RK3576_VP1_MASK | DRM_RK3576_VP2_MASK,
    {
      {DRM_RK3576_VP0, PLANE_RK3576_ALL_CLUSTER0_MASK|PLANE_RK3576_ALL_ESMART0_MASK},
      {DRM_RK3576_VP1, PLANE_RK3576_ALL_CLUSTER1_MASK|PLANE_RK3576_ALL_ESMART1_MASK},
      {DRM_RK3576_VP2, PLANE_RK3576_ALL_ESMART2_MASK|PLANE_RK3576_ALL_ESMART3_MASK}
    }
  },
};

int Hwc3576::assignPlaneByHWC(DrmDevice* drm, uint64_t connected_port_mask){

  if(connected_port_mask == 0){
      HWC2_ALOGW("connected_port_mask = 0, must disable all DrmPlane ");
      return 0;
  }

  // 根据已连接的 port_mask 选择图层分配策略
  auto map_policy = gMapPlanePolicy.find(connected_port_mask);
  if(map_policy == gMapPlanePolicy.end()){
      HWC2_ALOGW("can't find port_mask = 0x%" PRIx64 ", plaease check connected port.", connected_port_mask);
      return -1;
  }

  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  for (auto &conn : drm->connectors()) {
    int display_id = conn->display();
    if(conn->state() != DRM_MODE_CONNECTED){
      HWC2_ALOGE("display=%d conn state() is disconnect.", display_id);
      continue;
    }
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        HWC2_ALOGE("display=%d crtc is NULL.", display_id);
        continue;
    }

    uint32_t crtc_mask = 1 << crtc->pipe();
    uint64_t port_mask = 1 << crtc->get_port_id();

    if((connected_port_mask & port_mask) == 0){
        HWC2_ALOGW("display=%d connected_port_mask=0x%" PRIx64 " current_port_mask=0x%" PRIx64,
                    display_id, connected_port_mask, port_mask);
        continue;
    }

    uint64_t plane_mask = map_policy->second[crtc->get_port_id()];
    HWC2_ALOGI("display=%d crtc-id=%d port_mask=0x%" PRIx64" plane_mask=0x%" PRIx64 ,
             display_id, crtc->id(), port_mask, plane_mask);
    for(auto &plane_group : all_plane_group){
      uint64_t plane_group_win_type = plane_group->win_type;
      if((plane_mask & plane_group_win_type) == plane_group_win_type){
        plane_group->set_next_crtc(crtc_mask, display_id);
      }
    }
  }

  for(auto &plane_group : all_plane_group){
    HWC2_ALOGI("name=%s display=(%" PRIi64 " -> %" PRIi64 ") crtcs_mask=(0x%x -> 0x%x)",
      plane_group->planes[0]->name(), plane_group->possible_display_,
      plane_group->next_possible_display_,
      plane_group->current_crtc_, plane_group->next_crtc_);
  }
  return 0;
}

int Hwc3576::TryAssignPlane(DrmDevice* drm){
  int ret = -1;
  uint64_t connected_port_mask = 0;
  for (auto &conn : drm->connectors()) {
    int display_id = conn->display();
    if(conn->state() != DRM_MODE_CONNECTED)
      continue;
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
      HWC2_ALOGE("display %d crtc is NULL.", display_id);
      continue;
    }

    connected_port_mask |= (1 << crtc->get_port_id());
  }

  ret = assignPlaneByHWC(drm, connected_port_mask);
  return ret;
}
}


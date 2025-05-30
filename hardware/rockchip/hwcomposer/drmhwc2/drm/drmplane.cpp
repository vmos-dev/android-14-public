/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "hwc-drm-plane"

#include "drmplane.h"
#include "drmdevice.h"
#include "rockchip/utils/drmdebug.h"

#include <errno.h>
#include <stdint.h>
#include <cinttypes>

#include <log/log.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
namespace android {

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
struct plane_rotation_type_name plane_rotation_type_names[] = {
  { DRM_PLANE_ROTATION_0, "rotate-0" },
  { DRM_PLANE_ROTATION_90, "rotate-90" },
  { DRM_PLANE_ROTATION_270, "rotate-270" },
  { DRM_PLANE_ROTATION_X_MIRROR, "reflect-x" },
  { DRM_PLANE_ROTATION_Y_MIRROR, "reflect-y" },
  { DRM_PLANE_ROTATION_Unknown, "unknown" },
};

DrmPlane::DrmPlane(DrmDevice *drm, drmModePlanePtr p,int soc_id)
    : drm_(drm), id_(p->plane_id),
      possible_crtc_mask_(p->possible_crtcs),
      plane_(p),
      soc_id_(soc_id) {
}

int DrmPlane::Init() {
  DrmProperty p;

  int ret = drm_->GetPlaneProperty(*this, "type", &p);
  if (ret) {
    ALOGE("Could not get plane type property");
    return ret;
  }

  uint64_t type;
  std::tie(ret, type) = p.value();
  if (ret) {
    ALOGE("Failed to get plane type property value");
    return ret;
  }
  switch (type) {
    case DRM_PLANE_TYPE_OVERLAY:
    case DRM_PLANE_TYPE_PRIMARY:
    case DRM_PLANE_TYPE_CURSOR:
      type_ = (uint32_t)type;
      break;
    default:
      ALOGE("Invalid plane type %" PRIu64, type);
      return -EINVAL;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_ID", &crtc_property_);
  if (ret) {
    ALOGE("Could not get CRTC_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "FB_ID", &fb_property_);
  if (ret) {
    ALOGE("Could not get FB_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_X", &crtc_x_property_);
  if (ret) {
    ALOGE("Could not get CRTC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_Y", &crtc_y_property_);
  if (ret) {
    ALOGE("Could not get CRTC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_W", &crtc_w_property_);
  if (ret) {
    ALOGE("Could not get CRTC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_H", &crtc_h_property_);
  if (ret) {
    ALOGE("Could not get CRTC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_X", &src_x_property_);
  if (ret) {
    ALOGE("Could not get SRC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_Y", &src_y_property_);
  if (ret) {
    ALOGE("Could not get SRC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_W", &src_w_property_);
  if (ret) {
    ALOGE("Could not get SRC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_H", &src_h_property_);
  if (ret) {
    ALOGE("Could not get SRC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "EOTF", &eotf_property_);
  if (ret)
    ALOGI("Could not get eotf property");

  ret = drm_->GetPlaneProperty(*this, "ZPOS", &zpos_property_);
  if (ret){
    ALOGE("Could not get ZPOS property, try to get zpos property");
    ret = drm_->GetPlaneProperty(*this, "zpos", &zpos_property_);
    if (ret)
      ALOGE("Could not get zpos property");
  }

  ret = drm_->GetPlaneProperty(*this, "SHARE_FLAGS", &area_id_property_);
  if (ret)
    ALOGE("Could not get AREA_ID property");

  ret = drm_->GetPlaneProperty(*this, "SHARE_ID", &share_id_property_);
  if (ret)
    ALOGE("Could not get SHARE_ID property");

  ret = drm_->GetPlaneProperty(*this, "FEATURE", &feature_property_);
  if (ret)
    ALOGE("Could not get FEATURE property");
  std::tie(ret,b_scale_)   = feature_property_.value_bitmask("scale");
  std::tie(ret,b_alpha_)   = feature_property_.value_bitmask("alpha");
  std::tie(ret,b_hdr2sdr_)   = feature_property_.value_bitmask("hdr2sdr");
  std::tie(ret,b_sdr2hdr_)   = feature_property_.value_bitmask("sdr2hdr");
  std::tie(ret,b_afbdc_)   = feature_property_.value_bitmask("afbdc");

  if(isRK356x(soc_id_) || isRK3588(soc_id_) || isRK3528(soc_id_) || isRK3562(soc_id_) || isRK3576(soc_id_)){
    b_alpha_   = true;
    b_hdr2sdr_   = true;
    b_sdr2hdr_   = true;
  }

  if(isRK3399(soc_id_))
      b_alpha_   = true;

  //ALOGD("rk-debug scale=%d alpha=%d hdr2sdr=%d sdr2hdr=%d afbdc=%d", b_scale_,b_alpha_,b_hdr2sdr_,b_sdr2hdr_,b_afbdc_);

  support_format_list.clear();
  for (uint32_t j = 0; j < plane_->count_formats; j++) {
    support_format_list.insert(plane_->formats[j]);
  }

  ret = drm_->GetPlaneProperty(*this, "alpha", &alpha_property_);
  if (ret) {
    ret = drm_->GetPlaneProperty(*this, "GLOBAL_ALPHA", &alpha_property_vop1_kernel4_19_);
    if (ret)
      ALOGI("Could not get alpha property");
  }

  ret = drm_->GetPlaneProperty(*this, "pixel blend mode", &blend_mode_property_);
  if (ret) {
    ret = drm_->GetPlaneProperty(*this, "BLEND_MODE", &blend_mode_property_vop1_kernel4_19);
    if (ret)
      ALOGI("Could not get pixel blend mode property");
  }

  rotate_ = DRM_PLANE_ROTATION_0;
  ret = drm_->GetPlaneProperty(*this, "rotation", &rotation_property_);
  if (ret)
    ALOGE("Could not get rotation property");
  else{
    for(int i = 0; i < ARRAY_SIZE(plane_rotation_type_names); i++){
      bool find_name = false;
      int value = 0;
      std::tie(value,find_name) = rotation_property_.bitmask(plane_rotation_type_names[i].name);
      if(find_name){
        rotate_ |= value;
      }
    }
  }

  ret = drm_->GetPlaneProperty(*this, "NAME", &name_property_);
  if (ret){
    if(isRK3399(soc_id_))
      mark_type_by_name();
    else
      ALOGE("Could not get NAME property");
  }else{
    mark_type_by_name();
  }

  ret = drm_->GetPlaneProperty(*this, "INPUT_WIDTH", &input_w_property_);
  if (ret)
    ALOGE("Could not get INPUT_WIDTH property");
  else{
    uint64_t input_w_max=0;
    std::tie(ret,input_w_max) = input_w_property_.range_max();
    if(!ret)
      input_w_max_ = input_w_max;
    else
      ALOGE("Could not get INPUT_WIDTH range_max property");
  }
  ret = drm_->GetPlaneProperty(*this, "INPUT_HEIGHT", &input_h_property_);
  if (ret)
    ALOGE("Could not get INPUT_HEIGHT property");
  else{
    uint64_t input_h_max=0;
    std::tie(ret,input_h_max) = input_h_property_.range_max();
    if(!ret)
      input_h_max_ = input_h_max;
    else
      ALOGE("Could not get INPUT_HEIGHT range_max property");
  }

  ret = drm_->GetPlaneProperty(*this, "OUTPUT_WIDTH", &output_w_property_);
  if (ret)
    ALOGE("Could not get OUTPUT_WIDTH property");
  else{
    uint64_t output_w_max=0;
    std::tie(ret,output_w_max) = output_w_property_.range_max();
    if(!ret)
      output_w_max_ = output_w_max;
    else
      ALOGE("Could not get OUTPUT_WIDTH range_max property");
  }

  ret = drm_->GetPlaneProperty(*this, "OUTPUT_HEIGHT", &output_h_property_);
  if (ret)
    ALOGE("Could not get OUTPUT_HEIGHT property");
  else{
    uint64_t output_h_max=0;
    std::tie(ret,output_h_max) = output_h_property_.range_max();
    if(!ret)
      output_h_max_ = output_h_max;
    else
      ALOGE("Could not get OUTPUT_HEIGHT range_max property");
  }

  ret = drm_->GetPlaneProperty(*this, "SCALE_RATE", &scale_rate_property_);
  if (ret)
    ALOGE("Could not get SCALE_RATE property");
  else{
    uint64_t scale_rate=0;
    std::tie(ret,scale_rate) = scale_rate_property_.range_max();
    if(!ret)
      scale_min_ = 1/(scale_rate * 1.0);
    else
      ALOGE("Could not get SCALE_RATE range_min property");
    std::tie(ret,scale_rate) = scale_rate_property_.range_min();
    if(!ret)
      scale_max_ = scale_rate;
    else
      ALOGE("Could not get SCALE_RATE range_max property");

    if(isRK356x(soc_id_)){
      if(win_type_ & (DRM_PLANE_TYPE_SMART0_MASK | DRM_PLANE_TYPE_SMART1_MASK)){
        b_scale_ = false;
        scale_min_ = 1.0;
        scale_max_ = 1.0;
      }
    }
  }

  ret = drm_->GetPlaneProperty(*this, "ASYNC_COMMIT", &async_commit_property_);
  if (ret) {
    ALOGE("Could not get ASYNC_COMMIT property");
  }

  // Kernel 6.1 更名使用 COLOR_ENCODING 与 COLOR_RANGE 替代 COLOR_SPACE
  if(gIsDrmVerison6_1()){
    ret = drm_->GetPlaneProperty(*this, "COLOR_ENCODING", &kernel6_1_color_encoding_ );
    if (ret) {
      ALOGE("Could not get COLOR_ENCODING property");
    }

    ret = drm_->GetPlaneProperty(*this, "COLOR_RANGE", &kernel6_1_color_range_);
    if (ret) {
      ALOGE("Could not get COLOR_RANGE property");
    }
  }else{
    ret = drm_->GetPlaneProperty(*this, "COLOR_SPACE", &colorspace_property_);
    if (ret)
      ALOGI("Could not get colorspace property");

  }

  ret = drm_->GetPlaneProperty(*this, "DCI_DATA", &dci_data_);
  if (ret) {
    ALOGI("Could not get DCI_DATA property");
  }

  AddLocalPlaneInfo();

  return 0;
}

uint32_t DrmPlane::id() const {
  return id_;
}

bool DrmPlane::GetCrtcSupported(const DrmCrtc &crtc) const {
  return !!((1 << crtc.pipe()) & possible_crtc_mask_);
}

uint32_t DrmPlane::type() const {
  return type_;
}

const char* DrmPlane::name() const{
  return name_;
}

void DrmPlane::mark_type_by_name(){
  if(isRK3588(soc_id_)){
    struct plane_type_name_rk3588 {
      DrmPlaneTypeRK3588 type;
      const char *name;
    };

    struct plane_type_name_rk3588 plane_type_names_rk3588[] = {
      { PLANE_RK3588_CLUSTER0_WIN0, "Cluster0-win0" },
      { PLANE_RK3588_CLUSTER0_WIN1, "Cluster0-win1" },

      { PLANE_RK3588_CLUSTER1_WIN0, "Cluster1-win0" },
      { PLANE_RK3588_CLUSTER1_WIN1, "Cluster1-win1" },

      { PLANE_RK3588_CLUSTER2_WIN0, "Cluster2-win0" },
      { PLANE_RK3588_CLUSTER2_WIN1, "Cluster2-win1" },

      { PLANE_RK3588_CLUSTER3_WIN0, "Cluster3-win0" },
      { PLANE_RK3588_CLUSTER3_WIN1, "Cluster3-win1" },

      { PLANE_RK3588_ESMART0_WIN0, "Esmart0-win0" },
      { PLANE_RK3588_ESMART0_WIN1, "Esmart0-win1" },
      { PLANE_RK3588_ESMART0_WIN2, "Esmart0-win2" },
      { PLANE_RK3588_ESMART0_WIN3, "Esmart0-win3" },

      { PLANE_RK3588_ESMART1_WIN0, "Esmart1-win0" },
      { PLANE_RK3588_ESMART1_WIN1, "Esmart1-win1" },
      { PLANE_RK3588_ESMART1_WIN2, "Esmart1-win2" },
      { PLANE_RK3588_ESMART1_WIN3, "Esmart1-win3" },

      { PLANE_RK3588_ESMART2_WIN0, "Esmart2-win0" },
      { PLANE_RK3588_ESMART2_WIN1, "Esmart2-win1" },
      { PLANE_RK3588_ESMART2_WIN2, "Esmart2-win2" },
      { PLANE_RK3588_ESMART2_WIN3, "Esmart2-win3" },

      { PLANE_RK3588_ESMART3_WIN0, "Esmart3-win0" },
      { PLANE_RK3588_ESMART3_WIN1, "Esmart3-win1" },
      { PLANE_RK3588_ESMART3_WIN2, "Esmart3-win2" },
      { PLANE_RK3588_ESMART3_WIN3, "Esmart3-win3" },

      { PLANE_RK3588_Unknown, "unknown" },
    };

    for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk3588); i++){
      int ret;
      bool find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names_rk3588[i].name);
      if(find_name){
        win_type_ = plane_type_names_rk3588[i].type;
        name_ = plane_type_names_rk3588[i].name;
        break;
      }
    }
  }else if(isRK3576(soc_id_)){
    struct plane_type_name_rk3576 {
      DrmPlaneTypeRK3576 type;
      const char *name;
    };

    struct plane_type_name_rk3576 plane_type_names_rk3576[] = {
      { PLANE_RK3576_CLUSTER0_WIN0, "Cluster0-win0" },
      { PLANE_RK3576_CLUSTER0_WIN1, "Cluster0-win1" },

      { PLANE_RK3576_CLUSTER1_WIN0, "Cluster1-win0" },
      { PLANE_RK3576_CLUSTER1_WIN1, "Cluster1-win1" },

      { PLANE_RK3576_ESMART0_WIN0, "Esmart0-win0" },
      { PLANE_RK3576_ESMART0_WIN1, "Esmart0-win1" },
      { PLANE_RK3576_ESMART0_WIN2, "Esmart0-win2" },
      { PLANE_RK3576_ESMART0_WIN3, "Esmart0-win3" },

      { PLANE_RK3576_ESMART1_WIN0, "Esmart1-win0" },
      { PLANE_RK3576_ESMART1_WIN1, "Esmart1-win1" },
      { PLANE_RK3576_ESMART1_WIN2, "Esmart1-win2" },
      { PLANE_RK3576_ESMART1_WIN3, "Esmart1-win3" },

      { PLANE_RK3576_ESMART2_WIN0, "Esmart2-win0" },
      { PLANE_RK3576_ESMART2_WIN1, "Esmart2-win1" },
      { PLANE_RK3576_ESMART2_WIN2, "Esmart2-win2" },
      { PLANE_RK3576_ESMART2_WIN3, "Esmart2-win3" },

      { PLANE_RK3576_ESMART3_WIN0, "Esmart3-win0" },
      { PLANE_RK3576_ESMART3_WIN1, "Esmart3-win1" },
      { PLANE_RK3576_ESMART3_WIN2, "Esmart3-win2" },
      { PLANE_RK3576_ESMART3_WIN3, "Esmart3-win3" },

      { PLANE_RK3576_Unknown, "unknown" },
    };

    for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk3576); i++){
      int ret;
      bool find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names_rk3576[i].name);
      if(find_name){
        win_type_ = plane_type_names_rk3576[i].type;
        name_ = plane_type_names_rk3576[i].name;
        break;
      }
    }
  }else if(isRK356x(soc_id_)){
    struct plane_type_name_rk356x {
      DrmPlaneTypeRK356x type;
      const char *name;
    };
    struct plane_type_name_rk356x plane_type_names_rk356x[] = {
      { DRM_PLANE_TYPE_CLUSTER0_WIN0, "Cluster0-win0" },
      { DRM_PLANE_TYPE_CLUSTER0_WIN1, "Cluster0-win1" },
      { DRM_PLANE_TYPE_CLUSTER1_WIN0, "Cluster1-win0" },
      { DRM_PLANE_TYPE_CLUSTER1_WIN1, "Cluster1-win1" },

      { DRM_PLANE_TYPE_ESMART0_WIN0, "Esmart0-win0" },
      { DRM_PLANE_TYPE_ESMART0_WIN1, "Esmart0-win1" },
      { DRM_PLANE_TYPE_ESMART0_WIN2, "Esmart0-win2" },
      { DRM_PLANE_TYPE_ESMART0_WIN3, "Esmart0-win3" },

      { DRM_PLANE_TYPE_ESMART1_WIN0, "Esmart1-win0" },
      { DRM_PLANE_TYPE_ESMART1_WIN1, "Esmart1-win1" },
      { DRM_PLANE_TYPE_ESMART1_WIN2, "Esmart1-win2" },
      { DRM_PLANE_TYPE_ESMART1_WIN3, "Esmart1-win3" },

      { DRM_PLANE_TYPE_SMART0_WIN0, "Smart0-win0" },
      { DRM_PLANE_TYPE_SMART0_WIN1, "Smart0-win1" },
      { DRM_PLANE_TYPE_SMART0_WIN2, "Smart0-win2" },
      { DRM_PLANE_TYPE_SMART0_WIN3, "Smart0-win3" },

      { DRM_PLANE_TYPE_SMART1_WIN0, "Smart1-win0" },
      { DRM_PLANE_TYPE_SMART1_WIN1, "Smart1-win1" },
      { DRM_PLANE_TYPE_SMART1_WIN2, "Smart1-win2" },
      { DRM_PLANE_TYPE_SMART1_WIN3, "Smart1-win3" },

      { DRM_PLANE_TYPE_VOP2_Unknown, "unknown" },
    };

    for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk356x); i++){
      int ret;
      bool find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names_rk356x[i].name);
      if(find_name){
        win_type_ = plane_type_names_rk356x[i].type;
        name_ = plane_type_names_rk356x[i].name;
        break;
      }
    }

  }else if(isRK3399(soc_id_)){
    struct plane_type_name_rk3399 {
      DrmPlaneTypeRK3399 type;
      bool is_vop_big;          //     -Kernel5.10之前版本(4.19)需要使用图层属性推断图层
      uint32_t plane_type;      //  /
      bool isScale;             // /
      bool isArea0;             ///
      const char *name;
      bool matched = 0;
      bool match(bool _is_vop_big,uint32_t _plane_type,bool _isScale,int _id, int _share_id){
        bool _isArea0;

        if(matched)
          return false;

        if(_id == _share_id)
          _isArea0=true;
        else
          _isArea0=false;

        if(_is_vop_big==is_vop_big && _plane_type==plane_type && _isScale==isScale && _isArea0==isArea0){
          matched = true;
          return true;
        }else{
          return false;
        }
        
      }
    };
    //Kernel5.10之前版本(4.19)需要使用图层属性推断图层, Kernel5.10之后已拉齐支持使用NAME属性推断图层
    struct plane_type_name_rk3399 plane_type_names_rk3399[] = {
      { DRM_PLANE_TYPE_VOP0_WIN0  , true , DRM_PLANE_TYPE_PRIMARY,  true,   true,   "VOP0-win0-0" },
      { DRM_PLANE_TYPE_VOP0_WIN1  , true , DRM_PLANE_TYPE_OVERLAY,  true,   true,   "VOP0-win1-0" },
      { DRM_PLANE_TYPE_VOP0_WIN2_0, true , DRM_PLANE_TYPE_CURSOR,   false,  true,   "VOP0-win2-0" },
      { DRM_PLANE_TYPE_VOP0_WIN2_1, true , DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP0-win2-1" },
      { DRM_PLANE_TYPE_VOP0_WIN2_2, true , DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP0-win2-2" },
      { DRM_PLANE_TYPE_VOP0_WIN2_3, true , DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP0-win2-3" },
      { DRM_PLANE_TYPE_VOP0_WIN3_0, true , DRM_PLANE_TYPE_OVERLAY,  false,  true,   "VOP0-win3-0" },
      { DRM_PLANE_TYPE_VOP0_WIN3_1, true , DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP0-win3-1" },
      { DRM_PLANE_TYPE_VOP0_WIN3_2, true , DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP0-win3-2" },
      { DRM_PLANE_TYPE_VOP0_WIN3_3, true , DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP0-win3-3" },

      { DRM_PLANE_TYPE_VOP1_WIN0  , false, DRM_PLANE_TYPE_OVERLAY,  true,   true,   "VOP1-win0-0" },
      { DRM_PLANE_TYPE_VOP1_WIN2_0, false, DRM_PLANE_TYPE_PRIMARY,  false,  true,   "VOP1-win2-0" },
      { DRM_PLANE_TYPE_VOP1_WIN2_1, false, DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP1-win2-1" },
      { DRM_PLANE_TYPE_VOP1_WIN2_2, false, DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP1-win2-2" },
      { DRM_PLANE_TYPE_VOP1_WIN2_3, false, DRM_PLANE_TYPE_OVERLAY,  false,  false,  "VOP1-win2-3" },

      { DRM_PLANE_TYPE_VOP1_Unknown,0, 0,                       0,      0,      "unknown"     },
    };

    uint32_t crtc_mask_big = 0;

    for(auto &crtc:drm_->crtcs()){
      if(crtc->get_afbc() == true)
        crtc_mask_big = 1<<crtc->pipe();
    }

    if(name_property_.id()){
      for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk3399); i++){
        int ret;
        bool name_found = false;
        std::tie(ret,name_found) = name_property_.bitmask(plane_type_names_rk3399[i].name);
        if(name_found){
          win_type_ = plane_type_names_rk3399[i].type;
          name_ = plane_type_names_rk3399[i].name;
          return;
        }
      }
    }
    //Kernel5.10之前版本(4.19)无NAME属性，需要使用图层属性推断图层
    int _ret;
    int shareid;
    std::tie(_ret,shareid) = share_id_property_.value();

    for(int i=0;i<ARRAY_SIZE(plane_type_names_rk3399);i++){
      bool is_vop_big = possible_crtc_mask_ == crtc_mask_big;
      if(plane_type_names_rk3399[i].match(is_vop_big ,type_,b_scale_,id_,shareid)){
        win_type_=plane_type_names_rk3399[i].type;
        name_ = plane_type_names_rk3399[i].name;
        return;
      }
    }

    win_type_=DRM_PLANE_TYPE_VOP1_Unknown;
    name_ = "unknown";

  }else if(isRK3528(soc_id_)){
    struct plane_type_name_rk3528 {
      DrmPlaneTypeRK3528 type;
      const char *name;
    };

    struct plane_type_name_rk3528 plane_type_names_rk3528[] = {
      { PLANE_RK3528_CLUSTER0_WIN0, "Cluster0-win0" },
      { PLANE_RK3528_CLUSTER0_WIN1, "Cluster0-win1" },

      { PLANE_RK3528_ESMART0_WIN0, "Esmart0-win0" },
      { PLANE_RK3528_ESMART0_WIN1, "Esmart0-win1" },
      { PLANE_RK3528_ESMART0_WIN2, "Esmart0-win2" },
      { PLANE_RK3528_ESMART0_WIN3, "Esmart0-win3" },

      { PLANE_RK3528_ESMART1_WIN0, "Esmart1-win0" },
      { PLANE_RK3528_ESMART1_WIN1, "Esmart1-win1" },
      { PLANE_RK3528_ESMART1_WIN2, "Esmart1-win2" },
      { PLANE_RK3528_ESMART1_WIN3, "Esmart1-win3" },

      { PLANE_RK3528_ESMART2_WIN0, "Esmart2-win0" },
      { PLANE_RK3528_ESMART2_WIN1, "Esmart2-win1" },
      { PLANE_RK3528_ESMART2_WIN2, "Esmart2-win2" },
      { PLANE_RK3528_ESMART2_WIN3, "Esmart2-win3" },

      { PLANE_RK3528_ESMART3_WIN0, "Esmart3-win0" },
      { PLANE_RK3528_ESMART3_WIN1, "Esmart3-win1" },
      { PLANE_RK3528_ESMART3_WIN2, "Esmart3-win2" },
      { PLANE_RK3528_ESMART3_WIN3, "Esmart3-win3" },

      { PLANE_RK3528_Unknown, "unknown" },
    };
    for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk3528); i++){
      int ret;
      bool find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names_rk3528[i].name);
      if(find_name){
        win_type_ = plane_type_names_rk3528[i].type;
        name_ = plane_type_names_rk3528[i].name;
        break;
      }
    }
  }else if(isRK3326(soc_id_)){
    struct plane_type_name_rk3326 {
      DrmPlaneTypeRK3399 type;
      const char *name;
    };

    struct plane_type_name_rk3326 plane_type_names_rk3326[] = {
      { DRM_PLANE_TYPE_VOP0_WIN0  , "VOP0-win0-0" },
      { DRM_PLANE_TYPE_VOP0_WIN1  , "VOP0-win1-0" },
      { DRM_PLANE_TYPE_VOP0_WIN2_0, "VOP0-win2-0" },
      { DRM_PLANE_TYPE_VOP0_WIN2_1, "VOP0-win2-1" },
      { DRM_PLANE_TYPE_VOP0_WIN2_2, "VOP0-win2-2" },
      { DRM_PLANE_TYPE_VOP0_WIN2_3, "VOP0-win2-3" },
      { DRM_PLANE_TYPE_VOP1_WIN1, "VOP1-win1-0" },

      { DRM_PLANE_TYPE_VOP1_Unknown, "unknown" },
    };
    for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk3326); i++){
      int ret;
      bool find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names_rk3326[i].name);
      if(find_name){
        win_type_ = plane_type_names_rk3326[i].type;
        name_ = plane_type_names_rk3326[i].name;
        break;
      }
    }
  }else if(isRK3562(soc_id_)){
    struct plane_type_name_rk3562 {
      DrmPlaneTypeRK3562 type;
      const char *name;
    };

    struct plane_type_name_rk3562 plane_type_names_rk3562[] = {
      { PLANE_RK3562_ESMART0_WIN0, "Esmart0-win0" },
      { PLANE_RK3562_ESMART0_WIN1, "Esmart0-win1" },
      { PLANE_RK3562_ESMART0_WIN2, "Esmart0-win2" },
      { PLANE_RK3562_ESMART0_WIN3, "Esmart0-win3" },

      { PLANE_RK3562_ESMART1_WIN0, "Esmart1-win0" },
      { PLANE_RK3562_ESMART1_WIN1, "Esmart1-win1" },
      { PLANE_RK3562_ESMART1_WIN2, "Esmart1-win2" },
      { PLANE_RK3562_ESMART1_WIN3, "Esmart1-win3" },

      { PLANE_RK3562_ESMART2_WIN0, "Esmart2-win0" },
      { PLANE_RK3562_ESMART2_WIN1, "Esmart2-win1" },
      { PLANE_RK3562_ESMART2_WIN2, "Esmart2-win2" },
      { PLANE_RK3562_ESMART2_WIN3, "Esmart2-win3" },

      { PLANE_RK3562_ESMART3_WIN0, "Esmart3-win0" },
      { PLANE_RK3562_ESMART3_WIN1, "Esmart3-win1" },
      { PLANE_RK3562_ESMART3_WIN2, "Esmart3-win2" },
      { PLANE_RK3562_ESMART3_WIN3, "Esmart3-win3" },

      { PLANE_RK3562_Unknown, "unknown" },
    };
    for(int i = 0; i < ARRAY_SIZE(plane_type_names_rk3562); i++){
      int ret;
      bool find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names_rk3562[i].name);
      if(find_name){
        win_type_ = plane_type_names_rk3562[i].type;
        name_ = plane_type_names_rk3562[i].name;
        break;
      }
    }
  }else{
    HWC2_ALOGE("Can't find soc_id is %x",soc_id_);
  }

}

void DrmPlane::AddLocalPlaneInfo(){
  if(isRK3399(soc_id_) && gIsDrmVerison419()){
    if(win_type_&DRM_PLANE_TYPE_VOP0_MASK) {
      input_w_max_=4096;
      input_h_max_=8192;
      output_w_max_=4096;
      output_h_max_=8192;
    }else if(win_type_&DRM_PLANE_TYPE_VOP1_MASK) {
      input_w_max_=4096;
      input_h_max_=8192;
      output_w_max_=2560;
      output_h_max_=8192;
    }else{
      HWC2_ALOGE("name:%s,UNKNOW Layer",name_);
    }
    HWC2_ALOGD_IF_DEBUG("name:%s,fixing input and output size:%d,%d,%d,%d",\
                        name_,\
                        input_w_max_,input_h_max_,\
                        output_w_max_,output_h_max_);
    if(win_type_&(DRM_PLANE_TYPE_VOP1_WIN0|DRM_PLANE_TYPE_VOP0_WIN0|DRM_PLANE_TYPE_VOP0_WIN1)){
      scale_max_=8.0f;
      scale_min_=1.0f/8.0f;
      b_scale_=true;
    }else{
      scale_max_=1.0f;
      scale_min_=1.0f;
      b_scale_=false;
    }
    HWC2_ALOGD_IF_DEBUG("name:%s,fixing scale:%s(%f - %f)",name_,b_scale_?"true":"false",scale_min_,scale_max_);
    if(win_type_&DRM_PLANE_TYPE_VOP1_MASK){
      b_afbdc_ = false;
      HWC2_ALOGD_IF_DEBUG("VOP_LIT,not support afbc");
    }
  }
}

uint64_t DrmPlane::win_type() const{
  return win_type_;
}

const DrmProperty &DrmPlane::crtc_property() const {
  return crtc_property_;
}

const DrmProperty &DrmPlane::fb_property() const {
  return fb_property_;
}

const DrmProperty &DrmPlane::crtc_x_property() const {
  return crtc_x_property_;
}

const DrmProperty &DrmPlane::crtc_y_property() const {
  return crtc_y_property_;
}

const DrmProperty &DrmPlane::crtc_w_property() const {
  return crtc_w_property_;
}

const DrmProperty &DrmPlane::crtc_h_property() const {
  return crtc_h_property_;
}

const DrmProperty &DrmPlane::src_x_property() const {
  return src_x_property_;
}

const DrmProperty &DrmPlane::src_y_property() const {
  return src_y_property_;
}

const DrmProperty &DrmPlane::src_w_property() const {
  return src_w_property_;
}

const DrmProperty &DrmPlane::src_h_property() const {
  return src_h_property_;
}

const DrmProperty &DrmPlane::zpos_property() const {
  return zpos_property_;
}

const DrmProperty &DrmPlane::rotation_property() const {
  return rotation_property_;
}

const DrmProperty &DrmPlane::alpha_property() const {
  return alpha_property_;
}

const DrmProperty &DrmPlane::alpha_property_vop1_kernel4_19() const {
  return alpha_property_vop1_kernel4_19_;
}

const DrmProperty &DrmPlane::blend_property() const {
  return blend_mode_property_;
}

const DrmProperty &DrmPlane::blend_property_vop1_kernel4_19() const {
  return blend_mode_property_vop1_kernel4_19;
}

// RK support
const DrmProperty &DrmPlane::eotf_property() const {
  return eotf_property_;
}

  const DrmProperty &DrmPlane::colorspace_property() const {
    return colorspace_property_;
  }

  const DrmProperty &DrmPlane::area_id_property() const {
  return area_id_property_;
}

const DrmProperty &DrmPlane::share_id_property() const {
  return share_id_property_;
}

const DrmProperty &DrmPlane::feature_property() const {
  return feature_property_;
}

const DrmProperty &DrmPlane::name_property() const{
  return name_property_;
}

const DrmProperty &DrmPlane::input_w_property() const{
  return input_w_property_;
}

const DrmProperty &DrmPlane::input_h_property() const{
  return input_h_property_;
}

const DrmProperty &DrmPlane::output_w_property() const{
  return output_w_property_;
}

const DrmProperty &DrmPlane::output_h_property() const{
  return output_h_property_;
}

const DrmProperty &DrmPlane::scale_rate_property() const{
  return scale_rate_property_;
}

bool DrmPlane::get_scale(){
    return b_scale_;
}

bool DrmPlane::get_rotate(){
    return (rotate_ & DRM_PLANE_ROTATION_90)
        || (rotate_ & DRM_PLANE_ROTATION_270);
}

bool DrmPlane::get_hdr2sdr(){
    return b_hdr2sdr_;
}

bool DrmPlane::get_sdr2hdr(){
    return b_sdr2hdr_;
}

bool DrmPlane::get_afbc(){
    return b_afbdc_;
}

bool DrmPlane::get_yuv(){
    return b_yuv_;
}

int DrmPlane::get_input_w_max(){
    return input_w_max_;
}

int DrmPlane::get_input_h_max(){
    return input_h_max_;
}

int DrmPlane::get_output_w_max(){
    return output_w_max_;
}

int DrmPlane::get_output_h_max(){
    return output_h_max_;
}

void DrmPlane::set_yuv(bool b_yuv)
{
    b_yuv_ = b_yuv;
}

bool DrmPlane::is_use(){
    return b_use_;
}

void DrmPlane::set_use(bool b_use)
{
    b_use_ = b_use;
}

bool DrmPlane::is_reserved(){
  return bReserved_;
}

void DrmPlane::set_reserved(bool bReserved) {
    bReserved_ = bReserved;
}

bool DrmPlane::is_support_scale(float scale_rate){
  if(isRK3588(soc_id_)){
    if((win_type_ & PLANE_RK3588_ALL_CLUSTER_MASK) > 0)
      return (scale_rate >= scale_min_) && (scale_rate <= scale_max_);
    // RK3588 Esmart scale down 1080x1920 => 135x240 颜色出现错误，故认为缩小倍数8倍为不支持
    else if((win_type_ & PLANE_RK3588_ALL_ESMART_MASK) > 0)
      return (scale_rate > scale_min_) && (scale_rate <= scale_max_);
    else
      return scale_rate == 1.0;
  }else{
    if(get_scale()){
      return (scale_rate >= scale_min_) && (scale_rate <= scale_max_);
    }else{
      return scale_rate == 1.0;
    }
  }
}

bool DrmPlane::is_support_input(int input_w, int input_h){
  // RK platform VOP can't display src/dst w/h < 4 layer.
  return (input_w <= input_w_max_ && input_w >= 4) &&
         (input_h <= input_h_max_ && input_h >= 4);
}

bool DrmPlane::is_support_output(int output_w, int output_h){
  // RK platform VOP can't display src/dst w/h < 4 layer.
  return (output_w <= output_w_max_ && output_w >= 4) &&
         (output_h <= output_h_max_ && output_h >= 4);
}

bool DrmPlane::is_support_format(uint32_t format, bool afbcd, bool rfbcd){
  if(isRK3588(soc_id_)){
    if((win_type_ & PLANE_RK3588_ALL_CLUSTER_MASK) > 0){
      if(afbcd){
        return support_format_list.count(format);
      }else if(format == DRM_FORMAT_ABGR8888 ||
               format == DRM_FORMAT_BGR888 ||
               format == DRM_FORMAT_BGR565 ){
        return true;
      }else{
        return false;
      }
    }else if((win_type_ & PLANE_RK3588_ALL_ESMART_MASK) > 0 && !afbcd)
      return support_format_list.count(format);
    else
      return false;
  }else if(isRK3528(soc_id_)){
    if((win_type_ & PLANE_RK3528_ALL_CLUSTER_MASK) > 0){
      // RK3528 Cluster 支持非AFBC的 RGBA1010102格式
      if(format == HAL_PIXEL_FORMAT_RGBA_1010102 && !afbcd){
        return true;
      }
      return support_format_list.count(format);
    }else if((win_type_ & PLANE_RK3528_ALL_CLUSTER_MASK) == 0 && !afbcd)
      return support_format_list.count(format);
    else
      return false;
  }else if(isRK3562(soc_id_)){
    if(!afbcd){
      return support_format_list.count(format);
    }else{
      return false;
    }
  }else if(isRK356x(soc_id_)){
    if((win_type_ & DRM_PLANE_TYPE_ALL_CLUSTER_MASK) > 0 && afbcd)
      return support_format_list.count(format);
    else if((win_type_ & DRM_PLANE_TYPE_ALL_CLUSTER_MASK) == 0 && !afbcd)
      return support_format_list.count(format);
    else
      return false;
  }else if(isRK3399(soc_id_)){
      if(afbcd && get_afbc())
        return support_format_list.count(format);
      else if(!afbcd){
        if(win_type_&(DRM_PLANE_TYPE_VOP0_WIN0|DRM_PLANE_TYPE_VOP0_WIN1|DRM_PLANE_TYPE_VOP1_WIN0))
          if(format==DRM_FORMAT_P010)
            return true;
        return support_format_list.count(format);
      }
      else
        return false;
  }else if(isRK3326(soc_id_)){
      if(afbcd && !get_afbc())
        return false;
      return support_format_list.count(format);
  }else if(isRK3576(soc_id_)){
    if((win_type_ & PLANE_RK3576_ALL_CLUSTER_MASK) > 0){
      if(afbcd){
        if(format == DRM_FORMAT_ABGR8888  ||
           format == DRM_FORMAT_ARGB8888  ||
           format == DRM_FORMAT_XRGB8888  ||
           format == DRM_FORMAT_XBGR8888  ||
           format == DRM_FORMAT_BGR888    ||
           format == DRM_FORMAT_RGB888    ||
           format == DRM_FORMAT_XBGR2101010||
           format == DRM_FORMAT_XRGB2101010)
          return support_format_list.count(format);
        else
          return false;
      }else{
        return support_format_list.count(format);
      }
    }else if((win_type_ & PLANE_RK3576_ALL_ESMART_MASK) > 0 && !afbcd && !rfbcd)
      return support_format_list.count(format);
    else
      return false;
  }else{
      return false;
  }
}

int DrmPlane::get_transform(){
  return rotate_;
}

bool DrmPlane::is_support_transform(int transform){
  return (transform & rotate_) == transform;
}

// 8K
int DrmPlane::get_input_w_max_8k(){
    return 8096;
}

int DrmPlane::get_input_h_max_8k(){
    return 4320;
}

int DrmPlane::get_output_w_max_8k(){
    return 8096;
}

int DrmPlane::get_output_h_max_8k(){
    return 4320;
}

bool DrmPlane::is_support_scale_8k(float scale_rate){
  if(get_scale()){
    if((win_type_ & PLANE_RK3588_ALL_CLUSTER_MASK) > 0){
      return (scale_rate >= 0.9) && (scale_rate <= 1.1);
    }else{
      return (scale_rate >= scale_min_) && (scale_rate <= scale_max_);
    }
  }else{
    return scale_rate == 1.0;
  }
}

bool DrmPlane::is_support_input_8k(int input_w, int input_h){
  // RK platform VOP can't display src/dst w/h < 4 layer.
  return (input_w <= 8096 && input_w >= 4) && (input_h <= 4320 && input_h >= 4);
}

bool DrmPlane::is_support_output_8k(int output_w, int output_h){
  // RK platform VOP can't display src/dst w/h < 4 layer.
  return (output_w <= 8096 && output_w >= 4) && (output_h <= 4320 && output_h >= 4);
}

bool DrmPlane::is_support_transform_8k(int transform){
  return (transform & DRM_PLANE_ROTATION_0) == transform;
}

const DrmProperty &DrmPlane::async_commit_property() const{
  return async_commit_property_;
}

const DrmProperty &DrmPlane::kernel6_1_color_encoding() const{
  return kernel6_1_color_encoding_;

}
const DrmProperty &DrmPlane::kernel6_1_color_range() const{
  return kernel6_1_color_range_;

}

const DrmProperty &DrmPlane::dci_data() const{
  return dci_data_;

}

}  // namespace android

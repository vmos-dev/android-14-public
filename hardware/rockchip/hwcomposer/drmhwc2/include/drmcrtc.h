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

#ifndef ANDROID_DRM_CRTC_H_
#define ANDROID_DRM_CRTC_H_

#include "drmmode.h"
#include "drmproperty.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

class DrmDevice;

class DrmCrtc {
 public:
  DrmCrtc(DrmDevice *drm, drmModeCrtcPtr c, unsigned pipe);
  DrmCrtc(const DrmCrtc &) = delete;
  DrmCrtc &operator=(const DrmCrtc &) = delete;

  int Init();

  uint32_t id() const;
  unsigned pipe() const;

  int display() const;
  void set_display(int display);

  bool can_bind(int display) const;
  bool can_overscan() const;
  bool get_afbc() const;
  bool get_alpha_scale() const;
  bool get_hdr() const;
  bool get_next_hdr() const;

  const DrmMode &kernel_mode() const;

  uint32_t get_soc_id() const { return soc_id_; }
  uint32_t get_port_id() const { return port_id_; }
  uint32_t get_aclk() const { return aclk_; }
  uint64_t get_output_width() const { return output_width_; }
  uint64_t get_output_dlck() const { return output_dclk_; }
  uint64_t get_plane_mask() const { return plane_mask_; }
  uint64_t get_boot_plane_mask() const { return boot_plane_mask_; }
  void set_boot_plane_mask(uint64_t plane_mask) { boot_plane_mask_ = plane_mask; }
  void  set_hwc_plane_mask(uint64_t hwc_plane_mask) { hwc_plane_mask_ =  hwc_plane_mask; }
  uint64_t get_hwc_plane_mask() const { return hwc_plane_mask_; }


  const DrmProperty &active_property() const;
  const DrmProperty &mode_property() const;
  const DrmProperty &out_fence_ptr_property() const;
  const DrmProperty &left_margin_property() const;
  const DrmProperty &right_margin_property() const;
  const DrmProperty &top_margin_property() const;
  const DrmProperty &bottom_margin_property() const;
  const DrmProperty &alpha_scale_property() const;
  const DrmProperty &gamma_lut_property() const;
  const DrmProperty &gamma_lut_size_property() const;
  const DrmProperty &cubic_lut_property() const;
  const DrmProperty &cubic_lut_size_property() const;
  const DrmProperty &output_width_property() const;
  const DrmProperty &output_dclk_property() const;
  const DrmProperty &variable_refresh_rate() const;
  const DrmProperty &max_refresh_rate() const;
  const DrmProperty &min_refresh_rate() const;
  const DrmProperty &hdr_ext_data() const;
  const DrmProperty &acm_lut_data() const;
  const DrmProperty &post_csc_data() const;
  const DrmProperty &post_sharp_data() const;
  const DrmProperty &is_virtual() const;

  DrmDevice *getDrmDevice(){ return drm_; }
  bool is_vrr(){ return (max_refresh_rate_value_ > 0) && (min_refresh_rate_value_ > 0) && (max_refresh_rate_value_ > min_refresh_rate_value_); };
  bool is_virtual_crtc() { return is_virtual().id() > 0;};

 private:
  DrmDevice *drm_;
  int drm_version_;

  uint32_t id_;
  unsigned pipe_;
  int display_;

  DrmMode mode_;

  bool b_can_overscan_;
  bool b_can_alpha_scale_;
  bool b_can_afbc_; // RK3399
  bool b_can_hdr10_;
  bool b_can_next_hdr_;
  bool b_can_vivid_hdr_;

  DrmProperty active_property_;
  DrmProperty mode_property_;
  DrmProperty out_fence_ptr_property_;
  //RK support
  DrmProperty feature_property_;
  DrmProperty left_margin_property_;
  DrmProperty top_margin_property_;
  DrmProperty right_margin_property_;
  DrmProperty bottom_margin_property_;
  DrmProperty alpha_scale_property_;
  DrmProperty soc_type_property_;
  DrmProperty port_id_property_;
  DrmProperty aclk_property_;
  DrmProperty plane_mask_property_;
  DrmProperty gamma_lut_property_;
  DrmProperty gamma_lut_size_property_;
  DrmProperty cubic_lut_property_;
  DrmProperty cubic_lut_size_property_;
  DrmProperty output_width_property_;
  DrmProperty output_dclk_property_;
  // VRR 相关属性
  // variable_refresh_rate_ ： 为用户设置给驱动的请求值
  // max_refresh_rate_ ： VRR 支持的最大刷新率
  // min_refresh_rate_ ： VRR 支持的最小刷新率
  DrmProperty variable_refresh_rate_;
  DrmProperty max_refresh_rate_;
  DrmProperty min_refresh_rate_;
  DrmProperty hdr_ext_data_;
  DrmProperty acm_lut_data_;
  DrmProperty post_csc_data_;
  DrmProperty post_sharp_data_;
  DrmProperty is_virtual_;

  /* Vop2 */
  uint32_t soc_id_;
  uint32_t port_id_;
  uint32_t aclk_=0;
  uint64_t output_width_=0;
  uint64_t output_dclk_=0;
  // Plane_mask must limit crtc and plane binding relationship
  // If the Plane mask is specified, only the mask plane is allowed to be bound for crtc
  uint64_t boot_plane_mask_=0;
  uint64_t plane_mask_=0;
  // hwc_plane_mask_ will be set by DrmHwc
  uint64_t hwc_plane_mask_=0;

  uint64_t variable_refresh_rate_value_ = 0;
  uint64_t max_refresh_rate_value_ = 0;
  uint64_t min_refresh_rate_value_ = 0;
};
}  // namespace android

#endif  // ANDROID_DRM_CRTC_H_


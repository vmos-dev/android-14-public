/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef DRMHWC2_FOURCC_H
#define DRMHWC2_FOURCC_H
#include "drm_fourcc.h"


#ifndef DRM_FORMAT_NV15
/*
 * 2 plane YCbCr
 * index 0 = Y plane, [39:0] Y3:Y2:Y1:Y0 little endian
 * index 1 = Cr:Cb plane, [39:0] Cr1:Cb1:Cr0:Cb0 little endian
 */
#define DRM_FORMAT_NV15		fourcc_code('N', 'V', '1', '5') /* 2x2 subsampled Cr:Cb plane */
#endif

#ifndef DRM_FORMAT_NV12_10
#define DRM_FORMAT_NV12_10		fourcc_code('N', 'A', '1', '2') /* 2x2 subsampled Cr:Cb plane */
#endif

#ifndef DRM_FORMAT_NV20
#define DRM_FORMAT_NV20		fourcc_code('N', 'V', '2', '0') /* 2x1 subsampled Cr:Cb plane */
#endif

#ifndef DRM_FORMAT_NV30
#define DRM_FORMAT_NV30		fourcc_code('N', 'V', '3', '0') /* non-subsampled Cr:Cb plane */
#endif

#ifndef DRM_FORMAT_MOD_VENDOR_ROCKCHIP
#define DRM_FORMAT_MOD_VENDOR_ROCKCHIP 0x0b
#endif

#ifndef fourcc_mod_get_vendor
#define fourcc_mod_get_vendor(modifier) \
	(((modifier) >> 56) & 0xff)
#endif

#ifndef fourcc_mod_is_vendor
#define fourcc_mod_is_vendor(modifier, vendor) \
	(fourcc_mod_get_vendor(modifier) == DRM_FORMAT_MOD_VENDOR_## vendor)
#endif

#ifndef DRM_FORMAT_MOD_ROCKCHIP_TYPE_SHIFT
#define DRM_FORMAT_MOD_ROCKCHIP_TYPE_SHIFT	52
#endif

#ifndef DRM_FORMAT_MOD_ROCKCHIP_TYPE_MASK
#define DRM_FORMAT_MOD_ROCKCHIP_TYPE_MASK	0xf
#endif

#ifndef DRM_FORMAT_MOD_ROCKCHIP_TYPE_RFBC
#define DRM_FORMAT_MOD_ROCKCHIP_TYPE_RFBC	0x1
#endif

#ifndef ROCKCHIP_RFBC_BLOCK_SIZE_64x4
#define ROCKCHIP_RFBC_BLOCK_SIZE_64x4		(1ULL)
#endif

#ifndef DRM_FORMAT_MOD_ROCKCHIP_CODE
#define DRM_FORMAT_MOD_ROCKCHIP_CODE(__type, __val) \
	fourcc_mod_code(ROCKCHIP, ((__u64)(__type) << DRM_FORMAT_MOD_ROCKCHIP_TYPE_SHIFT) | \
			((__val) & 0x000fffffffffffffULL))
#endif

#ifndef DRM_FORMAT_MOD_ROCKCHIP_RFBC
/* Rockchip rfbc modifier format */
#define DRM_FORMAT_MOD_ROCKCHIP_RFBC(mode) \
	DRM_FORMAT_MOD_ROCKCHIP_CODE(DRM_FORMAT_MOD_ROCKCHIP_TYPE_RFBC, mode)
#endif

#ifndef IS_ROCKCHIP_RFBC_MOD
#define IS_ROCKCHIP_RFBC_MOD(val) \
	(fourcc_mod_is_vendor((val), ROCKCHIP) && \
	 (((val) >> DRM_FORMAT_MOD_ROCKCHIP_TYPE_SHIFT) & DRM_FORMAT_MOD_ROCKCHIP_TYPE_MASK) == DRM_FORMAT_MOD_ROCKCHIP_TYPE_RFBC)
#endif

#endif

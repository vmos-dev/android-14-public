/*
 * Copyright (C) 2022 Rockchip Electronics Co., Ltd.
 * Authors:
 *  Cerf Yu <cerf.yu@rock-chips.com>
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

#ifndef _RGA_UTILS_DRM_UTILS_H_
#define _RGA_UTILS_DRM_UTILS_H_

#include <stdint.h>
#include "drm/drm_fourcc.h"

#define is_drm_fourcc(format)                              \
    (((format >> 24) & 0xff) && ((format >> 16) & 0xff) && \
     ((format >> 8) & 0xff) && ((format)&0xff))

uint32_t lookup_rga_format_by_fourcc(uint32_t drm_fourcc);
uint32_t lookup_fourcc_by_rga_format(uint32_t drm_fourcc);

static inline const char* getDrmFourccString(uint32_t drm_fourcc)
{
    switch (drm_fourcc)
    {
        case DRM_FORMAT_C8: return "C8";
        case DRM_FORMAT_UYVY: return "UYVY";
        case DRM_FORMAT_VYUY: return "VYUY";
        case DRM_FORMAT_YUYV: return "YUYV";
        case DRM_FORMAT_YVYU: return "YVYU";
        case DRM_FORMAT_NV12: return "NV12";
        case DRM_FORMAT_NV21: return "NV21";
        case DRM_FORMAT_NV16: return "NV16";
        case DRM_FORMAT_NV61: return "NV61";
        case DRM_FORMAT_YUV420: return "YU12";
        case DRM_FORMAT_YVU420: return "YV12";
        /* RGB16 */
        case DRM_FORMAT_ARGB4444: return "AR12";
        case DRM_FORMAT_XRGB4444: return "XR12";
        case DRM_FORMAT_ABGR4444: return "AB12";
        case DRM_FORMAT_XBGR4444: return "XB12";
        case DRM_FORMAT_RGBA4444: return "RA12";
        case DRM_FORMAT_RGBX4444: return "RX12";
        case DRM_FORMAT_BGRA4444: return "BA12";
        case DRM_FORMAT_BGRX4444: return "BX12";
        case DRM_FORMAT_ARGB1555: return "AR15";
        case DRM_FORMAT_XRGB1555: return "XR15";
        case DRM_FORMAT_ABGR1555: return "AB15";
        case DRM_FORMAT_XBGR1555: return "XB15";
        case DRM_FORMAT_RGBA5551: return "RA15";
        case DRM_FORMAT_RGBX5551: return "RX15";
        case DRM_FORMAT_BGRA5551: return "BA15";
        case DRM_FORMAT_BGRX5551: return "BX15";
        case DRM_FORMAT_RGB565: return "RG16";
        case DRM_FORMAT_BGR565: return "BG16";
        /* RGB24 */
        case DRM_FORMAT_BGR888: return "BG24";
        case DRM_FORMAT_RGB888: return "RG24";
        /* RGB32 */
        case DRM_FORMAT_ARGB8888: return "AR24";
        case DRM_FORMAT_XRGB8888: return "XR24";
        case DRM_FORMAT_ABGR8888: return "AB24";
        case DRM_FORMAT_XBGR8888: return "XB24";
        case DRM_FORMAT_RGBA8888: return "RA24";
        case DRM_FORMAT_RGBX8888: return "RX24";
        case DRM_FORMAT_BGRA8888: return "BA24";
        case DRM_FORMAT_BGRX8888: return "BX24";
        case DRM_FORMAT_ARGB2101010: return "AR30";
        case DRM_FORMAT_XRGB2101010: return "XR30";
        case DRM_FORMAT_ABGR2101010: return "AB30";
        case DRM_FORMAT_XBGR2101010: return "XB30";
        case DRM_FORMAT_RGBA1010102: return "RA30";
        case DRM_FORMAT_RGBX1010102: return "RX30";
        case DRM_FORMAT_BGRA1010102: return "BA30";
        case DRM_FORMAT_BGRX1010102: return "BX30";
        default: return "Unknown";
    }
}

#endif /* #ifndef _RGA_UTILS_DRM_UTILS_H_ */

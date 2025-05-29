/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef HAL_ROCKCHIP_PSL_RKISP1_RGACROPSCALE_H_
#define HAL_ROCKCHIP_PSL_RKISP1_RGACROPSCALE_H_

namespace android {
namespace camera2 {

#if defined(TARGET_RK312X)
#define RGA_VER (1.0)
#define RGA_ACTIVE_W (2048)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (2048)
#define RGA_VIRTUAL_H (2048)
#else
#if defined(TARGET_RK3588)
#define RGA_VER (3.0)
#define RGA_ACTIVE_W (8128)
#define RGA_VIRTUAL_W (8128)
#define RGA_ACTIVE_H (8128)
#define RGA_VIRTUAL_H (8128)
#elif (defined(TARGET_RK3576)||defined(TARGET_RK3562))
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (8192)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (8192)
#else
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (4096)
#endif
#endif

class RgaCropScale {
 public:
    struct Params {
        /* use share fd if it's valid */
        int fd;
        /* if fd == -1, use virtual address */
        char *vir_addr;
        int offset_x;
        int offset_y;
        int width_stride;
        int height_stride;
        int width;
        int height;
        /* only support NV12,NV21 now */
        int fmt;
        /* just for src params */
        bool mirror;
        bool flip;
        int rotation;
    };

    static int CropScaleNV12Or21(struct Params* in, struct Params* out);
    static int WidthSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout);
    static int HeightSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout);
    static int WHSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout);

};

} /* namespace camera2 */
} /* namespace android */

#endif  // HAL_ROCKCHIP_PSL_RKISP1_RGACROPSCALE_H_

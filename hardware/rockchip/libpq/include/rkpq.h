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
#pragma once

#include "rkpq_api.h"
#ifdef USE_LIBPQ_HWPQ
#include "rkhwpq_api.h"
#endif
#include "PqType.h"

namespace android {

typedef enum _rkpq_mode
{
	PQ_NORMAL = 1,
	PQ_CACL_LUMA = 2,
	PQ_LF_RANGE = 4,
	PQ_IEP = 8,
} rkpq_mode;

class rkpq {
public:
    rkpq();
    ~rkpq();
    bool init(uint32_t src_width, uint32_t src_height, uint32_t* src_width_stride, uint32_t dst_width, uint32_t dst_height, 
        uint32_t alignment, uint32_t src_pix_format, uint32_t src_color_space, uint32_t dst_pix_format, uint32_t dst_color_space, uint32_t flag);
    bool dopq(uint32_t src_fd, uint32_t dst_fd, uint32_t mode);
#ifdef USE_LIBPQ_HWPQ
    bool dohwpq(HwPqImageInfo src, HwPqImageInfo dst, HwPqPreInfo preInfo);
#endif
    int setDstColorSpace(uint32_t plane_id, uint32_t color_space);
    int getResolutionInfo(uint32_t* width, uint32_t* height);
};

}

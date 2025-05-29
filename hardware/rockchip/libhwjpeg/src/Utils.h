/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#include "mpp_err.h"
#include "mpp_frame.h"
#include "mpp_packet.h"

#define ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

class CommonUtil {
public:
    /* global dump and store methods */
    static bool dumpDataToFile(void *data, size_t size, const char *fileName);
    static bool dumpDmaFdToFile(int fd, size_t size, const char *fileName);
    // allocate buffer memory inside, don't forget to free it.
    static bool storeFileData(const char *fileName, char **data, size_t *size);

    /* yuv image related operations */
    static bool cropImage(
            int src, int dst, int srcWidth, int srcHeight,
            int srcWstride, int srcHstride, int dstWidth, int dstHeight);

    /* other util methods */
    static bool isValidDmaFd(int fd);
    static bool setPerformanceMode(int on);
};

#endif //__UTILS_H__

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

#ifndef __RK_EXIF_WRAPPER_H__
#define __RK_EXIF_WRAPPER_H__

#include "ExifBuilder.h"
#include "RkExifInfo.h"

typedef struct {
    /* pointer to thumbnail image */
    void    *thumbData;
    int32_t  thumbSize;

    RkExifInfo *exifInfo;
} ExifInfo;

class RKExifWrapper {
public:
    /*
     * create jpeg exif app1 marker data
     *
     * param[in]  param   - ExifData
     * param[out] outBuf  - pointer to output buffer pointer.
     * param[out] size    - pointer to hold the buf number bytes
     */
    static bool getExifData(ExifInfo info, uint8_t **outBuf, int32_t *size);
};

#endif  // __RK_EXIF_WRAPPER_H__
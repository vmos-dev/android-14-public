/*
 * Copyright 2024 Rockchip Electronics Co. LTD.
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

#ifndef PREPROCESS_AEC_H_
#define PREPROCESS_AEC_H_

// AEC Normal
const effect_descriptor_t aec_normal_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xbb392ec0, 0x8d4d, 0x11e0, 0xa896, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "AEC Normal",
    "Rockchip Electronics Co. Ltd."
};

// AEC Delay
const effect_descriptor_t aec_delay_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x55fa4ede, 0x4adb, 0x45fe, 0x80b9, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "AEC Delay",
    "Rockchip Electronics Co. Ltd."
};

// AEC Array Reset
const effect_descriptor_t aec_array_reset_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x49cd17f2, 0x819f, 0x4acd, 0x9541, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "AEC Array Reset",
    "Rockchip Electronics Co. Ltd."
};

#endif /* PREPROCESS_AEC_H_ */

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

#ifndef PREPROCESS_BF_H_
#define PREPROCESS_BF_H_

// Fast Acoustic Echo Cancellation
const effect_descriptor_t bf_fast_aec_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xb5bfc822, 0x0b26, 0x4230, 0xadcb, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF Fast AEC",
    "Rockchip Electronics Co. Ltd."
};

// Wake Up
const effect_descriptor_t bf_wakeup_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x9ed2a703, 0x5336, 0x4a49, 0x9bbf, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF Wakeup",
    "Rockchip Electronics Co. Ltd."
};

// De-Reverberation
const effect_descriptor_t bf_dereverb_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xbd266323, 0xf95b, 0x4c4f, 0xab34, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF De-Reverberation",
    "Rockchip Electronics Co. Ltd."
};

// NLP
const effect_descriptor_t bf_nlp_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x53e4a6d1, 0x4e6b, 0x4421, 0xafbc, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF NLP",
    "Rockchip Electronics Co. Ltd."
};

// Acoustic Echo Suppression
const effect_descriptor_t bf_aes_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x3ad36ef7, 0x2f2a, 0x42cc, 0xafa3, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF AES",
    "Rockchip Electronics Co. Ltd."
};

// Automatic Gain Control
const effect_descriptor_t bf_agc_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xaa8130e0, 0x66fc, 0x11e0, 0xbad0, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF AGC",
    "Rockchip Electronics Co. Ltd."
};

// ANR
const effect_descriptor_t bf_anr_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x20b2732e, 0x28e9, 0x4b6b, 0x968a, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF ANR",
    "Rockchip Electronics Co. Ltd."
};

// GSC
const effect_descriptor_t bf_gsc_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x19d4a390, 0x7e23, 0x488b, 0x8568, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF GSC",
    "Rockchip Electronics Co. Ltd."
};

// GSC Method
const effect_descriptor_t bf_gsc_method_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x79dbccc7, 0xd690, 0x4ed3, 0xbe87, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF GSC Method",
    "Rockchip Electronics Co. Ltd."
};

// Fix
const effect_descriptor_t bf_fix_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x8e8fe0d9, 0xe335, 0x4a75, 0x847a, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF FIX",
    "Rockchip Electronics Co. Ltd."
};

// DTD
const effect_descriptor_t bf_dtd_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x97ef38d0, 0x9346, 0x48d3, 0xaeed, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF DTD",
    "Rockchip Electronics Co. Ltd."
};

// CNG
const effect_descriptor_t bf_cng_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xc0c12199, 0xfccb, 0x45a0, 0xa912, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF CNG",
    "Rockchip Electronics Co. Ltd."
};

// EQ
const effect_descriptor_t bf_eq_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x5f4428b2, 0x6827, 0x4a06, 0x842f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF EQ",
    "Rockchip Electronics Co. Ltd."
};

// CHN Select
const effect_descriptor_t bf_chn_select_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x874978b4, 0x1b82, 0x470f, 0xbe80, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF CHN Select",
    "Rockchip Electronics Co. Ltd."
};

// HOWLING
const effect_descriptor_t bf_howling_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x22075603, 0x4d9d, 0x40a6, 0xbfac, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF HOWLING",
    "Rockchip Electronics Co. Ltd."
};

// DOA
const effect_descriptor_t bf_doa_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x50404545, 0xa180, 0x40ad, 0xa8f4, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF DOA",
    "Rockchip Electronics Co. Ltd."
};

// WIND
const effect_descriptor_t bf_wind_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x2b8fea18, 0x547e, 0x4af7, 0x85c9, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF WIND",
    "Rockchip Electronics Co. Ltd."
};

// AINR
const effect_descriptor_t bf_ainr_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xa2913959, 0xf123, 0x43bc, 0xad9c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "BF AINR",
    "Rockchip Electronics Co. Ltd."
};

#endif /* PREPROCESS_BF_H_ */

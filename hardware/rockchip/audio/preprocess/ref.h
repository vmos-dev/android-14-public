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

#ifndef PREPROCESS_REF_H_
#define PREPROCESS_REF_H_

/*
 * NOTE: All the diagrams below are illustrated using stereo playback and
 * capture as examples.
 *
 * [L1][R1]... is a playback stream in a HAL, and then turns out to be a
 * loopback (reference) stream in the kernel (with DLP or Aloop) and a hardware.
 *
 * [L2][R2]... is a capture stream.
 */

/*
 * Use hardware loopback (HW reference).
 *
 * HAL                | Kernel                              | Hardware
 *                    |                                     |
 *                    |                       +----------+  |
 * +---------------+  |  [L1][R1]...          | CPU DAI0 |  |               +-----+
 * | Output Stream *------------------------->*          *----\------------>* SPK |
 * +---------------+  |                       +----------+  |  \            +-----+
 *                    |                                     |  |(Wired)
 *                    |                       +----------+  |  |
 * +---------------+  |  [L2][R2][L1][R1]...  | CPU DAI1 |  |  v [L2][R2]...+-----+
 * | Input Stream  *<=========================*          *<====+<-----------* MIC |
 * +---------------+  |                       +----------+  |               +-----+
 *                    |                                     |
 */
const effect_descriptor_t ref_hw_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xb95396ba, 0x60ed, 0x424d, 0x8ec7, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "HW Loopback",
    "Rockchip Electronics Co. Ltd."
};

/*
 * Use rockchip digital loopback (DLP) interleaved with source.
 *
 * HAL                | Kernel                              | Hardware
 *                    |                                     |
 *                    |                       +----------+  |
 * +---------------+  |  [L1][R1]...          | CPU DAI0 |  |
 * | Output Stream *--------------------------*---\      |  |
 * +---------------+  |                       |    \     |  |
 *                    |                       |    |(DLP)|  |
 * +---------------+  |  [L2][R2][L1][R1]...  |    v     |  |  [L2][R2]...  +-----+
 * | Input Stream  *<=========================*====+<----*------------------* MIC |
 * +---------------+  |                       |          |  |               +-----+
 *                    |                       +----------+  |
 */
const effect_descriptor_t ref_sw_dlp_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x7583028a, 0xe283, 0x4cb5, 0x9c0f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "Digital Loopback",
    "Rockchip Electronics Co. Ltd."
};

/*
 * Use generic loopback driver (PCM) to cache loopback stream.
 *
 * HAL                | Kernel                              | Hardware
 *                    |                                     |
 *                    |                       +----------+  |
 * +---------------+  |  [L1][R1]...          | CPU DAI0 |  |               +-----+
 * | Output Stream *------------------------->*          *----------------->* SPK |
 * +-------*-------+  |                       +----------+  |               +-----+
 *         |          |                                     |
 *          \         |                       +----------+  |
 *           \        |  [L1][R1]...          | Aloop    |  |
 *            \------------------------------>* Playback |  |
 *                    |                       | PCM      |  |
 *                    |                       +----------+  |
 */
const effect_descriptor_t ref_sw_alp_playback_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0xd3cb7c9b, 0x79a1, 0x4a84, 0xb26c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_POST_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "Aloop Playback",
    "Rockchip Electronics Co. Ltd."
};

/*
 * Use generic loopback driver (PCM) to obtain loopback stream.
 *
 * HAL                | Kernel                              | Hardware
 *                    |                                     |
 *                    |                       +----------+  |
 * +---------------+  |                       | CPU DAI1 |  |  [L2][R2]...  +-----+
 * | Input Stream  *<-------------------------*          *<-----------------* MIC |
 * +-------*-------+  |                       +----------+  |               +-----+
 *         ^          |                                     |
 *          \         |                       +----------+  |
 *           \        |                       | Aloop    |  |
 *            \-------------------------------* Capture  |  |
 *                    |                       | PCM      |  |
 *                    |                       +-*--------+  |
 *                    |                         ^           |
 *                    |                         |#cable0    |
 *                    |                         |[L1][R1]...|
 *                    |                       +----------+  |
 *                    |                       | Aloop    |  |
 *                    |                       | Playback |  |
 *                    |                       | PCM      |  |
 *                    |                       +----------+  |
 */
const effect_descriptor_t ref_sw_alp_capture_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x4f7f9f58, 0x122b, 0x4dc6, 0x831d, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_HW_ACC_TUNNEL |
     EFFECT_FLAG_NO_PROCESS),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "Aloop Capture",
    "Rockchip Electronics Co. Ltd."
};

#endif /* PREPROCESS_REF_H_ */

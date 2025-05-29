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

#ifndef PREPROCESS_H_
#define PREPROCESS_H_

#include <hardware/audio_effect.h>
#include <rkaudio_aec_bf.h>

#define BIT(nr) (1 << (nr))

// types of pre processing modules
enum preproc_id {
    PREPROC_AEC_NORMAL,
    PREPROC_AEC_DELAY,
    PREPROC_AEC_ARRAY_RESET,
    PREPROC_BF_FAST_AEC,
    PREPROC_BF_WAKEUP,
    PREPROC_BF_DEREVERB,
    PREPROC_BF_NLP,
    PREPROC_BF_AES,
    PREPROC_BF_AGC,
    PREPROC_BF_ANR,
    PREPROC_BF_GSC,
    PREPROC_BF_GSC_METHOD,
    PREPROC_BF_FIX,
    PREPROC_BF_DTD,
    PREPROC_BF_CNG,
    PREPROC_BF_EQ,
    PREPROC_BF_CHN_SELECT,
    PREPROC_BF_HOWLING,
    PREPROC_BF_DOA,
    PREPROC_BF_WIND,
    PREPROC_BF_AINR,
    PREPROC_RX_ANR,
    PREPROC_RX_HOWLING,
    PREPROC_REF_HW,
    PREPROC_REF_SW_DLP,
    PREPROC_REF_SW_ALP_PLAYBACK,
    PREPROC_REF_SW_ALP_CAPTURE,
    PREPROC_NUM_EFFECTS
};

#endif /* PREPROCESS_H_ */

/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef AUDIO_HW_HFP_H
#define AUDIO_HW_HFP_H

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <audio_utils/resampler.h>

#include "asound.h"
#include "asoundlib.h"

// HFP debug, dump call audio data
#define AUDIO_HFP_DUMP_DATA     1

#define AUDIO_HFP_BT_CARD   (1)
#define AUDIO_HFP_BT_DEV    (0)
#define AUDIO_HFP_SOC_CARD  (3)
#define AUDIO_HFP_SOC_DEV   (0)

#define AUDIO_SPEAKER_RATE_48K  (48000)

#define AUDIO_PARAMETER_HFP_ENABLE              "hfp_enable"
#define AUDIO_PARAMETER_HFP_VOLUME              "hfp_volume"
#define AUDIO_PARAMETER_HFP_SET_SAMPLING_RATE   "hfp_set_sampling_rate"

#define AUDIO_HFP_RATE_8K       (8000)
#define AUDIO_HFP_RATE_16K      (16000)

#define AUDIO_HFP_CHANNEL               (2)
#define AUDIO_HFP_PERIOD_DURATION       (20)
#define AUDIO_HFP_PERIOD_COUNT          (4)

void audio_hfp_run(struct audio_device *adev);
void audio_hfp_stop(struct audio_device *adev);
void audio_hfp_set_sample_rate(unsigned int rate);
void audio_hfp_set_volume(struct audio_device *adev, float value);

#endif //AUDIO_HW_HFP_H

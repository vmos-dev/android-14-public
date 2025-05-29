/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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

#define LOG_TAG "eqdrc-algo"

#include <errno.h>
#include <cutils/log.h>
#include <audio_utils/primitives.h>
#include <system/audio_effect.h>
#include <rkaudio_effect_eqdrc.h>
#include "algo.h"

int algo_init(struct algo *algo, struct profile *profile)
{
    algo->handle = AudioPost_Init((float *)profile, algo->frames);
    if (!algo->handle)
        return -EINVAL;

    pthread_mutex_init(&algo->lock, NULL);

    return 0;
}

int algo_process(struct algo *algo, audio_buffer_t *input, audio_buffer_t *output)
{
    if (algo->handle) {
        pthread_mutex_lock(&algo->lock);
        AudioPost_Process(algo->handle, input->raw, output->raw, algo->channels, algo->frames);
        pthread_mutex_unlock(&algo->lock);
    }

    return 0;
}

void algo_release(struct algo *algo)
{
    if (algo->handle) {
        AudioPost_Destroy(algo->handle);
        algo->handle = NULL;
    }
}

int algo_set_params(struct algo *algo, struct profile *profile)
{
    if (algo->handle) {
        pthread_mutex_lock(&algo->lock);
        AudioPost_SetPara(algo->handle, (float *)profile, algo->frames);
        pthread_mutex_unlock(&algo->lock);
    }

    return 0;
}

bool algo_initialized(struct algo *algo)
{
    return algo->handle ? true : false;
}

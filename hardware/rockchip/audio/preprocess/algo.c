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

#define LOG_TAG "preproc-algo"
// #define LOG_NDEBUG 0

#include <audio_utils/primitives.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <system/audio_effect.h>
#include <rkaudio_aec_bf.h>
#include "preprocess.h"
#include "algo.h"

#define ALGO_OUTPUT_CHANNELS 1

#define ALGO_DEBUG_INPUT_FMT "/data/misc/audioserver/preproc_input_%dhz_%dch.pcm"
#define ALGO_DEBUG_OUTPUT_FMT "/data/misc/audioserver/preproc_output_%dhz_%dch.pcm"

static inline size_t audio_bits_per_sample(audio_format_t format)
{
    size_t bytes = audio_bytes_per_sample(format);
    size_t bits = bytes * 8;

    return bits;
}

int algo_init(struct algo *algo, struct profile *profile, uint32_t enabled_mask)
{
    char value[PROPERTY_VALUE_MAX] = { 0 };
    char fname[NAME_MAX] = { 0 };
    int debug;
    size_t size;
    int ret;

    algo->handle = rkaudio_preprocess_init(profile->rate,
                                           audio_bits_per_sample(AUDIO_FORMAT_PCM_16_BIT),
                                           profile->channels_src, profile->channels_ref,
                                           &profile->param);
    if (!algo->handle) {
        ALOGD("%s: failed to initialize algorithm", __func__);
        ret = -EINVAL;
        goto err;
    }

    algo->channels = profile->channels;
    algo->channels_src = profile->channels_src;
    algo->channels_ref = profile->channels_ref;

    /* The algorithm outputs mono buffer. */
    size = profile->frames * audio_bytes_per_frame(ALGO_OUTPUT_CHANNELS, AUDIO_FORMAT_PCM_16_BIT);
    algo->buffer_processed = malloc(size);
    if (!algo->buffer_processed) {
        ALOGE("%s: failed to allocate mono buffer, expected size %zu", __func__, size);
        ret = -ENOMEM;
        goto err_alloc_processed_buf;
    }

    if (enabled_mask & BIT(PREPROC_REF_SW_ALP_CAPTURE)) {
        size = profile->frames * audio_bytes_per_frame(algo->channels, AUDIO_FORMAT_PCM_16_BIT);
        algo->buffer_merged = malloc(size);
        if (!algo->buffer_merged) {
            ALOGE("%s: failed to allocate mono buffer, expected size %zu", __func__, size);
            ret = -ENOMEM;
            goto err_alloc_merged_buf;
        }
    }

    rkaudio_param_printf(profile->channels_src, profile->channels_ref, &profile->param);

    property_get("vendor.audio.preproc.debug", value, "0");
    debug = atoi(value);
    if (debug) {
        snprintf(fname, sizeof(fname), ALGO_DEBUG_INPUT_FMT, profile->rate, profile->channels);
        algo->fd_input = fopen(fname, "wb+");

        snprintf(fname, sizeof(fname), ALGO_DEBUG_OUTPUT_FMT, profile->rate, ALGO_OUTPUT_CHANNELS);
        algo->fd_output = fopen(fname, "wb+");
    }

    return 0;

err_alloc_merged_buf:
    free(algo->buffer_processed);
    algo->buffer_processed = NULL;

err_alloc_processed_buf:
    rkaudio_preprocess_destory(algo->handle);
    algo->handle = NULL;

err:
    return ret;
}

int algo_process(struct algo *algo, audio_buffer_t *input, audio_buffer_t *output)
{
    void *src = input->s16;
    size_t frames = input->frameCount;
    size_t size;
    int targ_doa;
    int wakeup_status = 0;
    int samples = frames * algo->channels;
    int processed;

    if (algo->fd_input) {
        size = frames * audio_bytes_per_frame(algo->channels, AUDIO_FORMAT_PCM_16_BIT);
        fwrite(input->s16, size, 1, algo->fd_input);
        fflush(algo->fd_input);
    }

    /* NOTE: The preprocess outputs a buffer in 1 channel. */
    /* FIXME: The processed samples indicates 2 channel, actually it's 1 channel :( */
    processed = rkaudio_preprocess_short(algo->handle, (short *)src, algo->buffer_processed,
                                         samples, &wakeup_status);

    /* TODO: What is Doa? Should we invoke it here ? */
    targ_doa = rkaudio_Doa_invoke(algo->handle);

    if (algo->fd_output) {
        size = frames * audio_bytes_per_frame(ALGO_OUTPUT_CHANNELS, AUDIO_FORMAT_PCM_16_BIT);
        fwrite(algo->buffer_processed, size, 1, algo->fd_output);
        fflush(algo->fd_output);
    }

    upmix_to_stereo_i16_from_mono_i16(output->s16, algo->buffer_processed, frames);

    return 0;
}

int algo_process_merged(struct algo *algo, audio_buffer_t *input, audio_buffer_t *output,
                        audio_buffer_t *reference)
{
    audio_buffer_t tmp;
    int frames = input->frameCount;
    int index = 0;

    /*
     * Merge N channels buffer and M channels buffer into a (N+M) channels
     * interleaved buffer.
     */
    for (int s = 0; s < frames; ++s) {
        for (int c = 0; c < algo->channels_src; ++c) {
            /*
             * Always place the source samples at first as we set "ref_pos" to 1
             * in the default profile.
             */
            algo->buffer_merged[index++] = input->s16[s * algo->channels_src + c];
        }

        for (int c = 0; c < algo->channels_ref; ++c) {
            /* If the reference samples is empty, fill it with zero here. */
            algo->buffer_merged[index++] = reference->s16 ?
                                           reference->s16[s * algo->channels_ref + c] : 0;
        }
    }

    tmp.frameCount = input->frameCount;
    tmp.s16 = algo->buffer_merged;

    return algo_process(algo, &tmp, output);
}

void algo_release(struct algo *algo)
{
    if (algo) {
        if (algo->handle) {
            rkaudio_preprocess_destory(algo->handle);
            algo->handle = NULL;
        }

        if (algo->buffer_processed) {
            free(algo->buffer_processed);
            algo->buffer_processed = NULL;
        }

        if (algo->buffer_merged) {
            free(algo->buffer_merged);
            algo->buffer_merged = NULL;
        }

        if (algo->fd_input) {
            fclose(algo->fd_input);
            algo->fd_input = NULL;
        }

        if (algo->fd_output) {
            fclose(algo->fd_output);
            algo->fd_output = NULL;
        }

        algo->channels = 0;
        algo->channels_ref = 0;
        algo->channels_src = 0;
    }
}

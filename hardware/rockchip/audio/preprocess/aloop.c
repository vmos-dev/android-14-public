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

#define LOG_TAG "preproc-aloop"
// #define LOG_NDEBUG 0

#include <cutils/log.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <audio_utils/resampler.h>
#include <tinyalsa/asoundlib.h>
#include "aloop.h"

/*
 * Aloop is used to handle the reverse stream (software reference) from another
 * output thread.
 *
 * The following diagram shows the stream procedure:
 *
 * HAL                 | Kernel                              | Hardware
 *                     |                                     |
 *   <Far End In>      |                       +----------+  |
 * +------*---------+  |  [L1][R1]...          | CPU DAI0 |  |               +-----+
 * | Output Stream  *------------------------->*          *----------------->* SPK |
 * +------*---------+  |                       +----------+  |               +-----+
 *        | Step#1     |                                     |
 *        |[L1][R1]... |                                     |
 *        v            |                                     |
 * +------*--------+   |                       +----------+  |
 * | NOTE[1]       |   |                       | Aloop    |  |
 * |(optional pre  *-------------------------->* Playback |  |
 * |  resampler)   |   |                       | PCM      |  |
 * +---------------+   |                       +----------+  |
 *                     |                         |#cable0    |
 *                     |                         |           |
 *                     |                         v           |
 * +---------------+   |                       +-*--------+  |
 * | NOTE[2]       |   |  Step#2               | Aloop    |  |
 * |(optional post *<--------------------------* Capture  |  |
 * |  resampler)   |   |                       | PCM      |  |
 * +-----*-*-------+   |                       +----------+  |
 * Step#4| ^ Step#3    |                                     |
 * <AEC> | |[L2][R2]...|                                     |
 *       v |           |                       +----------+  |              <Near End>
 * +-----*-*--------+  |  Step#2'              | CPU DAI1 |  |  [L2][R2]...  +-----+
 * | Input Stream   *<-------------------------*          *<-----------------* MIC |
 * +-----*----------+  |                       +----------+  |               +-----+
 *   <Far End Out>     |                                     |
 *
 * NOTE[1]: A session is enabled with effect "Aloop Playback", and is bound to
 *          an output stream in an output thread.
 * <postprocess>
 *     <stream type="voice_call">
 *          <!-- ... -->
 *          <apply effect="preproc_ref_sw_alp_playback"/>
 *     </stream>
 * </postprocess>
 *
 * NOTE[2]: The other session is enabled with effect "Aloop Capture" and one of
 *          the "AEC", and is bound to an input stream in an input thread.
 *          The input thread will use AEC to process with data [L1][R1]... from
 *          <Far End In> and [L2][R2]... from <Near End>.
 * <preprocess>
 *     <stream type="voice_communication">
 *          <!-- <apply effect="preproc_aec_normal"/> -->
 *          <!-- <apply effect="preproc_aec_delay"/> -->
 *          <!-- <apply effect="preproc_aec_array_reset"/> -->
 *          <!-- ... -->
 *          <apply effect="preproc_ref_sw_alp_capture"/>
 *     </stream>
 * </preprocess>
 */
#define ALOOP_CARD                      0
#define ALOOP_CABLE0_CAPUTRE_DEVICE     0
#define ALOOP_CABLE0_PLAYBACK_DEVICE    1

/* NOTE: We only support for 2 channels loopback.*/
#define ALOOP_CHANNELS                  2

struct aloop {
    struct pcm *pcm;
    struct pcm_config config;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider provider;
    unsigned int card;
    unsigned int device;
    unsigned int flags;
    unsigned int rate;
    unsigned int channels;
    unsigned int frames;
    size_t frames_left;
    size_t frames_total;
    int16_t *buffer_raw;
    int16_t *buffer_processed;
};

static struct aloop aloop_capture = {
    .pcm = NULL,
    .resampler = NULL,
    .card = ALOOP_CARD,
    .device = ALOOP_CABLE0_CAPUTRE_DEVICE,
    .flags = PCM_IN | PCM_MONOTONIC,
    .frames = 0,
    .frames_left = 0,
    .frames_total = 0,
    .buffer_raw = NULL,
    .buffer_processed = NULL,
};

static struct aloop aloop_playback = {
    .pcm = NULL,
    .resampler = NULL,
    .card = ALOOP_CARD,
    .device = ALOOP_CABLE0_PLAYBACK_DEVICE,
    .flags = PCM_OUT | PCM_MONOTONIC,
    .frames = 0,
    .frames_left = 0,
    .frames_total = 0,
    .buffer_raw = NULL,
    .buffer_processed = NULL,
};

static size_t inline roundup_16(size_t frames)
{
    return ((frames + 15) / 16) * 16;
}

static int inline __aloop_next_buffer(int16_t **buffer, size_t *frames)
{
    struct aloop *aloop = &aloop_capture;
    size_t size;
    int samples_consumed;
    int ret;

    if (!aloop->frames_left) {
        size = pcm_frames_to_bytes(aloop->pcm, aloop->config.period_size);
        ret = pcm_read(aloop->pcm, aloop->buffer_raw, size);
        if (ret) {
            ALOGE("%s: failed to read, %s", __func__, pcm_get_error(aloop->pcm));
            *buffer = NULL;
            *frames = 0;
            return ret;
        }

        aloop->frames_left = aloop->config.period_size;
        aloop->frames_total = aloop->config.period_size;
    }

    samples_consumed = (aloop->frames_total - aloop->frames_left) * aloop->config.channels;
    *buffer = aloop->buffer_raw + samples_consumed;
    *frames = (*frames > aloop->frames_left) ? aloop->frames_left : *frames;

    return 0;
}

static int aloop_next_buffer(struct resampler_buffer_provider *provider,
                             struct resampler_buffer* buffer)
{
    return __aloop_next_buffer(&buffer->i16, &buffer->frame_count);
}

static void inline __aloop_consume_buffer(int frames)
{
    struct aloop *aloop = &aloop_capture;

    aloop->frames_left -= frames;
}

static void aloop_consume_buffer(struct resampler_buffer_provider *provider,
                                 struct resampler_buffer* buffer)
{
    __aloop_consume_buffer(buffer->frame_count);
}

static int aloop_init_pcm(struct aloop *aloop)
{
    aloop->pcm = pcm_open(aloop->card, aloop->device, aloop->flags, &aloop->config);
    if (!pcm_is_ready(aloop->pcm)) {
        aloop->pcm = NULL;
        return -errno;
    }

    return 0;
}

static void aloop_release_pcm(struct aloop *aloop)
{
    if (aloop->pcm) {
        pcm_close(aloop->pcm);
        aloop->pcm = NULL;
    }
}

static int aloop_init_resampler(struct aloop *aloop, bool playback)
{
    int ret;

    if (aloop->rate != aloop->config.rate) {
        if (playback) {
            /* pre-resampler */
            ret = create_resampler(aloop->rate,
                                   aloop->config.rate,
                                   ALOOP_CHANNELS,
                                   RESAMPLER_QUALITY_DEFAULT,
                                   NULL,
                                   &aloop->resampler);
        } else {
            /* post-resampler */
            aloop->provider.get_next_buffer = aloop_next_buffer;
            aloop->provider.release_buffer = aloop_consume_buffer;
            ret = create_resampler(aloop->config.rate,
                                   aloop->rate,
                                   ALOOP_CHANNELS,
                                   RESAMPLER_QUALITY_DEFAULT,
                                   &aloop->provider,
                                   &aloop->resampler);
        }

        if (ret) {
            ALOGD("%s: failed to create resampler", __func__);
            return ret;
        }
    }

    return 0;
}

static void aloop_release_resampler(struct aloop *aloop)
{
    if (aloop->resampler) {
        release_resampler(aloop->resampler);
        aloop->resampler = NULL;
    }
}

static int aloop_init_buffer(struct aloop *aloop, bool playback)
{
    size_t size;
    size_t frames_out;

    if (playback) {
        /* This buffer is used to cache the output of pre-resampler. */
        if (aloop->resampler) {
            frames_out = aloop->frames * aloop->config.rate / aloop->rate;
            frames_out = roundup_16(frames_out);
            size = pcm_frames_to_bytes(aloop->pcm, frames_out);
            aloop->buffer_processed = malloc(size);
            if (!aloop->buffer_processed) {
                ALOGD("%s: failed to alloc processed buffer for playback", __func__);
                return -ENOMEM;
            }
        }
    } else {
        /*
         * This buffer is used to cache the stream from aloop PCM, so the buffer
         * size is equal to the PCM frame size.
         */
        size = pcm_frames_to_bytes(aloop->pcm, aloop->config.period_size);
        aloop->buffer_raw = malloc(size);
        if (!aloop->buffer_raw) {
            ALOGD("%s: failed to alloc raw buffer for capture", __func__);
            return -ENOMEM;
        }

        if (aloop->resampler) {
            /* This buffer is used to cache the output of pre-resampler. */
            frames_out = aloop->config.period_size * aloop->rate / aloop->config.rate;
            frames_out = roundup_16(frames_out);
        } else if (aloop->frames != aloop->config.period_size) {
            /* This buffer is used to cache the output. */
            frames_out = aloop->frames;
        } else {
            frames_out = 0;
        }

        if (frames_out) {
            size = pcm_frames_to_bytes(aloop->pcm, frames_out);
            aloop->buffer_processed = malloc(size);
            if (!aloop->buffer_processed) {
                ALOGD("%s: failed to alloc processed buffer for capture", __func__);
                free(aloop->buffer_raw);
                aloop->buffer_raw = NULL;
                return -ENOMEM;
            }
        }
    }

    return 0;
}

static void aloop_release_buffer(struct aloop *aloop)
{
    if (aloop->buffer_raw) {
        free(aloop->buffer_raw);
        aloop->buffer_raw = NULL;
    }

    if (aloop->buffer_processed) {
        free(aloop->buffer_processed);
        aloop->buffer_processed = NULL;
    }
}

static int aloop_open(struct aloop *aloop, bool playback)
{
    struct aloop *another = playback? &aloop_capture : &aloop_playback;
    int ret;

    /* Already opened. */
    if (aloop->pcm)
        return -EBUSY;

    /* Update the PCM configurations. */
    if (another->pcm) {
        /*
         * Make sure the playback and capture configurations of aloop cable#0
         * are the same.
         */
        memcpy(&aloop->config, &another->config, sizeof(struct pcm_config));
    } else {
        aloop->config.rate = aloop->rate;
        aloop->config.channels = aloop->channels;
        aloop->config.period_size = aloop->frames;
        aloop->config.period_count = 4;
    }

    ret = aloop_init_pcm(aloop);
    if (ret)
        goto err_init_pcm;

    ret = aloop_init_resampler(aloop, playback);
    if (ret)
        goto err_init_resampler;

    ret = aloop_init_buffer(aloop, playback);
    if (ret)
        goto err_init_buffer;

    return 0;

err_init_buffer:
    aloop_release_resampler(aloop);

err_init_resampler:
    aloop_release_pcm(aloop);

err_init_pcm:
    return ret;
}

static void aloop_close(struct aloop *aloop)
{
    aloop_release_pcm(aloop);
    aloop_release_resampler(aloop);
    aloop_release_buffer(aloop);
}

int aloop_init_playback(int rate, int channels, int frames)
{
    struct aloop *aloop = &aloop_playback;
    int ret;

    if (aloop->pcm)
        return -EBUSY;

    if (channels != ALOOP_CHANNELS) {
        ALOGE("%s: expected %d channels, got %d channels", __func__, ALOOP_CHANNELS, channels);
        return -EINVAL;
    }

    aloop->rate = rate;
    aloop->channels = channels;
    aloop->frames = frames;

    ret = aloop_open(aloop, true);
    if (ret)
        return ret;

    return 0;
}

void aloop_release_playback(void)
{
    struct aloop *aloop = &aloop_playback;

    aloop_close(aloop);
}

int aloop_init_capture(int rate, int channels, int frames)
{
    struct aloop *aloop = &aloop_capture;
    int ret;

    if (aloop->pcm)
        return -EBUSY;

    if (channels != ALOOP_CHANNELS) {
        ALOGE("%s: expected %d channels, got %d channels", __func__, ALOOP_CHANNELS, channels);
        return -EINVAL;
    }

    aloop->rate = rate;
    aloop->channels = channels;
    aloop->frames = frames;

    ret = aloop_open(aloop, false);
    if (ret)
        return ret;

    return 0;
}

void aloop_release_capture(void)
{
    struct aloop *aloop = &aloop_capture;

    aloop_close(aloop);
}

int aloop_cache_buffer(audio_buffer_t *buffer)
{
    struct aloop *aloop = &aloop_playback;
    size_t frames_in;
    size_t frames_out;

    if (!aloop->pcm)
        return -ENODEV;

    if (aloop->resampler) {
        frames_in = buffer->frameCount;
        frames_out = aloop->config.period_size;
        aloop->resampler->resample_from_input(aloop->resampler,
                                              buffer->s16,
                                              &frames_in,
                                              aloop->buffer_processed,
                                              &frames_out);
        pcm_write(aloop->pcm, aloop->buffer_processed, pcm_frames_to_bytes(aloop->pcm, frames_out));
    } else {
        pcm_write(aloop->pcm, buffer->s16, pcm_frames_to_bytes(aloop->pcm, buffer->frameCount));
    }

    return 0;
}

static int aloop_obtain_buffer_concat(struct aloop *aloop, audio_buffer_t *buffer)
{
    int16_t *tmp;
    size_t frames_total = buffer->frameCount;
    size_t frames_requested = 0;
    size_t frames_remain;
    size_t bytes_requested;
    size_t bytes_remain;
    int ret;

    while (frames_requested < frames_total) {
        bytes_requested = pcm_frames_to_bytes(aloop->pcm, frames_requested);
        frames_remain = frames_total - frames_requested;

        if (aloop->resampler) {
            tmp = (int16_t *)((char *)aloop->buffer_processed + bytes_requested);
            ret = aloop->resampler->resample_from_provider(aloop->resampler,
                                                           tmp,
                                                           &frames_remain);
            if (ret)
                return ret;

        } else {
            ret = __aloop_next_buffer(&tmp, &frames_remain);
            if (ret)
                return ret;

            if (frames_remain) {
                bytes_remain = pcm_frames_to_bytes(aloop->pcm, frames_remain);
                memcpy((char *)aloop->buffer_processed + bytes_requested, tmp, bytes_remain);
            }

            __aloop_consume_buffer(frames_remain);
        }

        frames_requested += frames_remain;
    }

    buffer->frameCount = frames_requested;
    buffer->s16 = aloop->buffer_processed;

    return 0;
}

static int aloop_obtain_buffer_raw(struct aloop *aloop, audio_buffer_t *buffer)
{
    int ret;
    size_t size;

    size = pcm_frames_to_bytes(aloop->pcm, aloop->config.period_size);
    ret = pcm_read(aloop->pcm, aloop->buffer_raw, size);
    if (ret) {
        ALOGE("%s: failed to read, %s", __func__, pcm_get_error(aloop->pcm));
        return ret;
    }

    /* NOTE: To reduce memory copy. */
    buffer->frameCount = aloop->frames;
    buffer->s16 = aloop->buffer_raw;

    return 0;
}

int aloop_obtain_buffer(audio_buffer_t *buffer)
{
    struct aloop *aloop = &aloop_capture;
    int ret;

    if (!aloop->pcm) {
        ret = -ENODEV;
        goto err;
    }

    if (aloop->frames != buffer->frameCount) {
        ALOGE("%s: expected %u frames, got %zu frames", __func__, aloop->frames,
              buffer->frameCount);
        ret = -EINVAL;
        goto err;
    }

    if (aloop->frames != aloop->config.period_size)
        ret = aloop_obtain_buffer_concat(aloop, buffer);
    else
        ret = aloop_obtain_buffer_raw(aloop, buffer);

    if (ret)
        goto err;

    return 0;

err:
    buffer->frameCount = 0;
    buffer->s16 = NULL;
    return ret;
}

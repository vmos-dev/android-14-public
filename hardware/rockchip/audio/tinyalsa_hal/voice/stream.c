/*
 * Copyright (c) 2023, Rockchip Electronics Co. Ltd. All rights reserved.
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

/**
 * @file    stream.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#include "alsa_mixer.h"
#include "alsa_route.h"
#include "audio_hw.h"

#include "voice.h"

#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif

#define LOG_NDEBUG 0

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "voice-stream"

static int voice_open_pcm(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm **pcm = NULL;
    unsigned int flags;

    if (is_sink) {
        flags = PCM_OUT | PCM_MONOTONIC;
        device = session->stream->sink;
    } else {
        flags = PCM_IN;
        device = session->stream->source;
    }

    if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        if (is_sink)
            pcm = &session->pcm_hostless_tx;
        else
            pcm = &session->pcm_hostless_rx;
    } else if (device->type == VOICE_DAILINK_FE_BE) {
        if (is_sink)
            pcm = &session->pcm_tx;
        else
            pcm = &session->pcm_rx;
    }

    if (!pcm)
        return 0;

    *pcm = pcm_open(device->snd_card, device->snd_pcm, flags, device->config);

    if (!(*pcm) || !pcm_is_ready(*pcm)) {
        ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
              device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
        return -ENODEV;
    }

    return 0;
}

static int voice_open_pcm_full(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm **pcm;
    struct pcm **pcm_hostless;
    unsigned int flags;

    if (is_sink) {
        flags = PCM_OUT | PCM_MONOTONIC;
        pcm = &session->pcm_tx;
        pcm_hostless = &session->pcm_hostless_rx;
        device = session->stream->sink;
    } else {
        flags = PCM_IN;
        pcm = &session->pcm_rx;
        pcm_hostless = &session->pcm_hostless_tx;
        device = session->stream->source;
    }

    if (device->type == VOICE_DAILINK_FE_BE) {
        *pcm = pcm_open(device->snd_card, device->snd_pcm, flags, device->config);

        if (!(*pcm) || !pcm_is_ready(*pcm)) {
            ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
                  device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
            return -ENODEV;
        }
    } else if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        *pcm = pcm_open(device->backend->snd_card, device->backend->snd_pcm, flags,
                        device->backend->config);

        if (!(*pcm) || !pcm_is_ready(*pcm)) {
            ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
                  device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
            return -ENODEV;
        }

        /* NOTE: The direction is referened to the real backend, not SoC */
        flags = is_sink ? (PCM_IN) : (PCM_OUT | PCM_MONOTONIC);
        *pcm_hostless = pcm_open(device->snd_card, device->snd_pcm, flags, device->config);

        if (!(*pcm_hostless) || !pcm_is_ready(*pcm_hostless)) {
            ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
                  device->snd_card, device->snd_pcm, is_sink ? 'c' : 'p');
            return -ENODEV;
        }
    }

    return 0;
}

static void voice_stop_pcm(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm *pcm = NULL;

    if (is_sink)
        device = session->stream->sink;
    else
        device = session->stream->source;

    if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        if (is_sink)
            pcm = session->pcm_hostless_tx;
        else
            pcm = session->pcm_hostless_rx;
    } else if (device->type == VOICE_DAILINK_FE_BE) {
        if (is_sink)
            pcm = session->pcm_tx;
        else
            pcm = session->pcm_rx;
    }

    if (pcm)
        pcm_stop(pcm);
}

static void voice_stop_pcm_full(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm *pcm;
    struct pcm *pcm_hostless;

    if (is_sink) {
        pcm = session->pcm_tx;
        pcm_hostless = session->pcm_hostless_rx;
        device = session->stream->sink;
    } else {
        pcm = session->pcm_rx;
        pcm_hostless = session->pcm_hostless_tx;
        device = session->stream->source;
    }

    if (device->type == VOICE_DAILINK_FE_BE) {
        pcm_stop(pcm);
    } else if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        pcm_stop(pcm);
        pcm_stop(pcm_hostless);
    }
}

static void voice_dump_quirks(int quirks)
{
    ALOGD("  quirks =");

    if (quirks & VOICE_STREAM_CHANNEL_MONO_LEFT)
        ALOGD("    VOICE_STREAM_CHANNEL_MONO_LEFT");

    if (quirks & VOICE_STREAM_CHANNEL_MONO_RIGHT)
        ALOGD("    VOICE_STREAM_CHANNEL_MONO_RIGHT");
}

void voice_dump_stream(struct audio_device *adev)
{
    const struct voice_stream *stream;
    int i;

    for (i = 0; i < adev->voice.stream_num; ++i) {
        stream = &adev->voice.streams[i];

        ALOGD("stream%d:", i);

        if (stream->sink)
            ALOGD("  sink = %s", stream->sink->name);

        if (stream->source)
            ALOGD("  source = %s", stream->source->name);

        ALOGD("  route = 0x%x", stream->route);

        if (stream->quirks)
            voice_dump_quirks(stream->quirks);
    }
}

int voice_open_stream(struct voice_session *session)
{
    const struct voice_stream *stream = session->stream;
    int ret;

    if (stream->sink && stream->source) {
        ret = voice_open_pcm_full(session, true);
        if (ret)
            return ret;

        ret = voice_open_pcm_full(session, false);
        if (ret)
            return ret;
    } else {
        if (stream->sink) {
            ret = voice_open_pcm(session, true);
            if (ret)
                return ret;
        }

        if (stream->source) {
            ret = voice_open_pcm(session, false);
            if (ret)
                return ret;
        }
    }

    return 0;
}

void voice_stop_stream(struct voice_session *session)
{
    const struct voice_stream *stream = session->stream;

    if (stream->sink && stream->source) {
        voice_stop_pcm_full(session, true);

        voice_stop_pcm_full(session, false);
    } else {
        if (stream->sink)
            voice_stop_pcm(session, true);

        if (stream->source)
            voice_stop_pcm(session, false);
    }
}
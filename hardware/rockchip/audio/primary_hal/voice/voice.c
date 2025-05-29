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
 * @file    voice.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#include <cutils/list.h>

#include "alsa_mixer.h"
#include "alsa_route.h"
#include "audio_hw.h"

#include "voice.h"
#include "profile.h"

#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif

#define LOG_NDEBUG 0

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "voice"

static void voice_release_route(struct audio_device *adev)
{
    struct alsa_route *ar;
    int i;

    for (i = 0; i < MAX_VOICE_ROUTES; ++i) {
        ar = adev->voice.routes[i];
        if (!ar)
            continue;

        if (ar->applied <= MAX_ROUTE)
            route_reset(ar);

        route_close(ar);
        adev->voice.routes[i] = NULL;
    }
}

static int voice_apply_route(struct audio_device *adev)
{
    const struct voice_stream *stream;
    struct voice_session *session;
    const char *xml_path;
    const char *path;
    unsigned int mask = 0; /* NOTE: The maximun of sound card is not greater than 32 so far. */
    unsigned int route;
    int card;
    int i;
    int ret;

    /* Generate a bitmap for all sound cards */
    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];
        stream = session->stream;

        if (stream->sink) {
            card = stream->sink->snd_card;
            mask |= (0x01 << card);

            if (stream->sink->backend) {
                card = stream->sink->backend->snd_card;
                mask |= (0x01 << card);
            }
        }

        if (stream->source) {
            card = stream->source->snd_card;
            mask |= (0x01 << card);

            if (stream->source->backend) {
                card = stream->source->backend->snd_card;
                mask |= (0x01 << card);
            }
        }
    }

    i = 0;
    route = route_to_incall(adev->voice.route);
    /* Foreach set bit (sound card) and apply path */
    for (card = 0; card < (sizeof(mask) * 8); ++card) {
        if (!((0x01 << card) & mask))
            continue;

        if (i >= MAX_VOICE_ROUTES)
            break;

        /*
         * NOTE: The DAPM so called route configuration for some sound cards
         * is optional, and we should never issue an error.
         */
        ret = route_open(&adev->voice.routes[i], card);
        if (ret)
            continue;

        route_apply(adev->voice.routes[i], route);

        ++i;
    }

    return 0;
}

int voice_set_volume(struct audio_device *adev, float volume)
{
    struct voice_session *session;
    int i;

    adev->voice.volume = volume;

    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];
        session->volume = volume;
    }

    return 0;
}

int voice_set_mic_mute(struct audio_device *adev, bool mute)
{
    struct voice_session *session;
    const struct voice_stream *stream;
    int i;

    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];
        stream = session->stream;

        if (stream && (stream->quirks & VOICE_STREAM_OUTGOING)) {
            session->mute = !!mute;
        }
    }

    return 0;
}

bool voice_is_in_call(struct audio_device *adev)
{
    return adev->voice.in_call;
}

int voice_start_incall(struct audio_device *adev)
{
    int ret;

    if (!adev->voice.stream_num || !adev->voice.device_num)
        return 0;

    pthread_mutex_lock(&adev->voice.session_lock);

    ALOGD("%s", __FUNCTION__);

    ret = voice_create_session(adev);
    if (ret)
        goto cleanup_session;

    voice_dump_session(adev);

    ret = voice_apply_route(adev);
    if (ret)
        goto cleanup_route;

    ret = voice_prepare_session(adev);
    if (ret)
        goto cleanup_route;

    ret = voice_start_session(adev);
    if (ret)
        goto cleanup_route;

    adev->voice.in_call = true;

out:
    pthread_mutex_unlock(&adev->voice.session_lock);

    return 0;

cleanup_route:
    voice_release_route(adev);

cleanup_session:
    voice_release_session(adev);

    pthread_mutex_unlock(&adev->voice.session_lock);

    return ret;
}

int voice_stop_incall(struct audio_device *adev)
{
    if (!adev->voice.stream_num || !adev->voice.device_num)
        return 0;

    pthread_mutex_lock(&adev->voice.session_lock);

    ALOGD("%s", __FUNCTION__);

    voice_stop_session(adev);

    voice_release_route(adev);
    voice_release_session(adev);

    adev->voice.in_call = false;

    pthread_mutex_unlock(&adev->voice.session_lock);

    return 0;
}

int voice_init(struct audio_device *adev)
{
    int ret;

    adev->voice.route = AUDIO_DEVICE_NONE;
    adev->voice.stream_num = 0;
    adev->voice.device_num = 0;
    adev->voice.volume = 1.0f;

    ret = voice_load_profile(adev);
    if (ret)
        return ret;

    return 0;
}

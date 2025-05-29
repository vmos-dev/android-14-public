/*
 * Copyright (C) 2012 The Android Open Source Project
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
 * @file voice.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date 2023-04-12
 */

#ifndef VOICE_H
#define VOICE_H

#include <system/audio.h>
#include <audio_route/audio_route.h>

#include "device.h"
#include "stream.h"
#include "effect.h"
#include "session.h"

struct audio_device;

/**
 * struct voice - voice structure.
 * @devices: voice devices, loaded from session.xml.
 * @streams: the stream of a session, include sink and source, loaded from session.xml.
 * @routes: route configurations for voice streams.
 * @sessions: voice sessions.
 * @session_lock: the lock to guard voice sessions.
 * @device_num: the count of voice devices.
 * @stream_num: the count of streams.
 * @session_num: the count of created voice sessions.
 * @volume: the volume.
 * @route: route specified by the HAL.
 * @in_call: indicate whether the HAL is in call.
 */
struct voice {
    struct voice_device *devices;
    struct voice_stream *streams;
    struct alsa_route *routes[MAX_VOICE_ROUTES];
    struct voice_session sessions[MAX_VOICE_SESSIONS];
    pthread_mutex_t session_lock;
    int device_num;
    int stream_num;
    int session_num;
    audio_devices_t route;
    float volume;
    bool in_call;
};

bool voice_is_in_call(struct audio_device *adev);
bool voice_is_active(struct audio_device *adev);
int voice_set_volume(struct audio_device *adev, float volume);
int voice_set_mic_mute(struct audio_device *adev, bool mute);
int voice_start_incall(struct audio_device *adev);
int voice_stop_incall(struct audio_device *adev);
int voice_init(struct audio_device *adev);

#endif


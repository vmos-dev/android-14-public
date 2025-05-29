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
 * @file    stream.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#ifndef VOICE_STREAM_H
#define VOICE_STREAM_H

/* Stream is mono in the left channel */
#define VOICE_STREAM_CHANNEL_MONO_LEFT      (0x01 << 0)

/* Stream is mono in the right channel */
#define VOICE_STREAM_CHANNEL_MONO_RIGHT     (0x01 << 1)

/* Stream is outgoing */
#define VOICE_STREAM_OUTGOING               (0x01 << 2)

struct audio_device;
struct voice_session;

/**
 * struct voice_stream - stream configuration structure.
 * @source: the source of the stream so called input.
 * @sink: the sink of the stream so called output.
 * @route: the route which the stream is prefer, is used to match a route.
 * @quirks: a bitmap of stream quirks that require some special action.
 */
struct voice_stream {
    const struct voice_device *source;
    const struct voice_device *sink;
    audio_devices_t route;
    int quirks;
};

void voice_dump_stream(struct audio_device *adev);
int voice_open_stream(struct voice_session *session);
void voice_stop_stream(struct voice_session *session);

#endif

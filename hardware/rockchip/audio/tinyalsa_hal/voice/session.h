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
 * @file    session.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#ifndef VOICE_SESSION_H
#define VOICE_SESSION_H

#include <limits.h>

/*
 * NOTE: Each session has 1 stream, and each stream has 4 device (source or sink
 * with their backends), so the maximun of sound cards is 2 * 4. But note that
 * not all the sound cards are different.
 */
#define MAX_VOICE_SESSIONS  2
#define MAX_VOICE_ROUTES    ((MAX_VOICE_SESSIONS) * 4)

#define VOICE_STREAM_SINK   0
#define VOICE_STREAM_SOURCE 1
#define MAX_STREAM_DIR      2

struct audio_device;
struct voice_effect;

/**
 * struct voice_dump - voice dump structure.
 * @path: file path, found in /data/misc/audioserver/, named as
 *        session<x>_out.pcm or session<x>_in.pcm, x indicates it
 *        belongs to which session.
 * @file: file handle.
 * @size: the maximun of file size in MB.
 * @offset: written bytes of the file.
 */
struct voice_dump {
    char path[PATH_MAX];
    FILE *file;
    int size;
    int offset;
};

/**
 * struct voice_session - voice session structure.
 * @stream: the stream of this session.
 * @resampler: re-sampler for the sink and source of the stream.
 * @effect: effect to apply for the stream.
 * @pcm_tx: assigned PCM instance for the sink.
 * @pcm_rx: assigned PCM instance for the source.
 * @pcm_hostless_tx: assigned PCM instance for the hostless FE sink.
 * @pcm_hostless_rx: assigned PCM instance for the hostless FE source.
 * @out_buffer: output buffer of pcm_tx.
 * @in_buffer: input buffer of pcm_rx.
 * @out_buffer_size: the length of output buffer.
 * @in_buffer_size: the length of input buffer.
 * @dumps: dump configurations of sink and source.
 * @thread: thread to start voice streams off the HAL.
 * @thread_running: indicate whether the session thread is running.
 * @volume: volume of this session.
 * @mute: mute this session.
 */
struct voice_session {
    const struct voice_stream *stream;
    struct resampler_itfe *resampler;
    struct voice_effect effect;
    struct pcm *pcm_tx;
    struct pcm *pcm_rx;
    struct pcm *pcm_hostless_tx;
    struct pcm *pcm_hostless_rx;
    int16_t *out_buffer;
    int16_t *in_buffer;
    size_t out_buffer_size;
    size_t in_buffer_size;
    struct voice_dump dumps[MAX_STREAM_DIR];
    pthread_t thread;
    int thread_running;
    float volume;
    int mute;
};

void voice_dump_session(struct audio_device *adev);
int voice_create_session(struct audio_device *adev);
int voice_prepare_session(struct audio_device *adev);
int voice_start_session(struct audio_device *adev);
void voice_stop_session(struct audio_device *adev);
void voice_suspend_session(struct audio_device *adev);
void voice_release_session(struct audio_device *adev);

#endif

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

#ifndef PREPROCESS_ALOOP_H_
#define PREPROCESS_ALOOP_H_

#include <hardware/audio_effect.h>

int aloop_init_playback(int rate, int channels, int frames);
void aloop_release_playback(void);
int aloop_init_capture(int rate, int channels, int frames);
void aloop_release_capture(void);
int aloop_cache_buffer(audio_buffer_t *buffer);
int aloop_obtain_buffer(audio_buffer_t *buffer);

#endif /* PREPROCESS_ALOOP_H_ */

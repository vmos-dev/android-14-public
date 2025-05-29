/*
 * Copyright (C) 2023 The Android Open Source Project
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
 * @file    effect.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-11-09
 */

#include <errno.h>
#include <cutils/log.h>
#include "voice.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "voice-effect"

#define VOICE_PREPROCESS_RATE 8000

int voice_effect_init(struct voice_effect *effect, unsigned int rate,
                      unsigned int channels, unsigned int frames, unsigned int bits)
{
    // TODO
    return 0;
}

int voice_effect_process(struct voice_effect *effect, void *src, size_t frames)
{
    // TODO
    return 0;
}

void voice_effect_release(struct voice_effect *effect)
{
    // TODO
}

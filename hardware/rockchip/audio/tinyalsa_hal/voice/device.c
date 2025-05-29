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
 * @file    device.c
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

#define LOG_TAG "voice-device"

static void voice_dump_quirks(int quirks)
{
    ALOGD("  quirks =");

    if (quirks & VOICE_DEVICE_ROUTE_STATIC_UPDATE)
        ALOGD("    VOICE_DEVICE_ROUTE_STATIC_UPDATE");

    if (quirks & VOICE_DEVICE_CHANNEL_MONO_LEFT)
        ALOGD("    VOICE_DEVICE_CHANNEL_MONO_LEFT");

    if (quirks & VOICE_DEVICE_CHANNEL_MONO_RIGHT)
        ALOGD("    VOICE_DEVICE_CHANNEL_MONO_RIGHT");

    if (quirks & VOICE_DEVICE_CHANNEL_HAS_REFERENCE)
        ALOGD("    VOICE_DEVICE_CHANNEL_HAS_REFERENCE");
}

void voice_dump_device(struct audio_device *adev)
{
    const struct voice_device *device;
    int i;

    for (i = 0; i < adev->voice.device_num; ++i) {
        device = &adev->voice.devices[i];

        ALOGD("device%d:", i);
        ALOGD("  name = %s", device->name);
        ALOGD("  card = %d", device->snd_card);
        ALOGD("  pcm = %d", device->snd_pcm);
        ALOGD("  type = %d", device->type);

        if (device->backend)
            ALOGD("  backend = %s", device->backend->name);

        if (device->quirks)
            voice_dump_quirks(device->quirks);

        ALOGD("  channels = %d", device->config->channels);
        ALOGD("  rate = %d", device->config->rate);
        ALOGD("  period_size = %d", device->config->period_size);
        ALOGD("  period_count = %d", device->config->period_count);
        ALOGD("  format = %d", device->config->format);
    }
}

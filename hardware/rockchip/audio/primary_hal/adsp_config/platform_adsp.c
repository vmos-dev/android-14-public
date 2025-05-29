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

#define LOG_TAG "platform_adsp"
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0

#include "audio_hw.h"
#include "platform_adsp.h"
#include "rk3308_config.h"

int platform_adsp_set_volume(struct audio_device *adev, const char *addr, int32_t level)
{
    int count = 0, i;
    const char *name = "Playback Vol";
    struct mixer_ctl *ctl = NULL;
    struct audio_bus_ctls *bus_ctls = NULL;
    struct config_control *config_ctl = NULL;

    if (adev->mixer == NULL || addr == NULL)
        return -EINVAL;

    /* get audio bus control table */
    count = sizeof(bus_table) / sizeof(struct audio_bus_ctls);
    for (i = 0; i < count; i++) {
        if (strcmp(bus_table[i].bus_name, addr) == 0) {
            bus_ctls = &bus_table[i];
            break;
        }
    }

    if (bus_ctls == NULL)
        return -EINVAL;

    /* get audio config control */
    count = bus_ctls->controls_count;
    for (i = 0; i < count; i++) {
        if (strstr(bus_ctls->controls[i].ctl_name, name) != NULL) {
            config_ctl = &bus_ctls->controls[i];
            break;
        }
    }

    if (config_ctl == NULL)
        return -EINVAL;

    if (config_ctl->int_val[0] == level) {
        ALOGD("%s() don't need update kcontrol", __func__);
        return 0;
    }

    config_ctl->int_val[0] = level;
    config_ctl->int_val[1] = level;

    ctl = mixer_get_ctl_by_name(adev->mixer, config_ctl->ctl_name);
    if (ctl == NULL) {
        ALOGE("Invalid mixer control: %s\n", config_ctl->ctl_name);
        return ENOENT;
    }

    if (mixer_ctl_set_value(ctl, 0, config_ctl->int_val[0])) {
        ALOGE("Error: invalid value for index %d\n", 0);
        return -EINVAL;
    }

    return 0;
}

int platform_adsp_init(struct audio_device *adev)
{
    adev->mixer = mixer_open(MIXER_CARD);
    if (!adev->mixer) {
        ALOGE("Unable to open the mixer, aborting.");
        return -EINVAL;
    }

    return 0;
}

int platform_adsp_free(struct audio_device *adev)
{
    if (adev->mixer)
        mixer_close(adev->mixer);

    return 0;
}
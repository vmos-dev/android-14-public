/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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

#define LOG_TAG "eqdrc"
// #define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>
#include <utils/Timers.h>
#include <cutils/list.h>
#include <hardware/audio_effect.h>
#include "algo.h"
#include "profile.h"
#ifdef EQDRC_TUNER_ENABLED
#include "tuner.h"
#endif

#define EQDRC_PARAM_CURRENT_PRESET      0
#define EQDRC_PARAM_GET_NUM_OF_PRESETS  1
#define EQDRC_PARAM_GET_PRESET_NAME     2
#define EQDRC_PARAM_PROPERTIES          3

//------------------------------------------------------------------------------
// Effect presets
//------------------------------------------------------------------------------
/* TODO: add support other presets. */
// #define SUPPORT_EQDRC_OTHER_PRESETS
enum {
    EQDRC_PRESET_CUSTOM = 0,
#if SUPPORT_EQDRC_OTHER_PRESETS
    EQDRC_PRESET_STANDARD,
    EQDRC_PRESET_METTING,
    EQDRC_PRESET_CLASSICAL,
#endif
    EQDRC_NUM_PRESET,
};

static const char *eqdrc_preset_names[EQDRC_NUM_PRESET] = {
    "Custom",
#if SUPPORT_EQDRC_OTHER_PRESETS
    "Standard",
    "Metting",
    "Classical",
#endif
};

//------------------------------------------------------------------------------
// Effect context
//------------------------------------------------------------------------------
struct eqdrc {
    const struct effect_interface_s *itfe;
    struct listnode list;
    struct algo algo;
    struct profile profile;
    uint32_t current_preset; // current preset
    uint32_t num_presets; // number of supported presets.
    uint32_t state; // current state (enum preproc_session_state)
    uint32_t enabled;
    int id; // audio session ID
    int io; // handle of input stream this session is on
    uint32_t sampling_rate; // sampling rate at effect process interface
    uint32_t in_channel_count; // input channel count
    uint32_t out_channel_count; // output channel count
};

//------------------------------------------------------------------------------
// Effect descriptors
//------------------------------------------------------------------------------

const effect_descriptor_t eqdrc_descriptor = {
    { 0x34805d32, 0x2e6d, 0x4d1e, 0x9296, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // type
    { 0x79fe72b2, 0x4182, 0x44c1, 0xb2ea, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
    EFFECT_CONTROL_API_VERSION,
    (EFFECT_FLAG_TYPE_POST_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_NO_PROCESS |
     EFFECT_FLAG_HW_ACC_TUNNEL | EFFECT_FLAG_INSERT_EXCLUSIVE),
    0, // FIXME indicate CPU load
    0, // FIXME indicate memory usage
    "EQ DRC",
    "Rockchip Electronics Co. Ltd."
};

//------------------------------------------------------------------------------
// Effect global context
//------------------------------------------------------------------------------
struct listnode eqdrc_list;
pthread_mutex_t eqdrc_list_lock;
static int eqdrc_initialized = 0;

static void eqdrc_init()
{
    if (eqdrc_initialized)
        return;

    pthread_mutex_init(&eqdrc_list_lock, NULL);
    list_init(&eqdrc_list);
    eqdrc_initialized = 1;
}

//------------------------------------------------------------------------------
// Effect Command processing
//------------------------------------------------------------------------------

static int eqdrc_fx_init(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                         uint32_t *reply_size, void *reply_data)
{
    if (!reply_data || *reply_size != sizeof(int))
            return -EINVAL;

    *(int *)reply_data = 0;

    return 0;
}

static int eqdrc_fx_set_config(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                               uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;
    effect_config_t *config;
    uint32_t in_channels;
    uint32_t out_channels;

    if (!cmd_data || cmd_size != sizeof(effect_config_t) || !reply_data ||
        *reply_size != sizeof(int)) {
        ALOGV("%s EFFECT_CMD_SET_CONFIG: ERROR", __func__);
        return -EINVAL;
    }

    config = (effect_config_t *)cmd_data;

    in_channels = audio_channel_count_from_in_mask(config->inputCfg.channels);
    out_channels = audio_channel_count_from_out_mask(config->outputCfg.channels);

    if (config->inputCfg.samplingRate != config->outputCfg.samplingRate ||
        config->inputCfg.format != config->outputCfg.format ||
        config->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT) {
        *(int *)reply_data = -EINVAL;
        return -EINVAL;
    }

    if ((eqdrc->sampling_rate != config->inputCfg.samplingRate) ||
        (eqdrc->out_channel_count != out_channels)) {
        ALOGV("%s: sr %d cnl %08x", __func__, config->inputCfg.samplingRate,
              config->inputCfg.channels);

        if (profile_load(&eqdrc->profile, config->outputCfg.samplingRate, out_channels)) {
            ALOGD("failed to load profile");
            return -EINVAL;
        }

        profile_dump(&eqdrc->profile);

        eqdrc->sampling_rate = config->inputCfg.samplingRate;
        eqdrc->in_channel_count = in_channels;
        eqdrc->out_channel_count = out_channels;
    }

    *(int *)reply_data = 0;
    return 0;
}

static int eqdrc_fx_get_config(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                               uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;
    effect_config_t *config;

    if (!reply_data || *reply_size != sizeof(effect_config_t)) {
        ALOGV("%s: invalid lvm", __func__);
        return -EINVAL;
    }

    config = (effect_config_t *)reply_data;
    memset(config, 0, sizeof(effect_config_t));
    config->inputCfg.samplingRate = eqdrc->sampling_rate;
    config->outputCfg.samplingRate = eqdrc->sampling_rate;
    config->inputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    config->outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    config->inputCfg.channels = audio_channel_in_mask_from_count(eqdrc->in_channel_count);
    // "out" doesn't mean output device, so this is the correct API to convert channel count to mask
    config->outputCfg.channels = audio_channel_in_mask_from_count(eqdrc->out_channel_count);
    config->inputCfg.mask = (EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT);
    config->outputCfg.mask = (EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT);

    return 0;
}

static int eqdrc_fx_set_param(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                              uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;
    struct eqdrc *entry;
    struct listnode *node;
    effect_param_t *p;
    int voffset;
    uint32_t *param;
    uint32_t *psize;
    void *val;
    uint32_t *vsize;
    int ret = 0;

    if (!cmd_data || cmd_size < sizeof(effect_param_t) || !reply_data || !reply_size ||
        *reply_size != sizeof(int32_t)) {
        ALOGV("%s: invalid parameters", __func__);
        return -EINVAL;
    }

    p = (effect_param_t *)cmd_data;
    if (p->psize != sizeof(int32_t)) {
        ALOGV("%s: psize is not sizeof(int32_t)", __func__);
        return -EINVAL;
    }
    voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);

    param = (uint32_t *)p->data;
    psize = &p->psize;
    val = p->data + voffset;
    vsize = &p->vsize;

    switch (param[0]) {
    case EQDRC_PARAM_CURRENT_PRESET:
        if (*vsize < sizeof(eqdrc->current_preset)) {
            ALOGE("%s: invalid current preset param size %d, expected %zu", __func__, *vsize,
                  sizeof(eqdrc->current_preset));
            ret = -EINVAL;
            break;
        }
        memcpy(&eqdrc->current_preset, val, sizeof(eqdrc->current_preset));
        break;
    case EQDRC_PARAM_PROPERTIES:
        if (*vsize < (sizeof(eqdrc->current_preset) + sizeof(eqdrc->profile))) {
            ALOGE("%s: invalid properties param size %d, expected %zu", __func__, *vsize,
                  sizeof(eqdrc->current_preset) + sizeof(eqdrc->profile));
            ret = -EINVAL;
            break;
        }

        if (eqdrc->current_preset == EQDRC_PRESET_CUSTOM) {
            memcpy(&eqdrc->current_preset, &((uint32_t *)val)[0], sizeof(eqdrc->current_preset));
            memcpy(&eqdrc->profile, &((uint32_t *)val)[1], sizeof(eqdrc->profile));
            algo_set_params(&eqdrc->algo, &eqdrc->profile);
            profile_save(&eqdrc->profile, eqdrc->sampling_rate, eqdrc->out_channel_count);
            profile_dump(&eqdrc->profile);
        }

        pthread_mutex_lock(&eqdrc_list_lock);

        /* Sync the common profile to other sessions if they prefer the custom preset. */
        list_for_each(node, &eqdrc_list) {
            entry = node_to_item(node, struct eqdrc, list);
            if ((entry != eqdrc) && (entry->current_preset == EQDRC_PRESET_CUSTOM)) {
                memcpy(&entry->profile, &((uint32_t *)val)[1], sizeof(entry->profile));
                algo_set_params(&entry->algo, &entry->profile);
            }
        }

        pthread_mutex_unlock(&eqdrc_list_lock);

        break;
    default:
        break;
    }

    *(int *)reply_data = ret;
    return ret;
}

static int eqdrc_fx_get_param(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                              uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;
    effect_param_t *p;
    int voffset;
    uint32_t *param;
    uint32_t *psize;
    void *val;
    uint32_t *vsize;
    uint32_t preset;
    char *name;

    p = (effect_param_t *)cmd_data;
    if (!cmd_data || cmd_size < sizeof(effect_param_t) ||
        cmd_size < (sizeof(effect_param_t) + p->psize) || !reply_data ||
        !reply_size || *reply_size < (sizeof(effect_param_t) + p->psize)) {
        ALOGV("%s: invalid parameter", __func__);
        return -EINVAL;
    }

    memcpy(reply_data, cmd_data, sizeof(effect_param_t) + p->psize);

    p = (effect_param_t *)reply_data;
    voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);

    param = (uint32_t *)p->data;
    psize = &p->psize;
    val = p->data + voffset;
    vsize = &p->vsize;
    switch (param[0]) {
    case EQDRC_PARAM_CURRENT_PRESET:
        if (*vsize < sizeof(eqdrc->current_preset)) {
            ALOGE("%s: invalid current preset size %d", __func__, *vsize);
            *vsize = 0;
            return -EINVAL;
        }
        memcpy(val, &eqdrc->current_preset, sizeof(eqdrc->current_preset));
        break;
    case EQDRC_PARAM_GET_NUM_OF_PRESETS:
        if (*vsize < sizeof(eqdrc->num_presets)) {
            ALOGE("%s: invalid number of presets size %d", __func__, *vsize);
            *vsize = 0;
            return -EINVAL;
        }
        memcpy(val, &eqdrc->num_presets, sizeof(eqdrc->num_presets));
        break;
    case EQDRC_PARAM_GET_PRESET_NAME:
        if (*psize < (2 * sizeof(uint32_t))) {
            ALOGE("%s: invalid param size %d", __func__, *psize);
            return -EINVAL;
        }

        preset = param[1];
        if ((preset < 0) || (preset > eqdrc->num_presets)) {
            ALOGE("%s: invalid preset %d", __func__, preset);
            return -EINVAL;
        }

        if (*vsize < 1) {
            ALOGE("%s: invalid preset name size %d", __func__, *vsize);
            return -EINVAL;
        }

        name = (char *)val;
        name[*vsize - 1] = 0;
        strncpy(name, eqdrc_preset_names[preset], *vsize - 1);
        break;
    case EQDRC_PARAM_PROPERTIES:
        if (*vsize < (sizeof(eqdrc->current_preset) + sizeof(eqdrc->profile))) {
            ALOGE("%s: invalid properties param size %d, expected %zu", __func__, *vsize,
                  sizeof(eqdrc->current_preset) + sizeof(eqdrc->profile));
            *vsize = 0;
            return -EINVAL;
        }

        /* TODO: Get the profile of current preset not the common profile. */
        memcpy(&((uint32_t *)val)[0], &eqdrc->current_preset, sizeof(eqdrc->current_preset));
        memcpy(&((uint32_t *)val)[1], &eqdrc->profile, sizeof(eqdrc->profile));
        break;
    default:
        break;
    }

    p->status = 0;
    *reply_size = sizeof(effect_param_t) + voffset + p->vsize;
    return 0;
}

static int eqdrc_fx_enable(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                           uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;

    if (!reply_data || !reply_size || *reply_size != sizeof(int)) {
        ALOGV("%s EFFECT_CMD_ENABLE: ERROR", __func__);
        return -EINVAL;
    }

    eqdrc->enabled = 1;

    *(int *)reply_data = 0;
    return 0;
}

static int eqdrc_fx_disable(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                            uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;

    if (!reply_data || !reply_size || *reply_size != sizeof(int)) {
        ALOGV("%s EFFECT_CMD_DISABLE: ERROR", __func__);
        return -EINVAL;
    }

    eqdrc->enabled = 0;

    *(int *)reply_data = 0;
    return 0;
}

static int eqdrc_fx_set_device(effect_handle_t self, uint32_t cmd_size, void *cmd_data,
                               uint32_t *reply_size, void *reply_data)
{
    if (!cmd_data || cmd_size != sizeof(uint32_t)) {
        ALOGV("%s EFFECT_CMD_SET_DEVICE: ERROR", __func__);
        return -EINVAL;
    }

    /* TODO: *(uint32_t *)cmd_data is a device, should we ignore it? */

    return 0;
}

//------------------------------------------------------------------------------
// Effect Control Interface Implementation
//------------------------------------------------------------------------------

static int eqdrc_fx_process(effect_handle_t self, audio_buffer_t *input, audio_buffer_t *output)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;

    if (!eqdrc) {
        ALOGE("%s: invalid effect", __func__);
        return -EINVAL;
    }

    if (!input || !input->raw || !output || !output->raw) {
        ALOGW("%s: bad buffer", __func__);
        return -EINVAL;
    }

    if (input->frameCount != output->frameCount) {
        ALOGW("input frames %zu is not equal to output frames %zu", input->frameCount,
              output->frameCount);
        return -EINVAL;
    }

    if (eqdrc->enabled) {
        /*
         * NOTE: Framework may set configuration for several times, so we
         * initialize the algo here.
         */
        if (!algo_initialized(&eqdrc->algo)) {
            eqdrc->algo.frames = output->frameCount;
            eqdrc->algo.channels = eqdrc->profile.channels;

            if (algo_init(&eqdrc->algo, &eqdrc->profile))
                ALOGD("%s: failed to init algo", __func__);
        }

#ifdef EQDRC_TUNER_ENABLED
        if (tuner_synced()) {
            if (tuner_sync_profile(&eqdrc->profile, eqdrc->sampling_rate, eqdrc->out_channel_count)) {
                /* Make sure the PC tuner has pushed the profile to the right path. */
                ALOGD("%s: failed to sync profile from tuner", __func__);
            } else {
                algo_set_params(&eqdrc->algo, &eqdrc->profile);
                ALOGD("%s: reload profile from tuner", __func__);
            }
        }
#endif

        algo_process(&eqdrc->algo, input, output);
    }

    return 0;
}

static int eqdrc_fx_command(effect_handle_t self, uint32_t cmd_code, uint32_t cmd_size,
                            void *cmd_data, uint32_t *reply_size, void *reply_data)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;
    int ret = 0;

    if (!eqdrc)
        return -EINVAL;

    ALOGV("%s: command %d cmd_size %d", __func__, cmd_code, cmd_size);

    switch (cmd_code) {
    case EFFECT_CMD_INIT:
        ret = eqdrc_fx_init(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_SET_CONFIG:
        ret = eqdrc_fx_set_config(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_GET_CONFIG:
        ret = eqdrc_fx_get_config(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_SET_PARAM:
        ret = eqdrc_fx_set_param(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_GET_PARAM:
        ret = eqdrc_fx_get_param(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_ENABLE:
        ret = eqdrc_fx_enable(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_DISABLE:
        ret = eqdrc_fx_disable(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_INPUT_DEVICE:
        ret = eqdrc_fx_set_device(self, cmd_size, cmd_data, reply_size, reply_data);
        break;
    case EFFECT_CMD_SET_VOLUME:
    case EFFECT_CMD_SET_AUDIO_MODE:
    case EFFECT_CMD_SET_CONFIG_REVERSE:
    case EFFECT_CMD_GET_CONFIG_REVERSE:
    case EFFECT_CMD_RESET:
        break;
    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static int eqdrc_fx_get_descriptor(effect_handle_t self, effect_descriptor_t *descriptor)
{
    struct eqdrc *eqdrc = (struct eqdrc *)self;

    if (!eqdrc || !descriptor)
        return -EINVAL;

    memcpy(descriptor, &eqdrc_descriptor, sizeof(effect_descriptor_t));

    return 0;
}

// effect_handle_t interface implementation for effect
static const struct effect_interface_s effect_interface = {
    .process = eqdrc_fx_process,
    .command = eqdrc_fx_command,
    .get_descriptor = eqdrc_fx_get_descriptor,
    .process_reverse = NULL,
};

//------------------------------------------------------------------------------
// Effect Library Interface Implementation
//------------------------------------------------------------------------------

static int32_t eqdrc_create_effect(const effect_uuid_t *uuid, int32_t session_id, int32_t io_id,
                                   effect_handle_t *interface)
{
    struct eqdrc *eqdrc;

    ALOGV("%s: uuid: %08x session %d IO: %d", __func__, uuid->timeLow, session_id, io_id);

    if (memcmp(&eqdrc_descriptor.uuid, uuid, sizeof(effect_uuid_t))) {
        ALOGV("%s: not found", __func__);
        return -EINVAL;
    }

    eqdrc = calloc(1, sizeof(*eqdrc));
    if (!eqdrc) {
        ALOGE("%s: failed to allocate", __func__);
        return -ENOMEM;
    }

    eqdrc->itfe = &effect_interface;
    eqdrc->id = session_id;
    eqdrc->io = io_id;
    eqdrc->num_presets = EQDRC_NUM_PRESET;
    eqdrc->current_preset = 0;

    *interface = (effect_handle_t)&eqdrc->itfe;

    pthread_mutex_lock(&eqdrc_list_lock);
    list_add_tail(&eqdrc_list, &eqdrc->list);
    pthread_mutex_unlock(&eqdrc_list_lock);

    return 0;
}

static int32_t eqdrc_release_effect(effect_handle_t interface)
{
    struct eqdrc *eqdrc = (struct eqdrc *)interface;;

    ALOGV("%s: %p", __func__, interface);

    if (!interface)
        return -EINVAL;

    algo_release(&eqdrc->algo);
    profile_release(&eqdrc->profile);
    free(eqdrc);

    pthread_mutex_lock(&eqdrc_list_lock);
    list_remove(&eqdrc->list);
    pthread_mutex_unlock(&eqdrc_list_lock);

    return 0;
}

static int32_t eqdrc_get_descriptor(const effect_uuid_t *uuid, effect_descriptor_t *descriptor)
{
    if (!descriptor || !uuid)
        return -EINVAL;

    if (memcmp(&eqdrc_descriptor.uuid, uuid, sizeof(effect_uuid_t))) {
        ALOGV("%s: not found", __func__);
        return -EINVAL;
    }

    ALOGV("%s: got fx", __func__);

    memcpy(descriptor, &eqdrc_descriptor, sizeof(effect_descriptor_t));

#ifdef EQDRC_TUNER_ENABLED
    if (!tuner_initialized())
        if (tuner_init())
            ALOGE("failed to initialize tuner");
#endif

    eqdrc_init();

    return 0;
}

// This is the only symbol that needs to be exported
__attribute__((visibility("default"))) audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    .tag = AUDIO_EFFECT_LIBRARY_TAG,
    .version = EFFECT_LIBRARY_API_VERSION,
    .name = "Audio EQDRC Library",
    .implementor = "Rockchip Electronics Co. Ltd.",
    .create_effect = eqdrc_create_effect,
    .release_effect = eqdrc_release_effect,
    .get_descriptor = eqdrc_get_descriptor,
};

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

#define LOG_TAG "preproc"
// #define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>
#include <utils/Timers.h>
#include <audio_utils/primitives.h>
#include "preprocess.h"
#include "aec.h"
#include "bf.h"
#include "rx.h"
#include "algo.h"
#include "aloop.h"
#include "ref.h"
#include "profile.h"

//------------------------------------------------------------------------------
// local definitions
//------------------------------------------------------------------------------

// maximum number of sessions
#define PREPROC_NUM_SESSIONS 8

// Session state
enum preproc_session_state {
    PREPROC_SESSION_STATE_INIT, // initialized
    PREPROC_SESSION_STATE_CONFIG, // configuration received
    PREPROC_SESSION_STATE_PROCESS,
};

// Effect/Preprocessor state
enum preproc_effect_state {
    PREPROC_EFFECT_STATE_INIT, // initialized
    PREPROC_EFFECT_STATE_CREATED, // engine created
    PREPROC_EFFECT_STATE_CONFIG, // configuration received/disabled
    PREPROC_EFFECT_STATE_ACTIVE // active/enabled
};

/**
 * struct preproc_effect - effect context.
 * @itfe:
 * @proc_id: type of pre processor (enum preproc_id).
 * @state: current state (enum preproc_effect_state).
 * @session: session the effect is on.
 * @type: subtype of effect.
 */
struct preproc_effect {
    const struct effect_interface_s *itfe;
    uint32_t proc_id;
    uint32_t state;
    struct preproc_session *session;
    uint32_t type;
};

/**
 * struct preproc_session - Session context.
 * @effects: effects in this session.
 * @algo: algorithm handle in this session.
 * @profile: profile in this session.
 * @state: current state (enum preproc_session_state).
 * @id: audio session ID.
 * @io: handle of input stream this session is on.
 * @frame_count: frame count.
 * @sampling_rate: sampling rate at effect process interface.
 * @in_channel_count: input channel count.
 * @out_channel_count: output channel count.
 * @created_msk: bit field containing IDs of crested pre processors.
 * @enabled_msk: bit field containing IDs of enabled pre processors.
 * @processed_msk: bit field containing IDs of pre processors already.
 * @rev_channel_count: number of channels on reverse stream.
 * @rev_enabled_msk: bit field containing IDs of enabled pre processors with
 *                   reverse channel.
 * @rev_processed_msk: bit field containing IDs of pre processors with reverse
 *                     channel already processed in current round.
 */
struct preproc_session {
    struct preproc_effect effects[PREPROC_NUM_EFFECTS];
    struct algo algo;
    struct profile profile;
    uint32_t state;
    int id;
    int io;
    size_t frame_count;
    uint32_t sampling_rate;
    uint32_t in_channel_count;
    uint32_t out_channel_count;
    uint32_t created_msk;
    uint32_t enabled_msk;
    uint32_t processed_msk;
    uint32_t rev_channel_count;
    uint32_t rev_enabled_msk;
    uint32_t rev_processed_msk;
};

//------------------------------------------------------------------------------
// Effect descriptors
//------------------------------------------------------------------------------

static const effect_descriptor_t *descriptors[PREPROC_NUM_EFFECTS] = {
    &aec_normal_descriptor,
    &aec_delay_descriptor,
    &aec_array_reset_descriptor,
    &bf_fast_aec_descriptor,
    &bf_wakeup_descriptor,
    &bf_dereverb_descriptor,
    &bf_nlp_descriptor,
    &bf_aes_descriptor,
    &bf_agc_descriptor,
    &bf_anr_descriptor,
    &bf_gsc_descriptor,
    &bf_gsc_method_descriptor,
    &bf_fix_descriptor,
    &bf_dtd_descriptor,
    &bf_cng_descriptor,
    &bf_eq_descriptor,
    &bf_chn_select_descriptor,
    &bf_howling_descriptor,
    &bf_doa_descriptor,
    &bf_wind_descriptor,
    &bf_ainr_descriptor,
    &rx_anr_descriptor,
    &rx_howling_descriptor,
    &ref_hw_descriptor,
    &ref_sw_dlp_descriptor,
    &ref_sw_alp_playback_descriptor,
    &ref_sw_alp_capture_descriptor,
};

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

static uint32_t uuid_to_proc_id(const effect_uuid_t *uuid)
{
    size_t i;

    for (i = 0; i < PREPROC_NUM_EFFECTS; i++) {
        if (!memcmp(uuid, &descriptors[i]->uuid, sizeof(*uuid)))
            break;
    }

    return i;
}

static bool has_reverse_stream(uint32_t proc_id)
{
    if (proc_id == PREPROC_AEC_NORMAL ||
        proc_id == PREPROC_AEC_DELAY ||
        proc_id == PREPROC_AEC_ARRAY_RESET) {
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
// Effect functions
//------------------------------------------------------------------------------

static void session_set_proc_enabled(struct preproc_session *session, uint32_t proc_id,
                                     bool enabled);

const struct effect_interface_s effect_interface;
const struct effect_interface_s effect_interface_reverse;

#define BAD_STATE_ABORT(from, to) LOG_ALWAYS_FATAL("Bad state transition from %d to %d", from, to);

int effect_set_state(struct preproc_effect *effect, uint32_t state)
{
    int ret = 0;

    ALOGV("%s: proc %d, new %d old %d", __func__, effect->proc_id, state, effect->state);

    switch (state) {
    case PREPROC_EFFECT_STATE_INIT:
        switch (effect->state) {
        case PREPROC_EFFECT_STATE_ACTIVE:
            session_set_proc_enabled(effect->session, effect->proc_id, false);
            break;
        case PREPROC_EFFECT_STATE_CONFIG:
        case PREPROC_EFFECT_STATE_CREATED:
        case PREPROC_EFFECT_STATE_INIT:
            break;
        default:
            BAD_STATE_ABORT(effect->state, state);
        }
        break;
    case PREPROC_EFFECT_STATE_CREATED:
        switch (effect->state) {
        case PREPROC_EFFECT_STATE_INIT:
            break;
        case PREPROC_EFFECT_STATE_CREATED:
        case PREPROC_EFFECT_STATE_ACTIVE:
        case PREPROC_EFFECT_STATE_CONFIG:
            ALOGE("%s: invalid transition", __func__);
            ret = -ENOSYS;
            break;
        default:
            BAD_STATE_ABORT(effect->state, state);
        }
        break;
    case PREPROC_EFFECT_STATE_CONFIG:
        switch (effect->state) {
        case PREPROC_EFFECT_STATE_INIT:
            ALOGE("%s: invalid transition", __func__);
            ret = -ENOSYS;
            break;
        case PREPROC_EFFECT_STATE_ACTIVE:
            session_set_proc_enabled(effect->session, effect->proc_id, false);
            break;
        case PREPROC_EFFECT_STATE_CREATED:
        case PREPROC_EFFECT_STATE_CONFIG:
            break;
        default:
            BAD_STATE_ABORT(effect->state, state);
        }
        break;
    case PREPROC_EFFECT_STATE_ACTIVE:
        switch (effect->state) {
        case PREPROC_EFFECT_STATE_INIT:
        case PREPROC_EFFECT_STATE_CREATED:
            ALOGE("%s: invalid transition", __func__);
            ret = -ENOSYS;
            break;
        case PREPROC_EFFECT_STATE_ACTIVE:
            /* enabling an already enabled effect */
            break;
        case PREPROC_EFFECT_STATE_CONFIG:
            session_set_proc_enabled(effect->session, effect->proc_id, true);
            break;
        default:
            BAD_STATE_ABORT(effect->state, state);
        }
        break;
    default:
        BAD_STATE_ABORT(effect->state, state);
    }

    if (!ret)
        effect->state = state;

    return ret;
}

static int effect_init(struct preproc_effect *effect, uint32_t proc_id)
{
    if (has_reverse_stream(proc_id))
        effect->itfe = &effect_interface_reverse;
    else
        effect->itfe = &effect_interface;

    effect->proc_id = proc_id;
    effect->state = PREPROC_EFFECT_STATE_INIT;

    return 0;
}

static int effect_create(struct preproc_effect *effect, struct preproc_session *session,
                         effect_handle_t *interface)
{
    effect->session = session;
    *interface = (effect_handle_t)&effect->itfe;

    return effect_set_state(effect, PREPROC_EFFECT_STATE_CREATED);
}

static int effect_release(struct preproc_effect *effect)
{
    return effect_set_state(effect, PREPROC_EFFECT_STATE_INIT);
}

//------------------------------------------------------------------------------
// Session functions
//------------------------------------------------------------------------------

#define PREPROC_DEFAULT_SAMPLING_RATE   16000
#define PREPROC_DEFAULT_CHANNEL         1

static int session_init(struct preproc_session *session)
{
    size_t i;
    int ret = 0;

    session->state = PREPROC_SESSION_STATE_INIT;
    session->id = 0;
    session->io = 0;
    session->created_msk = 0;
    for (i = 0; i < PREPROC_NUM_EFFECTS && !ret; i++)
        ret = effect_init(&session->effects[i], i);

    return ret;
}

static int session_create_effect(struct preproc_session *session, int32_t proc_id,
                                 effect_handle_t *interface)
{
    int ret = -ENOMEM;

    ALOGV("%s proc_id %d, created_msk %08x", __func__, proc_id, session->created_msk);

    if (!session->created_msk) {
        session->frame_count = PREPROC_DEFAULT_SAMPLING_RATE / 100;
        session->sampling_rate = PREPROC_DEFAULT_SAMPLING_RATE;
        session->in_channel_count = PREPROC_DEFAULT_CHANNEL;
        session->out_channel_count = PREPROC_DEFAULT_CHANNEL;
        session->rev_channel_count = 0;
        session->enabled_msk = 0;
        session->processed_msk = 0;
        session->rev_enabled_msk = 0;
        session->rev_processed_msk = 0;
    }

    ret = effect_create(&session->effects[proc_id], session, interface);
    if (ret < 0)
        goto error;

    ALOGV("%s OK", __func__);
    session->created_msk |= BIT(proc_id);
    return ret;

error:
    if (!session->created_msk) {
        /*
         * Release sources here if it's the last one effect and we have
         * allocated sources before.
         */
    }

    return ret;
}

static int session_release_effect(struct preproc_session *session, struct preproc_effect *effect)
{
    if (effect_release(effect))
        ALOGW("failed to release proc ID %d", effect->proc_id);

    if (effect->proc_id == PREPROC_REF_SW_ALP_PLAYBACK)
        aloop_release_playback();
    else if (effect->proc_id == PREPROC_REF_SW_ALP_CAPTURE)
        aloop_release_capture();

    session->created_msk &= ~BIT(effect->proc_id);
    if (!session->created_msk) {
        session->id = 0;
        profile_release(&session->profile);
        algo_release(&session->algo);
    }

    return 0;
}

static int session_set_config(struct preproc_session *session, effect_config_t *config,
                              struct preproc_effect *effect)
{
    uint32_t in_channel_count = audio_channel_count_from_in_mask(config->inputCfg.channels);
    uint32_t out_channel_count = audio_channel_count_from_in_mask(config->outputCfg.channels);

    if (effect->proc_id == PREPROC_REF_SW_ALP_PLAYBACK) {
        in_channel_count = audio_channel_count_from_out_mask(config->inputCfg.channels);
        out_channel_count = audio_channel_count_from_out_mask(config->outputCfg.channels);
    } else {
        in_channel_count = audio_channel_count_from_in_mask(config->inputCfg.channels);
        out_channel_count = audio_channel_count_from_in_mask(config->outputCfg.channels);
    }

    if (config->inputCfg.samplingRate != config->outputCfg.samplingRate ||
        config->inputCfg.format != config->outputCfg.format ||
        config->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT) {
        return -EINVAL;
    }

    ALOGV("%s: sampling rate %d channel mask %08x", __func__, config->inputCfg.samplingRate,
          config->inputCfg.channels);

    session->sampling_rate = config->inputCfg.samplingRate;
    session->frame_count = session->sampling_rate / 100;
    session->in_channel_count = in_channel_count;
    session->out_channel_count = out_channel_count;

    session->state = PREPROC_SESSION_STATE_CONFIG;

    return 0;
}

static void session_get_config(struct preproc_session *session, effect_config_t *config,
                               struct preproc_effect *effect)
{
    memset(config, 0, sizeof(effect_config_t));
    config->inputCfg.samplingRate = session->sampling_rate;
    config->outputCfg.samplingRate = session->sampling_rate;
    config->inputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    config->outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;

    if (effect->proc_id == PREPROC_REF_SW_ALP_PLAYBACK) {
        config->inputCfg.channels = audio_channel_out_mask_from_count(session->in_channel_count);
        config->outputCfg.channels = audio_channel_out_mask_from_count(session->out_channel_count);
    } else {
        config->inputCfg.channels = audio_channel_in_mask_from_count(session->in_channel_count);
        config->outputCfg.channels = audio_channel_in_mask_from_count(session->out_channel_count);
    }

    config->inputCfg.mask = (EFFECT_CONFIG_SMP_RATE |
                             EFFECT_CONFIG_CHANNELS |
                             EFFECT_CONFIG_FORMAT);
    config->outputCfg.mask = (EFFECT_CONFIG_SMP_RATE |
                              EFFECT_CONFIG_CHANNELS |
                              EFFECT_CONFIG_FORMAT);
}

static int session_set_reverse_config(struct preproc_session *session, effect_config_t *config)
{
    if (config->inputCfg.samplingRate != config->outputCfg.samplingRate ||
        config->inputCfg.format != config->outputCfg.format ||
        config->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT) {
        return -EINVAL;
    }

    ALOGV("%s: sampling rate %d, channel mask %08x", __func__, config->inputCfg.samplingRate,
          config->inputCfg.channels);

    if (session->state < PREPROC_SESSION_STATE_CONFIG)
        return -ENOSYS;

    if (config->inputCfg.samplingRate != session->sampling_rate ||
        config->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT) {
        return -EINVAL;
    }

    session->rev_channel_count = audio_channel_count_from_out_mask(config->inputCfg.channels);

    return 0;
}

static void session_get_reverse_config(struct preproc_session *session, effect_config_t *config,
                                       struct preproc_effect *effect)
{
    memset(config, 0, sizeof(effect_config_t));
    config->inputCfg.samplingRate = config->outputCfg.samplingRate = session->sampling_rate;
    config->inputCfg.format = config->outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;

    if (effect->proc_id == PREPROC_REF_SW_ALP_PLAYBACK) {
        config->inputCfg.channels = audio_channel_out_mask_from_count(session->rev_channel_count);
        config->outputCfg.channels = audio_channel_out_mask_from_count(session->rev_channel_count);
    } else {
        config->inputCfg.channels = audio_channel_in_mask_from_count(session->rev_channel_count);
        config->outputCfg.channels = audio_channel_in_mask_from_count(session->rev_channel_count);
    }

    config->inputCfg.mask = config->outputCfg.mask = (EFFECT_CONFIG_SMP_RATE |
                                                      EFFECT_CONFIG_CHANNELS |
                                                      EFFECT_CONFIG_FORMAT);
}

static void session_set_proc_enabled(struct preproc_session *session, uint32_t proc_id,
                                     bool enabled)
{
    if (enabled) {
        session->enabled_msk |= BIT(proc_id);
        if (has_reverse_stream(proc_id))
            session->rev_enabled_msk |= BIT(proc_id);

    } else {
        session->enabled_msk &= ~BIT(proc_id);
        if (has_reverse_stream(proc_id))
            session->rev_enabled_msk &= ~BIT(proc_id);
    }

    ALOGV("%s: proc %d, enabled %d enabled_msk %08x rev_enabled_msk %08x", __func__, proc_id,
          enabled, session->enabled_msk, session->rev_enabled_msk);

    session->processed_msk = 0;
    if (has_reverse_stream(proc_id))
        session->rev_processed_msk = 0;
}

//------------------------------------------------------------------------------
// Bundle functions
//------------------------------------------------------------------------------

static int init_status = 1;
static struct preproc_session sessions[PREPROC_NUM_SESSIONS];

static struct preproc_session *preproc_get_session(int32_t proc_id, int32_t session_id,
                                                   int32_t io_id)
{
    size_t i;

    for (i = 0; i < PREPROC_NUM_SESSIONS; i++) {
        if (sessions[i].id == session_id) {
            if (sessions[i].created_msk & BIT(proc_id))
                return NULL;

            return &sessions[i];
        }
    }

    for (i = 0; i < PREPROC_NUM_SESSIONS; i++) {
        if (sessions[i].id == 0) {
            sessions[i].id = session_id;
            sessions[i].io = io_id;
            return &sessions[i];
        }
    }

    return NULL;
}

static int preproc_init(void)
{
    size_t i;
    int ret = 0;

    /*
     * NOTE: It's a bundle of effects, if one of the effects is failed to be
     * initialized or all the effects are intialized, we should mark the status
     * and return the status.
     */
    if (init_status <= 0)
        return init_status;

    for (i = 0; i < PREPROC_NUM_SESSIONS && !ret; i++)
        ret = session_init(&sessions[i]);

    init_status = ret;
    return init_status;
}

static const effect_descriptor_t *uuid_to_descriptor(const effect_uuid_t *uuid)
{
    size_t i;

    for (i = 0; i < PREPROC_NUM_EFFECTS; i++) {
        if (!memcmp(&descriptors[i]->uuid, uuid, sizeof(effect_uuid_t)))
            return descriptors[i];
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Effect Control Interface Implementation
//------------------------------------------------------------------------------

static int preproc_fx_process(effect_handle_t self, audio_buffer_t *input, audio_buffer_t *output)
{
    struct preproc_effect *effect = (struct preproc_effect *)self;
    struct preproc_session *session;

    if (!effect) {
        ALOGV("%s: invalid effect ", __func__);
        return -EINVAL;
    }

    session = (struct preproc_session *)effect->session;

    if (!input || !input->raw || !output || !output->raw) {
        ALOGW("%s: bad pointer", __func__);
        return -EINVAL;
    }

    if (input->frameCount != output->frameCount) {
        ALOGW("input frames %zu is not equal to output frames %zu", input->frameCount,
              output->frameCount);
        return -EINVAL;
    }

    if (session->state == PREPROC_SESSION_STATE_CONFIG) {
        if (session->enabled_msk & BIT(PREPROC_REF_SW_ALP_PLAYBACK)) {
            aloop_init_playback(session->sampling_rate, session->in_channel_count,
                                input->frameCount);
        } else {
            if (session->enabled_msk & BIT(PREPROC_REF_SW_ALP_CAPTURE)) {
                /*
                 * FIXME: The reverse channels can be different when the aloop
                 * playback is bound to an USB output device. The primary output
                 * devices we currently use all output in 2 channels.
                 */
                session->rev_channel_count = 2;
                aloop_init_capture(session->sampling_rate, session->rev_channel_count,
                                   input->frameCount);
            }

            if (session->enabled_msk & (BIT(PREPROC_AEC_NORMAL) |
                                        BIT(PREPROC_AEC_DELAY) |
                                        BIT(PREPROC_AEC_ARRAY_RESET))) {
                if (!(session->enabled_msk & (BIT(PREPROC_REF_HW) |
                                              BIT(PREPROC_REF_SW_DLP) |
                                              BIT(PREPROC_REF_SW_ALP_CAPTURE)))) {
                    /* We must specify one of the reference type. */
                    ALOGW("%s: the reverse stream has not been sourced from anywhere", __func__);
                }

                if (!session->rev_channel_count) {
                    /* We must specify the reverse channel. */
                    ALOGW("%s: there is none reverse channel", __func__);
                }
            }

            if (profile_init(&session->profile, session->sampling_rate, input->frameCount,
                             session->in_channel_count, session->rev_channel_count,
                             session->enabled_msk)) {
                ALOGD("%s: failed to initialize profile", __func__);
                return -EINVAL;
            }

            if (algo_init(&session->algo, &session->profile, session->enabled_msk)) {
                ALOGD("%s: failed to initialize algorithm", __func__);
                return -EINVAL;
            }
        }

        session->state = PREPROC_SESSION_STATE_PROCESS;
    }

    /* NOTE: Gathering up all the effects, and process the input together. */
    session->processed_msk |= BIT(effect->proc_id);

    if ((session->processed_msk & session->enabled_msk) == session->enabled_msk) {
        /* Mark to be processed. */
        effect->session->processed_msk = 0;

        if (session->enabled_msk & BIT(PREPROC_REF_SW_ALP_PLAYBACK)) {
            /* Cache the output stream to aloop. */
            aloop_cache_buffer(input);
        } else {
            if (session->enabled_msk & BIT(PREPROC_REF_SW_ALP_CAPTURE)) {
                audio_buffer_t reference;

                reference.frameCount = input->frameCount;
                aloop_obtain_buffer(&reference);
                /* Merge the reverse stream and pure input stream. */
                algo_process_merged(&session->algo, input, output, &reference);
            } else {
                /* Input stream is already interleaved with reverse stream. */
                algo_process(&session->algo, input, output);
            }
        }
    }

    return 0;
}

static int preproc_fx_command(effect_handle_t self, uint32_t cmd_code, uint32_t cmd_size,
                              void *cmd_data, uint32_t *reply_size, void *reply_data)
{
    struct preproc_effect *effect = (struct preproc_effect *)self;
    int ret = 0;

    if (!effect)
        return -EINVAL;

    ALOGV("%s: command %d, cmd_size %d", __func__, cmd_code, cmd_size);

    switch (cmd_code) {
    case EFFECT_CMD_INIT:
        if (!reply_data || *reply_size != sizeof(int))
            return -EINVAL;

        *(int *)reply_data = ret;
        break;

    case EFFECT_CMD_SET_CONFIG:
        if (!cmd_data || cmd_size != sizeof(effect_config_t) || !reply_data ||
            *reply_size != sizeof(int)) {
            ALOGE("%s: invalid parameters", __func__);
            return -EINVAL;
        }

        ret = session_set_config(effect->session, (effect_config_t *)cmd_data, effect);
        if (!ret) {
            if (effect->state != PREPROC_EFFECT_STATE_ACTIVE)
                ret = effect_set_state(effect, PREPROC_EFFECT_STATE_CONFIG);
        }

        *(int *)reply_data = ret;
        break;

    case EFFECT_CMD_GET_CONFIG:
        if (!reply_data || *reply_size != sizeof(effect_config_t)) {
            ALOGE("%s: invalid paramters", __func__);
            return -EINVAL;
        }

        session_get_config(effect->session, (effect_config_t *)reply_data, effect);
        break;

    case EFFECT_CMD_SET_CONFIG_REVERSE:
        if (!cmd_data || cmd_size != sizeof(effect_config_t) || !reply_data ||
            *reply_size != sizeof(int)) {
            ALOGE("%s: invalid paramters", __func__);
            return -EINVAL;
        }

        ret = session_set_reverse_config(effect->session, (effect_config_t *)cmd_data);
        *(int *)reply_data = ret;
        break;

    case EFFECT_CMD_GET_CONFIG_REVERSE:
        if (!reply_data || *reply_size != sizeof(effect_config_t)) {
            ALOGE("%s: invalid paramters", __func__);
            return -EINVAL;
        }

        session_get_reverse_config(effect->session, (effect_config_t *)cmd_data, effect);
        break;

    case EFFECT_CMD_RESET:
        /* Not supported. */
        break;

    case EFFECT_CMD_GET_PARAM:
        /*
         * Not supported. All the parameters is fixed up with the hardware
         * features, and is configured in the profile.
         */
        break;

    case EFFECT_CMD_SET_PARAM:
        /*
         * Not supported. All the parameters is fixed up with the hardware
         * features, and is configured in the profile.
         */
        *(int *)reply_data = ret;

        break;

    case EFFECT_CMD_ENABLE:
        if (!reply_data || !reply_size || *reply_size != sizeof(int)) {
            ALOGE("%s: invalid parameters", __func__);
            return -EINVAL;
        }

        ret = effect_set_state(effect, PREPROC_EFFECT_STATE_ACTIVE);
        *(int *)reply_data = ret;
        break;

    case EFFECT_CMD_DISABLE:
        if (!reply_data || !reply_size || *reply_size != sizeof(int)) {
            ALOGE("%s: invalid parameters", __func__);
            return -EINVAL;
        }

        ret = effect_set_state(effect, PREPROC_EFFECT_STATE_CONFIG);
        *(int *)reply_data = ret;
        break;

    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_INPUT_DEVICE:
        if (!cmd_data || cmd_size != sizeof(uint32_t)) {
            ALOGE("%s: invalid parameters", __func__);
            return -EINVAL;
        }

        /*
         * If we want this session is exclusive binding to some devices, mark
         * the session is invalid here if the device is expected, or vice versa.
         *
         * Do not return errors.
         */

        *(int *)reply_data = ret;
        break;

    case EFFECT_CMD_SET_VOLUME:
    case EFFECT_CMD_SET_AUDIO_MODE:
        /* Not supported. */
        break;
    default:
        ALOGE("%s: unexpected command %d", __func__, cmd_code);
        return -EINVAL;
    }

    return 0;
}

static int preproc_fx_get_descriptor(effect_handle_t self, effect_descriptor_t *descriptor)
{
    struct preproc_effect *effect = (struct preproc_effect *)self;

    if (!effect || !descriptor)
        return -EINVAL;

    *descriptor = *descriptors[effect->proc_id];

    return 0;
}

static int preproc_fx_process_reverse(effect_handle_t self, audio_buffer_t *input,
                                      audio_buffer_t *output)
{
    struct preproc_effect *effect = (struct preproc_effect *)self;
    struct preproc_session *session;

    if (!effect) {
        ALOGW("%s: invalid effect", __func__);
        return -EINVAL;
    }

    session = (struct preproc_session *)effect->session;

    if (!input || !input->raw) {
        ALOGW("%s: bad pointer", __func__);
        return -EINVAL;
    }

    if (input->frameCount != output->frameCount) {
        ALOGW("input frames %zu is not equal to output frames %zu", input->frameCount,
              output->frameCount);
        return -EINVAL;
    }

    if (input->frameCount != session->frame_count) {
        ALOGW("input frames %zu != %zu representing 10ms at sampling rate %d",
              input->frameCount, session->frame_count, session->sampling_rate);
        return -EINVAL;
    }

    session->rev_processed_msk |= BIT(effect->proc_id);

    if ((session->rev_processed_msk & session->rev_enabled_msk) == session->rev_enabled_msk) {
        effect->session->rev_processed_msk = 0;
        /* TODO: Process reverse stream here? */
    }

    return 0;
}

// effect_handle_t interface implementation for effect
const struct effect_interface_s effect_interface = {
    .process = preproc_fx_process,
    .command = preproc_fx_command,
    .get_descriptor = preproc_fx_get_descriptor,
    .process_reverse = NULL,
};

const struct effect_interface_s effect_interface_reverse = {
    .process = preproc_fx_process,
    .command = preproc_fx_command,
    .get_descriptor = preproc_fx_get_descriptor,
    .process_reverse = preproc_fx_process_reverse
};

//------------------------------------------------------------------------------
// Effect Library Interface Implementation
//------------------------------------------------------------------------------

static int32_t preproc_create_effect(const effect_uuid_t *uuid, int32_t session_id, int32_t io_id,
                                     effect_handle_t *interface)
{
    const effect_descriptor_t *desc;
    struct preproc_session *session;
    uint32_t proc_id;
    int ret;

    ALOGV("%s: uuid: %08x session %d IO: %d", __func__, uuid->timeLow, session_id, io_id);

    if (preproc_init() != 0)
        return init_status;

    desc = uuid_to_descriptor(uuid);
    if (!desc) {
        ALOGW("%s: fx not found uuid: %08x", __func__, uuid->timeLow);
        return -EINVAL;
    }
    proc_id = uuid_to_proc_id(&desc->uuid);

    session = preproc_get_session(proc_id, session_id, io_id);
    if (!session) {
        ALOGW("%s: no more session available", __func__);
        return -EINVAL;
    }

    ret = session_create_effect(session, proc_id, interface);

    if (ret < 0 && !session->created_msk)
        session->id = 0;

    return ret;
}

static int32_t preproc_release_effect(effect_handle_t interface)
{
    struct preproc_effect *effect;

    ALOGV("%s: interface %p", __func__, interface);

    if (preproc_init() != 0)
        return init_status;

    effect = (struct preproc_effect *)interface;

    if (effect->session->id == 0)
        return -EINVAL;

    return session_release_effect(effect->session, effect);
}

static int32_t preproc_get_descriptor(const effect_uuid_t *uuid, effect_descriptor_t *descriptor)
{
    const effect_descriptor_t *desc;

    if (!descriptor || !uuid)
        return -EINVAL;

    desc = uuid_to_descriptor(uuid);
    if (!desc) {
        ALOGV("%s: not found", __func__);
        return -EINVAL;
    }

    ALOGV("%s: got fx %s", __func__, desc->name);

    *descriptor = *desc;

    return 0;
}

// This is the only symbol that needs to be exported
__attribute__((visibility("default"))) audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    .tag = AUDIO_EFFECT_LIBRARY_TAG,
    .version = EFFECT_LIBRARY_API_VERSION,
    .name = "Audio Preprocessing Library",
    .implementor = "Rockchip Electronics Co. Ltd.",
    .create_effect = preproc_create_effect,
    .release_effect = preproc_release_effect,
    .get_descriptor = preproc_get_descriptor,
};

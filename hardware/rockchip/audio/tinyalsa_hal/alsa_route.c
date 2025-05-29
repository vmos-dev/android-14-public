/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd.
 */

/*
 * @file alsa_route.c
 * @brief 
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#define LOG_TAG "alsa_route"

//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/config_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include <linux/ioctl.h>
#include <system/audio.h>
#include "audio_hw.h"

#include "alsa_mixer.h"
#include "alsa_route.h"

#define __force
#ifdef __bitwise
#undef __bitwise
#endif
#define __bitwise
#define __user
#include "asound.h"

#include "codec_config/config_list.h"

#define PCM_DEVICE0_PLAYBACK 0
#define PCM_DEVICE0_CAPTURE 1
#define PCM_DEVICE1_PLAYBACK 2
#define PCM_DEVICE1_CAPTURE 3
#define PCM_DEVICE2_PLAYBACK 4
#define PCM_DEVICE2_CAPTURE 5

#define PCM_MAX PCM_DEVICE2_CAPTURE

static const struct config_route_table *route_table = NULL;
static int route_card = -1;

struct pcm* mPcm[PCM_MAX + 1];
struct mixer* mMixerPlayback;
struct mixer* mMixerCapture;

/**
 * @brief route_init 
 *
 * @returns 
 */
int route_init(void)
{
    char soundCardID[20] = "";
    static FILE * fp;
    unsigned i, config_count = sizeof(sound_card_config_list) / sizeof(struct alsa_sound_card_config);
    size_t read_size;

    ALOGV("route_init()");

    fp = fopen("/proc/asound/card0/id", "rt");
    if (!fp) {
        ALOGE("Open sound card0 id error!");
    } else {
        read_size = fread(soundCardID, sizeof(char), sizeof(soundCardID), fp);
        fclose(fp);

        if (soundCardID[read_size - 1] == '\n') {
            read_size--;
            soundCardID[read_size] = '\0';
        }

        ALOGV("Sound card0 is %s", soundCardID);

        for (i = 0; i < config_count; i++) {
            if (!(sound_card_config_list + i) || !sound_card_config_list[i].sound_card_name ||
                !sound_card_config_list[i].route_table)
                continue;

            if (strncmp(sound_card_config_list[i].sound_card_name, soundCardID, 
                read_size) == 0) {
                route_table = sound_card_config_list[i].route_table;
                ALOGD("Get route table for sound card0 %s", soundCardID);
            }
        }
    }

    if (!route_table) {
        route_table = &default_config_table;
        ALOGD("Can not get config table for sound card0 %s, so get default config table.", soundCardID);
    }

    for (i = PCM_DEVICE0_PLAYBACK; i < PCM_MAX; i++)
         mPcm[i] = NULL;

    return 0;
}

/**
 * @brief route_init
 *
 * @returns
 */
int route_card_init(int card)
{
    char soundcard[32];
    char soundCardID[20] = "";
    static FILE * fp;
    unsigned i, config_count = sizeof(sound_card_config_list) / sizeof(struct alsa_sound_card_config);
    size_t read_size;

    ALOGV("route_card_init(card %d)", card);
    sprintf(soundcard, "/proc/asound/card%d/id", card);
    fp = fopen(soundcard, "rt");
    if (!fp) {
        ALOGE("Open %s error!", soundcard);
    } else {
        read_size = fread(soundCardID, sizeof(char), sizeof(soundCardID), fp);
        fclose(fp);

        if (soundCardID[read_size - 1] == '\n') {
            read_size--;
            soundCardID[read_size] = '\0';
        }

        ALOGV("Sound card%d is %s", card, soundCardID);

        for (i = 0; i < config_count; i++) {
            if (!(sound_card_config_list + i) || !sound_card_config_list[i].sound_card_name ||
                !sound_card_config_list[i].route_table)
                continue;

            if (strncmp(sound_card_config_list[i].sound_card_name, soundCardID,
                read_size) == 0) {
                route_table = sound_card_config_list[i].route_table;
                ALOGD("Get route table for sound card0 %s", soundCardID);
            }
        }
    }

    if (!route_table) {
        route_table = &default_config_table;
        ALOGD("Can not get config table for sound card0 %s, so get default config table.", soundCardID);
    }

    for (i = PCM_DEVICE0_PLAYBACK; i < PCM_MAX; i++)
         mPcm[i] = NULL;

    return 0;
}

/**
 * @brief route_uninit 
 */
void route_uninit(void)
{
    ALOGV("route_uninit()");
    route_pcm_close(PLAYBACK_OFF_ROUTE);

	route_pcm_close(CAPTURE_OFF_ROUTE);
}

/**
 * @brief is_playback_route 
 *
 * @param route
 *
 * @returns 
 */
int is_playback_route(unsigned route)
{
    switch (route) {
    case MAIN_MIC_CAPTURE_ROUTE:
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
    case CAPTURE_OFF_ROUTE:
    case USB_CAPTURE_ROUTE:
    case HDMI_IN_NORMAL_ROUTE:
    case HDMI_IN_OFF_ROUTE:
    case HDMI_IN_CAPTURE_ROUTE:
    case HDMI_IN_CAPTURE_OFF_ROUTE:
        return 0;
    case SPEAKER_NORMAL_ROUTE:
    case SPEAKER_INCALL_ROUTE:
    case SPEAKER_RINGTONE_ROUTE:
    case SPEAKER_VOIP_ROUTE:
    case EARPIECE_NORMAL_ROUTE:
    case EARPIECE_INCALL_ROUTE:
    case EARPIECE_RINGTONE_ROUTE:
    case EARPIECE_VOIP_ROUTE:
    case HEADPHONE_NORMAL_ROUTE:
    case HEADPHONE_INCALL_ROUTE:
    case HEADPHONE_RINGTONE_ROUTE:
    case SPEAKER_HEADPHONE_NORMAL_ROUTE:
    case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
    case HEADPHONE_VOIP_ROUTE:
    case HEADSET_NORMAL_ROUTE:
    case HEADSET_INCALL_ROUTE:
    case HEADSET_RINGTONE_ROUTE:
    case HEADSET_VOIP_ROUTE:
    case BLUETOOTH_NORMAL_ROUTE:
    case BLUETOOTH_INCALL_ROUTE:
    case BLUETOOTH_VOIP_ROUTE:
    case PLAYBACK_OFF_ROUTE:
    case INCALL_OFF_ROUTE:
    case VOIP_OFF_ROUTE:
    case HDMI_NORMAL_ROUTE:
    case USB_NORMAL_ROUTE:
    case SPDIF_NORMAL_ROUTE:
        return 1;
    default:
        ALOGE("is_playback_route() Error route %d", route);
        return -EINVAL;
    }
}

/**
 * @brief route_set_input_source 
 *
 * @param source
 *
 * @returns 
 */
int route_set_input_source(const char *source)
{
    struct mixer* mMixer = mMixerCapture;

    if (mMixer == NULL || source[0] == '\0') return 0;

    struct mixer_ctl *ctl= mixer_get_control(mMixer, "Input Source", 0);

    if (ctl == NULL)
        return 0;

    ALOGV("mixer_ctl_select, Input Source, (%s)", source);
    return mixer_ctl_select(ctl, source);
}

/**
 * @brief route_set_voice_volume 
 *
 * @param ctlName
 * @param volume
 *
 * @returns 
 */
int route_set_voice_volume(const char *ctlName, float volume)
{
    struct mixer* mMixer = mMixerPlayback;

    if (mMixer == NULL || ctlName[0] == '\0')
        return 0;

    struct mixer_ctl *ctl = mixer_get_control(mMixer, ctlName, 0);
    if (ctl == NULL)
        return 0;

    long long vol, vol_min, vol_max;
    unsigned int Nmax = 6, N = volume * 5 + 1;
    float e = 2.71828, dB_min, dB_max, dB_vol, dB_step, volFloat;

    ALOGD("route_set_voice_volume() set incall voice volume %f to control %s", volume, ctlName);

    if (mixer_get_ctl_minmax(ctl, &vol_min, &vol_max) < 0) {
        ALOGE("mixer_get_dB_range() get control min max value fail");
        return 0;
    }

    mixer_get_dB_range(ctl, (long)vol_min, (long)vol_max, &dB_min, &dB_max, &dB_step);

    dB_vol = 20 * log((Nmax * pow(e, dB_min / 20) + N * (pow(e, dB_max / 20) - pow(e, dB_min / 20))) / Nmax);

    volFloat = vol_min + (dB_vol - dB_min) / dB_step;
    vol = (long long)volFloat;

    if (((unsigned)(volFloat * 10) % 10) >= 5)
        vol++;

    ALOGV("dB_min = %f, dB_step = %f, dB_max = %f, dB_vol = %f",
        dB_min,
        dB_step,
        dB_max,
        dB_vol);

    ALOGV("N = %u, volFloat = %f, vol = %lld", N, volFloat, vol);

    return mixer_ctl_set_int(ctl, vol);
}

/**
 * @brief get_route_config 
 *
 * @param route
 *
 * @returns 
 */
const struct config_route *get_route_config(unsigned route)
{
    ALOGV("get_route_config() route %d", route);

    if (!route_table) {
        ALOGE("get_route_config() route_table is NULL!");
        return NULL;
    }
    switch (route) {
    case SPEAKER_NORMAL_ROUTE:
        return &(route_table->speaker_normal);
    case SPEAKER_INCALL_ROUTE:
        return &(route_table->speaker_incall);
    case SPEAKER_RINGTONE_ROUTE:
        return &(route_table->speaker_ringtone);
    case SPEAKER_VOIP_ROUTE:
        return &(route_table->speaker_voip);
    case EARPIECE_NORMAL_ROUTE:
        return &(route_table->earpiece_normal);
    case EARPIECE_INCALL_ROUTE:
        return &(route_table->earpiece_incall);
    case EARPIECE_RINGTONE_ROUTE:
        return &(route_table->earpiece_ringtone);
    case EARPIECE_VOIP_ROUTE:
        return &(route_table->earpiece_voip);
    case HEADPHONE_NORMAL_ROUTE:
        return &(route_table->headphone_normal);
    case HEADPHONE_INCALL_ROUTE:
        return &(route_table->headphone_incall);
    case HEADPHONE_RINGTONE_ROUTE:
        return &(route_table->headphone_ringtone);
    case SPEAKER_HEADPHONE_NORMAL_ROUTE:
        return &(route_table->speaker_headphone_normal);
    case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
        return &(route_table->speaker_headphone_ringtone);
    case HEADPHONE_VOIP_ROUTE:
        return &(route_table->headphone_voip);
    case HEADSET_NORMAL_ROUTE:
        return &(route_table->headset_normal);
    case HEADSET_INCALL_ROUTE:
        return &(route_table->headset_incall);
    case HEADSET_RINGTONE_ROUTE:
        return &(route_table->headset_ringtone);
    case HEADSET_VOIP_ROUTE:
        return &(route_table->headset_voip);
    case BLUETOOTH_NORMAL_ROUTE:
        return &(route_table->bluetooth_normal);
    case BLUETOOTH_INCALL_ROUTE:
        return &(route_table->bluetooth_incall);
    case BLUETOOTH_VOIP_ROUTE:
        return &(route_table->bluetooth_voip);
    case MAIN_MIC_CAPTURE_ROUTE:
        return &(route_table->main_mic_capture);
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
        return &(route_table->hands_free_mic_capture);
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
        return &(route_table->bluetooth_sco_mic_capture);
    case PLAYBACK_OFF_ROUTE:
        return &(route_table->playback_off);
    case CAPTURE_OFF_ROUTE:
        return &(route_table->capture_off);
    case INCALL_OFF_ROUTE:
        return &(route_table->incall_off);
    case VOIP_OFF_ROUTE:
        return &(route_table->voip_off);
    case HDMI_NORMAL_ROUTE:
        return &(route_table->hdmi_normal);
    case USB_NORMAL_ROUTE:
        return &(route_table->usb_normal);
    case USB_CAPTURE_ROUTE:
        return &(route_table->usb_capture);
    case SPDIF_NORMAL_ROUTE:
        return &(route_table->spdif_normal);
    case HDMI_IN_NORMAL_ROUTE:
        return &(route_table->hdmiin_normal);
    case HDMI_IN_OFF_ROUTE:
        return &(route_table->hdmiin_off);
    case HDMI_IN_CAPTURE_ROUTE:
        return &(route_table->hdmiin_captrue);
    case HDMI_IN_CAPTURE_OFF_ROUTE:
        return &(route_table->hdmiin_captrue_off);
    default:
        ALOGE("get_route_config() Error route %d", route);
        return NULL;
    }
}

/**
 * @brief set_controls 
 *
 * @param mixer
 * @param ctls
 * @param ctls_count
 *
 * @returns 
 */
int set_controls(struct mixer *mixer, const struct config_control *ctls, const unsigned ctls_count)
{
    struct mixer_ctl *ctl;
    unsigned i;

    ALOGV("set_controls() ctls_count %d", ctls_count);

    if (!ctls || ctls_count <= 0) {
        ALOGV("set_controls() ctls is NULL");
        return 0;
    }

    for (i = 0; i < ctls_count; i++) {
        ctl = mixer_get_control(mixer, ctls[i].ctl_name, 0);
        if (!ctl) {
            ALOGE_IF(route_table != &default_config_table, "set_controls() Can not get ctl : %s", ctls[i].ctl_name);
            ALOGV_IF(route_table == &default_config_table, "set_controls() Can not get ctl : %s", ctls[i].ctl_name);
            continue;
        }

        if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_BOOLEAN &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER64 &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
            ALOGE("set_controls() ctl %s is not a type of INT or ENUMERATED", ctls[i].ctl_name);
            return -EINVAL;
        }

        if (ctls[i].str_val) {
            if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
                ALOGE("set_controls() ctl %s is not a type of ENUMERATED", ctls[i].ctl_name);
                return -EINVAL;
            }
            if (mixer_ctl_select(ctl, ctls[i].str_val) != 0) {
                ALOGE("set_controls() Can not set ctl %s to %s", ctls[i].ctl_name, ctls[i].str_val);
                return -EINVAL;
            }
            ALOGV("set_controls() set ctl %s to %s", ctls[i].ctl_name, ctls[i].str_val);
        } else {
            if (mixer_ctl_set_int_double(ctl, ctls[i].int_val[0], ctls[i].int_val[1]) != 0) {
                ALOGE("set_controls() can not set ctl %s to %d", ctls[i].ctl_name, ctls[i].int_val[0]);
                return -EINVAL;
            }
            ALOGV("set_controls() set ctl %s to %d", ctls[i].ctl_name, ctls[i].int_val[0]);
        }
    }

    return 0;
}

/**
 * @brief route_set_controls 
 *
 * @param route
 *
 * @returns 
 */
int route_set_controls(unsigned route)
{
    struct mixer* mMixer;

    if (route >= MAX_ROUTE) {
        ALOGE("route_set_controls() route %d error!", route);
        return -EINVAL;
    }

#ifdef SUPPORT_USB //usb input maybe used for primary
    if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!", route);
        return -EINVAL;
    }
#else //primary input maybe used for usb
    if (route > SPDIF_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != HDMI_IN_NORMAL_ROUTE &&
        route != HDMI_IN_OFF_ROUTE &&
        route != HDMI_IN_CAPTURE_ROUTE &&
        route != HDMI_IN_CAPTURE_OFF_ROUTE) {
        ALOGV("route %d error for codec or hdmi!", route);
        return -EINVAL;
    }
#endif

    ALOGD("route_set_controls() set route %d", route);

    mMixer = is_playback_route(route) ? mMixerPlayback : mMixerCapture;

    if (!mMixer) {
        ALOGE("route_set_controls() mMixer is NULL!");
        return -EINVAL;
    }

    const struct config_route *route_info = get_route_config(route);
    if (!route_info) {
        ALOGE("route_set_controls() Can not get config of route");
        return -EINVAL;
    }

    if (route_info->controls_count > 0)
        set_controls(mMixer, route_info->controls, route_info->controls_count);

    return 0;
}

/**
 * @brief route_pcm_open 
 *
 * @param route
 */
void route_pcm_open(uint32_t route)
{
    int is_playback;

    if (route >= MAX_ROUTE) {
        ALOGE("route_pcm_open() route %d error!", route);
        goto __exit;
    }

#ifdef SUPPORT_USB //usb input maybe used for primary
	
	if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!", route);
        goto __exit;
    }
#else //primary input maybe used for usb
    if (route > BLUETOOTH_SOC_MIC_CAPTURE_ROUTE &&
        route != HDMI_NORMAL_ROUTE &&
        route != SPDIF_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != HDMI_IN_NORMAL_ROUTE &&
        route != HDMI_IN_OFF_ROUTE &&
        route != PLAYBACK_OFF_ROUTE &&
        route != HDMI_IN_CAPTURE_ROUTE &&
        route != HDMI_IN_CAPTURE_OFF_ROUTE) {
        ALOGV("route %d error for codec or hdmi!", route);
        goto __exit;
    }
#endif

    ALOGV("route_pcm_open() route %d", route);

    is_playback = is_playback_route(route);

    if (!route_table) {
        route_init();
    }

    const struct config_route *route_info = get_route_config(route);
    if (!route_info) {
        ALOGE("route_pcm_open() Can not get config of route");
        goto __exit;
    }

    ALOGD("route_info->sound_card %d, route_info->devices 0 %s %s",
        route_info->sound_card,
        (route_info->devices == DEVICES_0_1 || route_info->devices == DEVICES_0_2 ||
        route_info->devices == DEVICES_0_1_2) ? (route_info->devices == DEVICES_0_2 ? "2" : "1") : "",
        route_info->devices == DEVICES_0_1_2 ? "2" : "");

   

    if (is_playback) {
        //close all route and pcm
        if (mMixerPlayback) {
            route_set_controls(INCALL_OFF_ROUTE);
            route_set_controls(VOIP_OFF_ROUTE);
        }
        route_pcm_close(PLAYBACK_OFF_ROUTE);

        //Open playback and capture of device 2
        
    } else {
        route_pcm_close(CAPTURE_OFF_ROUTE);
    }

    //update mMixer
    if (is_playback) {
        if (mMixerPlayback == NULL)
            mMixerPlayback = mixer_open_legacy(route_info->sound_card == 1 ? 0 : route_info->sound_card);
    } else {
        if (mMixerCapture == NULL)
            mMixerCapture = mixer_open_legacy(route_info->sound_card == 1 ? 0 : route_info->sound_card);
    }

    //set controls
    if (route_info->controls_count > 0)
        route_set_controls(route);
__exit:
	ALOGV("route_pcm_open exit");
}

/**
 * @brief route_pcm_open
 *
 * @param route
 */
void route_pcm_card_open(int card, uint32_t route)
{
    int is_playback;

    if (route >= MAX_ROUTE) {
        ALOGE("route_pcm_card_open() route %d error!", route);
        goto __exit;
    }
    if (card < 0) {
        ALOGE("route_pcm_card_open() card %d error!", card);
        goto __exit;
    }
#ifdef SUPPORT_USB //usb input maybe used for primary

	if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!", route);
        goto __exit;
    }
#else //primary input maybe used for usb
    if (route > BLUETOOTH_SOC_MIC_CAPTURE_ROUTE &&
        route != HDMI_NORMAL_ROUTE &&
        route != SPDIF_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != HDMI_IN_NORMAL_ROUTE &&
        route != HDMI_IN_OFF_ROUTE &&
        route != PLAYBACK_OFF_ROUTE) {
        ALOGV("route %d error for codec or hdmi!", route);
        goto __exit;
    }
#endif

    ALOGV("route_pcm_card_open(card %d, route %d)", card, route);

    is_playback = is_playback_route(route);

    if (!route_table || route_card != card) {
        route_card_init(card);
        route_card = card;
    }

    const struct config_route *route_info = get_route_config(route);
    if (!route_info) {
        ALOGE("route_pcm_open() Can not get config of route");
        goto __exit;
    }

    ALOGD("route_info->sound_card %d, route_info->devices 0 %s %s",
        route_info->sound_card,
        (route_info->devices == DEVICES_0_1 || route_info->devices == DEVICES_0_2 ||
        route_info->devices == DEVICES_0_1_2) ? (route_info->devices == DEVICES_0_2 ? "2" : "1") : "",
        route_info->devices == DEVICES_0_1_2 ? "2" : "");

    if (is_playback) {
        //close all route and pcm
        if (mMixerPlayback) {
            route_set_controls(INCALL_OFF_ROUTE);
            route_set_controls(VOIP_OFF_ROUTE);
        }
        route_pcm_close(PLAYBACK_OFF_ROUTE);

        //Open playback and capture of device 2

    } else {
        route_pcm_close(CAPTURE_OFF_ROUTE);
    }

    //update mMixer
    if (is_playback) {
        if (mMixerPlayback == NULL)
            mMixerPlayback = mixer_open_legacy(card);
    } else {
        if (mMixerCapture == NULL)
            mMixerCapture = mixer_open_legacy(card);
    }

    //set controls
    if (route_info->controls_count > 0)
        route_set_controls(route);
__exit:
	ALOGV("route_pcm_open exit");

}

/**
 * @brief route_pcm_close 
 *
 * @param route
 *
 * @returns 
 */
int route_pcm_close(unsigned route)
{
    unsigned i;

    if (route != PLAYBACK_OFF_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != INCALL_OFF_ROUTE &&
        route != VOIP_OFF_ROUTE &&
        route != HDMI_IN_CAPTURE_OFF_ROUTE) {
        ALOGE("route_pcm_close() is not a off route");
        return 0;
    }

    ALOGV("route_pcm_close() route %d", route);

	//set controls
    if (is_playback_route(route) ? mMixerPlayback : mMixerCapture)
        route_set_controls(route);

    //close mixer
    if (route == PLAYBACK_OFF_ROUTE) {
        if (mMixerPlayback) {
            mixer_close_legacy(mMixerPlayback);
            mMixerPlayback = NULL;
        }
    } else if (route == CAPTURE_OFF_ROUTE) {
        if (mMixerCapture) {
            mixer_close_legacy(mMixerCapture);
            mMixerCapture = NULL;
        }
    }

    return 0;
}

static int route_get_config_table(int card, const struct config_route_table **table)
{
    FILE *file;
    const struct config_route_table *found = NULL;
    char path[NAME_MAX];
    char id[16]; /* Copy from include/sound/core.h */
    size_t len;
    int i;

    sprintf(path, "/proc/asound/card%d/id", card);
    file = fopen(path, "rt");

    if (!file) {
        ALOGE("Open %s error!", path);
        return -ENODEV;
    }

    len = fread(id, sizeof(char), sizeof(id), file);

    if (id[len - 1] == '\n') {
        id[len - 1] = '\0';
        len--;
    }

    for (i = 0; i < ARRAY_SIZE(sound_card_config_list); i++) {
        if (!strncmp(sound_card_config_list[i].sound_card_name, id, len)) {
            found = sound_card_config_list[i].route_table;
            ALOGD("Find out configuration table for %s", id);
        }
    }

    if (!found) {
        found = &default_config_table;
        ALOGD("Use default configuration table for %s", id);
    }

    *table = found;

    fclose(file);

    return 0;
}

static int route_get_config(unsigned int route, const struct config_route_table *table,
                            const struct config_route **config)
{
    if (!table) {
        ALOGE("%s: invalid configuration table", __FUNCTION__);
        return -EINVAL;
    }

    /*
     * TODO: We should use an array to store route configurations instead of
     * structure members. The code looks so long, an array is better to replace
     * structure members in this case.
     */
    switch (route) {
    case SPEAKER_NORMAL_ROUTE:
        *config = &table->speaker_normal;
        break;
    case SPEAKER_INCALL_ROUTE:
        *config = &table->speaker_incall;
        break;
    case SPEAKER_RINGTONE_ROUTE:
        *config = &table->speaker_ringtone;
        break;
    case SPEAKER_VOIP_ROUTE:
        *config = &table->speaker_voip;
        break;
    case EARPIECE_NORMAL_ROUTE:
        *config = &table->earpiece_normal;
        break;
    case EARPIECE_INCALL_ROUTE:
        *config = &table->earpiece_incall;
        break;
    case EARPIECE_RINGTONE_ROUTE:
        *config = &table->earpiece_ringtone;
        break;
    case EARPIECE_VOIP_ROUTE:
        *config = &table->earpiece_voip;
        break;
    case HEADPHONE_NORMAL_ROUTE:
        *config = &table->headphone_normal;
        break;
    case HEADPHONE_INCALL_ROUTE:
        *config = &table->headphone_incall;
        break;
    case HEADPHONE_RINGTONE_ROUTE:
        *config = &table->headphone_ringtone;
        break;
    case SPEAKER_HEADPHONE_NORMAL_ROUTE:
        *config = &table->speaker_headphone_normal;
        break;
    case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
        *config = &table->speaker_headphone_ringtone;
        break;
    case HEADPHONE_VOIP_ROUTE:
        *config = &table->headphone_voip;
        break;
    case HEADSET_NORMAL_ROUTE:
        *config = &table->headset_normal;
        break;
    case HEADSET_INCALL_ROUTE:
        *config = &table->headset_incall;
        break;
    case HEADSET_RINGTONE_ROUTE:
        *config = &table->headset_ringtone;
        break;
    case HEADSET_VOIP_ROUTE:
        *config = &table->headset_voip;
        break;
    case BLUETOOTH_NORMAL_ROUTE:
        *config = &table->bluetooth_normal;
        break;
    case BLUETOOTH_INCALL_ROUTE:
        *config = &table->bluetooth_incall;
        break;
    case BLUETOOTH_VOIP_ROUTE:
        *config = &table->bluetooth_voip;
        break;
    case MAIN_MIC_CAPTURE_ROUTE:
        *config = &table->main_mic_capture;
        break;
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
        *config = &table->hands_free_mic_capture;
        break;
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
        *config = &table->bluetooth_sco_mic_capture;
        break;
    case PLAYBACK_OFF_ROUTE:
        *config = &table->playback_off;
        break;
    case CAPTURE_OFF_ROUTE:
        *config = &table->capture_off;
        break;
    case INCALL_OFF_ROUTE:
        *config = &table->incall_off;
        break;
    case VOIP_OFF_ROUTE:
        *config = &table->voip_off;
        break;
    case HDMI_NORMAL_ROUTE:
        *config = &table->hdmi_normal;
        break;
    case USB_NORMAL_ROUTE:
        *config = &table->usb_normal;
        break;
    case USB_CAPTURE_ROUTE:
        *config = &table->usb_capture;
        break;
    case SPDIF_NORMAL_ROUTE:
        *config = &table->spdif_normal;
        break;
    case HDMI_IN_NORMAL_ROUTE:
        *config = &table->hdmiin_normal;
        break;
    case HDMI_IN_OFF_ROUTE:
        *config = &table->hdmiin_off;
        break;
    case HDMI_IN_CAPTURE_ROUTE:
        *config = &table->hdmiin_captrue;
        break;
    case HDMI_IN_CAPTURE_OFF_ROUTE:
        *config = &table->hdmiin_captrue_off;
        break;
    default:
        ALOGE("%s: invalid route %d", __FUNCTION__, route);
        return -EINVAL;
    }

    return 0;
}

static int route_to_off(unsigned int route)
{
    switch (route) {
    case SPEAKER_NORMAL_ROUTE:
    case SPEAKER_RINGTONE_ROUTE:
    case EARPIECE_NORMAL_ROUTE:
    case EARPIECE_RINGTONE_ROUTE:
    case HEADPHONE_NORMAL_ROUTE:
    case HEADPHONE_RINGTONE_ROUTE:
    case SPEAKER_HEADPHONE_NORMAL_ROUTE:
    case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
    case HEADSET_NORMAL_ROUTE:
    case HEADSET_RINGTONE_ROUTE:
    case BLUETOOTH_NORMAL_ROUTE:
    case HDMI_NORMAL_ROUTE:
    case USB_NORMAL_ROUTE:
    case SPDIF_NORMAL_ROUTE:
        return PLAYBACK_OFF_ROUTE;

    case MAIN_MIC_CAPTURE_ROUTE:
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
    case USB_CAPTURE_ROUTE:
        return CAPTURE_OFF_ROUTE;

    case HDMI_IN_NORMAL_ROUTE:
        return HDMI_IN_OFF_ROUTE;

    case HDMI_IN_CAPTURE_ROUTE:
        return HDMI_IN_CAPTURE_OFF_ROUTE;

    case SPEAKER_INCALL_ROUTE:
    case EARPIECE_INCALL_ROUTE:
    case HEADPHONE_INCALL_ROUTE:
    case HEADSET_INCALL_ROUTE:
    case BLUETOOTH_INCALL_ROUTE:
        return INCALL_OFF_ROUTE;

    case SPEAKER_VOIP_ROUTE:
    case EARPIECE_VOIP_ROUTE:
    case HEADPHONE_VOIP_ROUTE:
    case HEADSET_VOIP_ROUTE:
    case BLUETOOTH_VOIP_ROUTE:
        return VOIP_OFF_ROUTE;

    default:
        ALOGE("%s: invalid route %d", __FUNCTION__, route);
        return MAX_ROUTE;
    }
}

int route_open(struct alsa_route **ar, int card)
{
    const struct config_route_table *table = NULL;
    struct mixer *mixer;
    struct alsa_route *tmp;
    int ret;

    if (card < 0) {
        ALOGE("%s: invalid card %d", __FUNCTION__, card);
        return -EINVAL;
    }

    ret = route_get_config_table(card, &table);
    if (ret) {
        ALOGE("%s: can not find configuration table for %d", __FUNCTION__, card);
        return ret;
    }

    mixer = mixer_open_legacy(card);
    if (!mixer) {
        return -EAGAIN;
    }

    tmp = calloc(1, sizeof(*tmp));
    if (!tmp) {
        mixer_close_legacy(mixer);
        return -ENOMEM;
    }

    tmp->table = table;
    tmp->mixer = mixer;
    tmp->applied = MAX_ROUTE;

    *ar = tmp;

    return 0;
}

void route_close(struct alsa_route *ar)
{

    if (ar) {
        mixer_close_legacy(ar->mixer);
        free(ar);
    }
}

int route_apply(struct alsa_route *ar, unsigned int route)
{
    const struct config_route *config;
    int ret;

    if (route >= MAX_ROUTE) {
        ALOGE("%s: route %d exceed maxinum %d", __FUNCTION__, route, MAX_ROUTE);
        return -EINVAL;
    }

    ret = route_get_config(route, ar->table, &config);
    if (ret) {
        ALOGE("%s: can not get configuration for route %d", __FUNCTION__, route);
        return ret;
    }

    ret = set_controls(ar->mixer, config->controls, config->controls_count);
    if (ret)
        return ret;

    ar->applied = route;

    return 0;
}

int route_reset(struct alsa_route *ar)
{
    const struct config_route *config;
    unsigned int route;
    int ret;

    /* NOTE: It has not been applied, there is no need to reset. */
    if (ar->applied >= MAX_ROUTE)
        goto out;

    route = route_to_off(ar->applied);
    if (route >= MAX_ROUTE) {
        ALOGE("%s: invalid applied route %d", __FUNCTION__, route);
        return -EAGAIN;
    }

    ret = route_get_config(route, ar->table, &config);
    if (ret) {
        ALOGE("%s: can not get configuration for route %d", __FUNCTION__, route);
        return ret;
    }

    ret = set_controls(ar->mixer, config->controls, config->controls_count);
    if (ret)
        return ret;

    ar->applied = MAX_ROUTE;

out:
    return 0;
}

int route_to_index(audio_devices_t device)
{
    switch ((int)device) {
    /* Input */
    case AUDIO_DEVICE_IN_BUILTIN_MIC:
        return MAIN_MIC_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
        return HANDS_FREE_MIC_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        return BLUETOOTH_SOC_MIC_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET:
        return USB_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_HDMI:
        return HDMI_IN_CAPTURE_ROUTE;
    /* Output */
    case AUDIO_DEVICE_OUT_SPEAKER:
        return SPEAKER_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return HEADSET_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return HEADPHONE_NORMAL_ROUTE;
    case (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADPHONE):
    case (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADSET):
        return SPEAKER_HEADPHONE_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return BLUETOOTH_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_AUX_DIGITAL:
#ifdef SUPPORT_MULTIAUDIO
    case VX_ROCKCHIP_OUT_HDMI0:
#endif
        return HDMI_NORMAL_ROUTE;
    default:
        return MAX_ROUTE;
    }
}

int route_to_incall(audio_devices_t device)
{
    switch ((int)device) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return SPEAKER_INCALL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return HEADSET_INCALL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return HEADPHONE_INCALL_ROUTE;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
    case AUDIO_DEVICE_OUT_ALL_SCO:
        return BLUETOOTH_INCALL_ROUTE;
    default:
        return MAX_ROUTE;
    }
}

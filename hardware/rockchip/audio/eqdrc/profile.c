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

#define LOG_TAG "eqdrc-profile"
// #define LOG_NDEBUG 0

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include "profile.h"

static void profile_dump_speaker(struct profile *profile)
{
    struct spk_params *speaker;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("SPEAKER:");
    for (i = 0; i < dumps; i++) {
        speaker = &profile->speakers[i];
        ALOGV("  Channel %d:", i);
        ALOGV("    pre_gain: %f", speaker->pre_gain);
    }
}

static void profile_dump_auto_gain(struct profile *profile)
{
    struct auto_gain_params *auto_gain;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("AUTO GAIN:");
    for (i = 0; i < dumps; i++) {
        auto_gain = &profile->auto_gains[i];
        ALOGV("  Channel %d:", i);
        ALOGV("    target_i_db: %f", auto_gain->target_i_db);
        ALOGV("    attack_time: %f", auto_gain->attack_time);
        ALOGV("    release_time: %f", auto_gain->release_time);
        ALOGV("    r128_wins: %f", auto_gain->r128_wins);
        ALOGV("    r128_steps: %f", auto_gain->r128_steps);
    }
}

static void profile_dump_bass_enhancer(struct profile *profile)
{
    struct bass_enhancer_params *bass_enhancer;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("BASS ENHANCER:");
    for (i = 0; i < dumps; i++) {
        bass_enhancer = &profile->bass_enhancers[i];
        ALOGV("  Channel %d:", i);
        ALOGV("    amount_db: %f", bass_enhancer->amount_db);
        ALOGV("    drive: %f", bass_enhancer->drive);
        ALOGV("    blend: %f", bass_enhancer->blend);
        ALOGV("    freq: %f", bass_enhancer->freq);
        ALOGV("    floor_active: %f", bass_enhancer->floor_active);
        ALOGV("    floor_freq: %f", bass_enhancer->floor_freq);
    }
}

static void profile_dump_exciter(struct profile *profile)
{
    struct exciter_params *exciter;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("EXCITER:");
    for (i = 0; i < dumps; i++) {
        exciter = &profile->exciters[i];
        ALOGV("  Channel %d:", i);
        ALOGV("    event_harmonics: %f", exciter->event_harmonics);
        ALOGV("    distortion_amount: %f", exciter->distortion_amount);
        ALOGV("    cutoff: %f", exciter->cutoff);
        ALOGV("    mix: %f", exciter->mix);
    }
}

static void profile_dump_deesser(struct profile *profile)
{
    struct deesser_params *deesser;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("DEESSER:");
    for (i = 0; i < dumps; i++) {
        deesser = &profile->deessers[i];
        ALOGV("  Channel %d:", i);
        ALOGV("    f0: %f", deesser->f0);
        ALOGV("    threshold: %f", deesser->threshold);
    }
}

static void profile_dump_eq(struct profile *profile)
{
    struct eq_band_params *band;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;
    int j;

    ALOGV("EQ:");
    for (i = 0; i < dumps; i++) {
        ALOGV("  Channel %d:", i);
        for (j = 0; j < EQ_MAX_BANDS; ++j) {
            band = &profile->eq[i].bands[j];
            ALOGV("    enabled: %d", !!band->enabled);
            switch ((int)band->filter) {
            case EQ_BAND_FILTER_PARAMETRIC:
                ALOGV("    filter: Parametric");
                break;
            case EQ_BAND_FILTER_LOW_SHELF:
                ALOGV("    filter: Low Shelf");
                break;
            case EQ_BAND_FILTER_HIGH_SHELF:
                ALOGV("    filter: High Shelf");
                break;
            case EQ_BAND_FILTER_GENERAL_LP:
            ALOGV("    filter: General LP");
                break;
            case EQ_BAND_FILTER_GENERAL_HP:
            ALOGV("    filter: General HP");
                break;
            default:
                break;
            }
            ALOGV("    fc: %f", band->fc);
            ALOGV("    q: %f", band->q);
            ALOGV("    boost: %f", band->boost);
        }
    }
}

static void profile_dump_mbdrc_band(struct mbdrc_freq_params *band)
{
    ALOGV("      freq_start: %f", band->freq_start);
    ALOGV("      freq_end: %f", band->freq_end);
    ALOGV("      gain_db: %f", band->gain_db);
    ALOGV("      drc_enable: %f", band->drc_enable);
    ALOGV("      compress_start: %f", band->compress_start);
    ALOGV("      expand_end: %f", band->expand_end);
    ALOGV("      noise_threshold: %f", band->noise_threshold);
    ALOGV("      max_gain: %f", band->max_gain);
    ALOGV("      max_peek: %f", band->max_peek);
    ALOGV("      attack_time: %f", band->attack_time);
    ALOGV("      release_time: %f", band->release_time);
    ALOGV("      hold_time: %f", band->hold_time);
}

static void profile_dump_mbdrc(struct profile *profile)
{
    struct mbdrc_params *mbdrc;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("MBDRC:");
    for (i = 0; i < dumps; i++) {
        mbdrc = &profile->mbdrcs[i];
        ALOGV("  Channel %d:", i);
        ALOGV("    cross band: %f", mbdrc->cross_band);
        ALOGV("    Low Frequency:");
        profile_dump_mbdrc_band(&mbdrc->low_freq);
        ALOGV("    Med Frequency 1:");
        profile_dump_mbdrc_band(&mbdrc->med_freq1);
        ALOGV("    Med Frequency 2:");
        profile_dump_mbdrc_band(&mbdrc->med_freq2);
        ALOGV("    High Frequency:");
        profile_dump_mbdrc_band(&mbdrc->high_freq);
    }
}

static void profile_dump_maximizer(struct profile *profile)
{
    struct maximizer_params *maximizer;
    struct agc_params *agc;
    int dumps = !!profile->link ? 1 : MAX_CHANNELS;
    int i;

    ALOGV("MAXIMIZER:");
    for (i = 0; i < dumps; i++) {
        if (i == 0) {
            maximizer = &profile->maximizer_ch1;
            agc = &profile->agc_ch1;
        } else {
            maximizer = &profile->maximizer_ch2;
            agc = &profile->agc_ch2;
        }
        ALOGV("  Channel %d:", i);
        ALOGV("    AGC:");
        ALOGV("      compress_start: %f", agc->compress_start);
        ALOGV("      expand_end: %f", agc->expand_end);
        ALOGV("      noise_threshold: %f", agc->noise_threshold);
        ALOGV("      max_gain: %f", agc->max_gain);
        ALOGV("      max_peek: %f", agc->max_peek);
        ALOGV("      attack_time: %f", agc->attack_time);
        ALOGV("      release_time: %f", agc->release_time);
        ALOGV("      hold_time: %f", agc->hold_time);
        ALOGV("    Maximizer:");
        ALOGV("      max_threshold: %f", maximizer->max_threshold);
        ALOGV("      ceiling: %f", maximizer->ceiling);
        ALOGV("      release: %f", maximizer->release);
    }
}

void profile_dump(struct profile *profile)
{
    if (profile) {
        ALOGV("Sampling rate: %f", profile->sampling_rate);
        ALOGV("Bitrate: %f", profile->bit_rate);
        ALOGV("Channels: %f", profile->channels);
        ALOGV("Link: %d", !!profile->link);
        ALOGV("Auto gain enabled: %d", !!profile->auto_gain_enable);
        ALOGV("Bass_enable: %d", !!profile->bass_enable);
        ALOGV("Exciter_enable: %d", !!profile->exciter_enable);
        ALOGV("Deesser_enable: %d", !!profile->deesser_enable);
        ALOGV("Eq10_enable: %d", !!profile->eq10_enable);
        ALOGV("Mbdrc_enable: %d", !!profile->mbdrc_enable);
        ALOGV("Agc_enable: %d", !!profile->agc_enable);
        ALOGV("Maximizer_enalbe: %d", !!profile->maximizer_enalbe);

        profile_dump_speaker(profile);
        profile_dump_auto_gain(profile);
        profile_dump_bass_enhancer(profile);
        profile_dump_exciter(profile);
        profile_dump_deesser(profile);
        profile_dump_eq(profile);
        profile_dump_mbdrc(profile);
        profile_dump_maximizer(profile);
    }
}

int profile_read(struct profile *profile, const char *name)
{
    FILE *file;
    int ret;

    ret = access(name, 0);
    if (ret) {
        ALOGE("%s: %s is not exist", __func__, name);
        return ret;
    }

    file = fopen(name, "rb");
    if (!file) {
        ALOGE("%s: failed to open %s", __func__, name);
        return -EINVAL;
    }

    ret = fread(profile, sizeof(float), sizeof(*profile) / sizeof(float), file);
    if (ret < 0) {
        ALOGE("%s: failed to read %s", __func__, name);
        fclose(file);
        return ret;
    }

    fclose(file);

    return 0;
}

int profile_write(struct profile *profile, const char *name)
{
    FILE *file;
    int ret;

    file = fopen(name, "wb+");
    if (!file) {
        ALOGE("%s: failed to open %s", __func__, name);
        return -EINVAL;
    }

    ret = fwrite(profile, sizeof(float), sizeof(*profile) / sizeof(float), file);
    if (ret < 0) {
        ALOGE("%s: failed to write %s", __func__, name);
        fclose(file);
        return ret;
    }

    fclose(file);

    return 0;
}

/* TODO: Load presets here, i.e. "classic", "metting" and "standard". */
int profile_load(struct profile *profile, unsigned int sampling_rate, unsigned int channels)
{
    char fname[PATH_MAX];
    int ret;

    snprintf(fname, sizeof(fname), PROFILE_PATH_SAVED_FMT, sampling_rate, channels);
    ret = profile_read(profile, fname);
    if (!ret)
        return 0;

    /* Fallback to default profile */
    snprintf(fname, sizeof(fname), PROFILE_PATH_FMT, sampling_rate, channels);
    ret = profile_read(profile, fname);
    if (ret)
        return ret;

    return 0;
}

int profile_save(struct profile *profile, unsigned int sampling_rate, unsigned int channels)
{
    char fname[PATH_MAX];
    int ret;

    snprintf(fname, sizeof(fname), PROFILE_PATH_SAVED_FMT, sampling_rate, channels);
    ret = profile_write(profile, fname);
    if (ret)
        return ret;

    return 0;
}

void profile_release(struct profile *profile)
{
    if (profile)
        memset(profile, 0, sizeof(*profile));
}

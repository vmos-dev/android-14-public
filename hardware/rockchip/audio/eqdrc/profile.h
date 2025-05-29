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

#ifndef EQDRC_PROFILE_H_
#define EQDRC_PROFILE_H_

#define PROFILE_PATH_PREFIX "/vendor/etc/rkaudio_effect_eqdrc_"
#define PROFILE_PATH_FMT "/vendor/etc/rkaudio_effect_eqdrc_%dhz_%dch.bin"
#define PROFILE_PATH_SAVED_FMT "/data/vendor/audio/rkaudio_effect_eqdrc_%dhz_%dch.bin"

#define MAX_CHANNELS                    2

/* Equalizer */
#define EQ_MAX_BANDS                    10

#define EQ_BAND_FILTER_PARAMETRIC       0
#define EQ_BAND_FILTER_LOW_SHELF        1
#define EQ_BAND_FILTER_HIGH_SHELF       2
#define EQ_BAND_FILTER_GENERAL_LP       3
#define EQ_BAND_FILTER_GENERAL_HP       4

/* SPK */
// 0x50
struct spk_params {
    float pre_gain;
    float reserved[4];
} __attribute__((__packed__));

/* Auto Gain */
// 0x78
struct auto_gain_params {
    float target_i_db;
    float attack_time;
    float release_time;
    float r128_wins;
    float r128_steps;
    float reserved[5];
} __attribute__((__packed__));

/* No Linear */
// 0xf0
struct bass_enhancer_params {
    float amount_db;
    float drive;
    float blend;
    float freq;
    float floor_active;
    float floor_freq;
    float reserved[4];
} __attribute__((__packed__));

// 0x140
struct exciter_params {
    float event_harmonics;
    float distortion_amount;
    float cutoff;
    float mix;
    float reserved[1];
} __attribute__((__packed__));

// 0x168
struct deesser_params {
    float f0;
    float threshold;
    float reserved[3];
} __attribute__((__packed__));

/* EQ */
// 0x190
struct eq_band_params {
    float enabled;
    float filter;
    float fc;
    float q;
    float boost;
} __attribute__((__packed__));

struct eq_params {
    struct eq_band_params bands[EQ_MAX_BANDS];
} __attribute__((__packed__));

/* MBDRC */
struct mbdrc_freq_params {
    float freq_start;
    float freq_end;
    float gain_db;
    float drc_enable;
    float compress_start;
    float expand_end;
    float noise_threshold;
    float max_gain;
    float max_peek;
    float attack_time;
    float release_time;
    float hold_time;
    float reserved[8];
} __attribute__((__packed__));

// 0x320
struct mbdrc_params {
    float cross_band;
    float reserved1[9];
    struct mbdrc_freq_params low_freq;
    struct mbdrc_freq_params med_freq1;
    struct mbdrc_freq_params med_freq2;
    struct mbdrc_freq_params high_freq;
    float reserved2[10];
} __attribute__((__packed__));

/* MAXIMIZER */
// 0x640
struct maximizer_params {
    float max_threshold;
    float ceiling;
    float release;
} __attribute__((__packed__));

struct agc_params {
    float compress_start;
    float expand_end;
    float noise_threshold;
    float max_gain;
    float max_peek;
    float attack_time;
    float release_time;
    float hold_time;
    float reserved[9];
} __attribute__((__packed__));

/*
 * NOTE: It's a binary profile, the maximun of our profile is 0x6e0 bytes,
 * but our util prefers 0x880 bytes and reserves 416 bytes for further use.
 */
struct profile {
    float sampling_rate;
    float bit_rate;
    float link;
    float channels;
    float reserved1[6];
    float auto_gain_enable;
    float bass_enable;
    float exciter_enable;
    float deesser_enable;
    float eq10_enable;
    float mbdrc_enable;
    float agc_enable;
    float maximizer_enalbe;
    float reserved2[2];
    struct spk_params speakers[MAX_CHANNELS];
    struct auto_gain_params auto_gains[MAX_CHANNELS];
    float reserved3[10];
    struct bass_enhancer_params bass_enhancers[MAX_CHANNELS];
    struct exciter_params exciters[MAX_CHANNELS];
    struct deesser_params deessers[MAX_CHANNELS];
    struct eq_params eq[MAX_CHANNELS];
    struct mbdrc_params mbdrcs[MAX_CHANNELS];
    struct maximizer_params maximizer_ch1;
    struct agc_params agc_ch1;
    struct maximizer_params maximizer_ch2;
    struct agc_params agc_ch2;
    float reserved4[104];
} __attribute__((__packed__));

void profile_dump(struct profile *profile);
int profile_read(struct profile *profile, const char *name);
int profile_load(struct profile *profile, unsigned int sampling_rate, unsigned int channels);
int profile_save(struct profile *profile, unsigned int sampling_rate, unsigned int channels);
void profile_release(struct profile *profile);

#endif /* EQDRC_PROFILE_H_ */

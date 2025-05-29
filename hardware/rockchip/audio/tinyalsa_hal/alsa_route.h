/*
** Copyright 2023, Rockchip Electronics Co. Ltd. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/**
 * @file alsa_route.h
 * @brief
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#ifndef _ALSA_ROUTE_H_
#define _ALSA_ROUTE_H_

#include <system/audio.h>

struct alsa_route {
    const struct config_route_table *table;
    struct mixer *mixer;
    unsigned applied;
};

int route_init(void);
void route_uninit(void);
int route_set_input_source(const char *source);
int route_set_voice_volume(const char *ctlName, float volume);
int route_set_controls(unsigned route);
void route_pcm_open(unsigned route);
void route_pcm_card_open(int card, unsigned route);
int route_pcm_close(unsigned route);

int route_open(struct alsa_route **ar, int card);
void route_close(struct alsa_route *ar);
int route_apply(struct alsa_route *ar, unsigned int route);
int route_reset(struct alsa_route *ar);
int route_to_index(audio_devices_t device);
int route_to_incall(audio_devices_t device);

#endif

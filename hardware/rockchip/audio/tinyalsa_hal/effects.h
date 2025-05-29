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

#include <hardware/audio_effect.h>
#include <cutils/list.h>

struct effect_list {
    struct listnode list;
    effect_handle_t handle;
    effect_descriptor_t desc;
};

int audio_effect_count(struct listnode *effects);
void audio_effect_add(struct listnode *effects, effect_handle_t effect);
void audio_effect_remove(struct listnode *effects, effect_handle_t effect);
void audio_effect_process(struct listnode *effects, void *in, void *out, size_t frames);
void audio_effect_process_reverse(struct listnode *effects, void *in, void *out, size_t frames);
void audio_effect_set_config(struct listnode *effects, effect_config_t *config);
void audio_effect_set_config_reverse(struct listnode *effects, effect_config_t *config);

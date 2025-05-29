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

#ifndef PREPROCESS_ALGO_H_
#define PREPROCESS_ALGO_H_

#include "profile.h"

struct algo {
    void *handle;
    FILE *fd_input;
    FILE *fd_output;
    unsigned int channels;
    int channels_src;
    int channels_ref;
    int16_t *buffer_processed;
    int16_t *buffer_merged;
};

int algo_init(struct algo *algo, struct profile *profile, uint32_t enabled_mask);
int algo_process(struct algo *algo, audio_buffer_t *input, audio_buffer_t *output);
int algo_process_merged(struct algo *algo, audio_buffer_t *input, audio_buffer_t *output,
                        audio_buffer_t *reference);
void algo_release(struct algo *algo);

#endif /* PREPROCESS_ALGO_H_ */

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

#ifndef EQDRC_ALGO_H_
#define EQDRC_ALGO_H_

#include "profile.h"

struct algo {
    void *handle;
    unsigned int channels;
    unsigned int frames;
    pthread_mutex_t lock;
};

int algo_init(struct algo *algo, struct profile *profile);
int algo_process(struct algo *algo, audio_buffer_t *input, audio_buffer_t *output);
void algo_release(struct algo *algo);
int algo_set_params(struct algo *algo, struct profile *profile);
bool algo_initialized(struct algo *algo);

#endif /* EQDRC_ALGO_H_ */

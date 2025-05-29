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

#ifndef PREPROCESS_PROFILE_H_
#define PREPROCESS_PROFILE_H_

struct profile {
    RKAUDIOParam param;
    int rate;
    int frames;
    int channels;
    int channels_src;
    int channels_ref;
};

int profile_init(struct profile *profile, int rate, int frames, int channels_src, int channels_ref,
                 uint32_t enabled_mask);
void profile_release(struct profile *profile);
void profile_dump(struct profile *profile);

#endif /* PREPROCESS_PROFILE_H_ */
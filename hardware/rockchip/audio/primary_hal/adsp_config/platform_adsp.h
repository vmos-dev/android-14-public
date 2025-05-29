/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef _PLATFORM_ADSP_H_
#define _PLATFORM_ADSP_H_

int platform_adsp_set_volume(struct audio_device *adev, const char *addr, int32_t level);
int platform_adsp_init(struct audio_device *adev);
int platform_adsp_free(struct audio_device *adev);

#endif //_PLATFORM_ADSP_H_
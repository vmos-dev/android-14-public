/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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

/**
 * @file    channel.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#ifndef UTILS_CHANNEL_H
#define UTILS_CHANNEL_H

#include <stdint.h>

#define CHR_VALID (1 << 1)
#define CHL_VALID (1 << 0)
#define CH_CHECK (1 << 2)

void channel_fixed(void *data, unsigned len, uint32_t flag);
uint32_t channel_check(void *data, unsigned int len);

#endif

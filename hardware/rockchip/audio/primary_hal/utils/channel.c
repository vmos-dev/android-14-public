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
 * @file    channel.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#include "channel.h"

void channel_fixed(void *data, unsigned len, uint32_t flag)
{
    short *ch0, *ch1, *src, *dest;
    unsigned int i;

    if (((flag & (CHL_VALID | CHR_VALID)) == 0) ||
        ((flag & (CHL_VALID | CHR_VALID)) == (CHL_VALID | CHR_VALID)))
        return;

    ch0 = (short *)data;
    ch1 = ch0 + 1;
    src = ch0;
    dest = ch0;

    if (flag & CHL_VALID)
        dest = ch1;
    else if (flag & CHR_VALID)
        src = ch1;

    for (i = 0; i < len; i += 2)
        dest[i] = src[i];
}

uint32_t channel_check(void *data, unsigned int len)
{
    short *left = (short *)data;
    short *right = left + 1;
    int left_valid = 0x0;
    int right_valid = 0x0;
    short valuel = 0;
    short valuer = 0;
    uint32_t flag = 0;
    unsigned int i = 0;

    valuel = *left;
    valuer = *right;
    for (i = 0; i < len; i += 2) {
        if ((left[i] >= valuel + 50) || (left[i] <= valuel - 50))
            left_valid++;

        if ((right[i] >= valuer + 50) || (right[i] <= valuer - 50))
            right_valid++;
    }

    if (left_valid > 20)
        flag |= CHL_VALID;

    if (right_valid > 20)
        flag |= CHR_VALID;

    return flag;
}

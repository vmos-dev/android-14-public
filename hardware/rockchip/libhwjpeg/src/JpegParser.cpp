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
 *
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "JpegParser"
#include <utils/Log.h>

#include "JpegParser.h"
#include "BitReader.h"

bool jpeg_parser_skip_section(BitReader *br) {
    uint32_t len = 0;

    if (br->numBitsLeft() < 2)
        return false;

    len = br->getBits(16);
    if (len < 2 /* invalid marker */ || (uint32_t)len - 2 > br->numBitsLeft()) {
        /* too short length or bytes is not enough */
        return false;
    }

    if (len > 2)
        br->skipBits((len - 2) * 8);

    return true;
}

/* return the 8 bit start code value and update the search
   state. Return -1 if no start code found */
int32_t jpeg_parser_find_marker(const uint8_t **pbufPtr, const uint8_t *bufEnd) {
    const uint8_t *bufPtr;
    unsigned int v, v2;
    int val;
    int skipped = 0;

    bufPtr = *pbufPtr;
    while (bufEnd - bufPtr > 1) {
        v  = *bufPtr++;
        v2 = *bufPtr;
        if ((v == 0xff) && (v2 >= 0xc0) && (v2 <= 0xfe) && bufPtr < bufEnd) {
            val = *bufPtr++;
            goto found;
        } else if ((v == 0x89) && (v2 == 0x50)) {
            ALOGV("input img maybe png format,check it");
        }
        skipped++;
    }
    bufPtr = bufEnd;
    val = -1;

found:
    ALOGV("find_marker skipped %d bytes", skipped);
    *pbufPtr = bufPtr;
    return val;
}

bool jpeg_parser_decode_dht(BitReader *br) {
    uint32_t len;

    len = br->getBits(16);
    len -= 2; /* quantize tables length */

    if (len > br->numBitsLeft()) {
        ALOGE("dht: len %d is too large", len);
        return false;
    }

    while (len > 0) {
        if (len < 16 + 1) {
            ALOGE("dht: len %d is too small", len);
            return false;
        }

        uint32_t tableType, tableId;
        uint32_t i, num;

        tableType = br->getBits(4); /* 0 - DC; 1 - AC */
        if (tableType >= 2) {
            ALOGE("table type %d error", tableType);
            return false;
        }

        tableId = br->getBits(4);
        if (tableId >= 2) {
            ALOGE("table id %d is unsupported for baseline", tableId);
            return false;
        }

        num = 0;
        for (i = 0; i < 16; i++) {
            num += br->getBits(8);
        }

        len -= 17;
        for (i = 0; i < num; i++) {
            br->skipBits(8);
        }

        len -= num;
    }
    return true;
}

/* quantize tables */
bool jpeg_parser_decode_dqt(BitReader *br) {
    uint32_t len;

    len = br->getBits(16);
    len -= 2; /* quantize tables length */

    if (len > br->numBitsLeft()) {
        ALOGE("dqt: len %d is too large", len);
        return false;
    }

    while (len >= 65) {
        uint32_t pr, index;

        pr = br->getBits(4);
        if (pr > 1) {
            ALOGE("dqt: invalid precision");
            return false;
        }

        index = br->getBits(4);
        if (index >= 4) {
            ALOGE("dqt: invalid quantize tables ID");
            return false;
        }

        ALOGV("quantize tables ID=%d", index);

        /* read quant table */
        for (int32_t i = 0; i < 64; i++) {
            br->skipBits(pr ? 16 : 8);
        }

        len -= 1 + 64 * (1 + pr);
    }
    return true;
}

bool jpeg_parser_decode_dri(BitReader *br) {
    uint32_t len;

    len = br->getBits(16);
    if (len != 4) {
        ALOGE("DRI length %d error", len);
        return false;
    }

    br->skipBits(16);

    return true;
}

bool jpeg_parser_decode_sof(BitReader *br, int32_t *outWidth, int32_t *outHeight) {
    uint32_t len, bits;
    int32_t width, height, nbComponents;

    len = br->getBits(16);
    if (len > br->numBitsLeft()) {
        ALOGE("sof0: len %d is too large", len);
        return false;
    }

    bits = br->getBits(8);
    if (bits > 16 || bits < 1) {
        /* usually bits is 8 */
        ALOGE("sof0: bits %d is invalid", bits);
        return false;
    }

    height = br->getBits(16);
    width = br->getBits(16);

    ALOGV("sof0: picture: %dx%d", width, height);

    nbComponents = br->getBits(8);
    if ((nbComponents != 1) && (nbComponents != MAX_COMPONENTS)) {
        ALOGE("sof0: components number %d error", nbComponents);
        return false;
    }

    if (len != (8 + (3 * nbComponents)))
        ALOGE("sof0: error, len(%d) mismatch nb_components(%d)",
              len, nbComponents);

    *outWidth = width;
    *outHeight = height;

    return true;
}

bool jpeg_parser_get_dimens(
        char *data, size_t size, int32_t *outWidth, int32_t *outHeight) {
    const uint8_t *bufEnd, *bufPtr;
    uint8_t *ubuf = (uint8_t*)data;

    bufPtr = ubuf;
    bufEnd = ubuf + size;

    if (size < 4 || *bufPtr != 0xFF || *(bufPtr + 1) != SOI) {
        // not jpeg
        return false;
    }

    while (bufPtr < bufEnd) {
        int sectionFinish = 1;
        /* find start marker */
        int32_t startCode = jpeg_parser_find_marker(&bufPtr, bufEnd);
        if (startCode < 0) {
            ALOGV("start code not found");
        }

        ALOGV("marker = 0x%x, avail_size_in_buf = %d\n",
              startCode, (int)(bufEnd - bufPtr));

        /* setup bit read context */
        BitReader br(bufPtr, bufEnd - bufPtr);

        switch (startCode) {
        case SOI:
            /* nothing to do on SOI */
            break;
        case SOF0:
            return jpeg_parser_decode_sof(&br, outWidth, outHeight);
        case DHT:
            if (!jpeg_parser_decode_dht(&br)) {
                ALOGE("huffman table decode error");
                return false;
            }
            break;
        case DQT:
            if (!jpeg_parser_decode_dqt(&br)) {
                ALOGE("quantize tables decode error");
                return false;
            }
            break;
        case COM:
        case EOI:
        case SOS:
        case DRI:
            if (!jpeg_parser_decode_dri(&br)) {
                ALOGE("dri decode error");
                return false;
            }
            break;
        case SOF2:
        case SOF3:
        case SOF5:
        case SOF6:
        case SOF7:
        case SOF9:
        case SOF10:
        case SOF11:
        case SOF13:
        case SOF14:
        case SOF15:
        case SOF48:
        case LSE:
        case JPG:
            sectionFinish = 0;
            ALOGD("jpeg: unsupported coding type (0x%x)", startCode);
            break;
        default:
            sectionFinish = 0;
            ALOGV("unsupported coding type 0x%x switch.", startCode);
            break;
        }

        if (!sectionFinish) {
            if (!jpeg_parser_skip_section(&br)) {
                ALOGV("Fail to skip section 0xFF%02x!", startCode);
                return false;
            }
        }

        bufPtr = br.data();
    }

    return true;
}

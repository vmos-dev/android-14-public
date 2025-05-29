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

#ifndef __MPI_JPEG_ENCODER_H__
#define __MPI_JPEG_ENCODER_H__

#include "rk_mpi.h"

class QList;

class MpiJpegEncoder {
public:
    MpiJpegEncoder();
    ~MpiJpegEncoder();

    /* All supported input formats */
    typedef enum {
        INPUT_FMT_YUV420SP     = MPP_FMT_YUV420SP,
        INPUT_FMT_YUV420P      = MPP_FMT_YUV420P,
        INPUT_FMT_YUV422SP_VU  = MPP_FMT_YUV422SP_VU,
        INPUT_FMT_YUV422_YUYV  = MPP_FMT_YUV422_YUYV,
        INPUT_FMT_YUV422_UYVY  = MPP_FMT_YUV422_UYVY,
        INPUT_FMT_ARGB8888     = MPP_FMT_ARGB8888,
        INPUT_FMT_RGBA8888     = MPP_FMT_RGBA8888,
        INPUT_FMT_ABGR8888     = MPP_FMT_ABGR8888
    } InputFormat;

    typedef struct {
        /* Output parameters */
        void     *data;
        int32_t   size;
        /* packet handle, used to release cache buffer */
        void     *handle;
    } OutputPacket_t;

    typedef struct {
        /* input buffer information */
        int32_t     inputPhyAddr;
        uint8_t    *inputVirAddr;
        int32_t     width;
        int32_t     height;
        InputFormat format;

        /* coding quality - range from (1 - 10) */
        int32_t     qLvl;

        /* insert thumbnail or not */
        int32_t     doThumbNail;
        int32_t     thumbWidth;
        int32_t     thumbHeight;
        int32_t     thumbQLvl;

        void        *exifInfo;
        void        *gpsInfo;
    } EncInInfo;

    typedef struct {
        /* Input parameters, User-specified output buffer address */
        int32_t  outputPhyAddr;
        uint8_t *outputVirAddr;
        int32_t  outBufLen;
    } EncOutInfo;

    bool prepareEncoder();

    bool flushBuffer();

    bool updateEncodeCfg(
            int32_t width, int32_t height,
            InputFormat format = INPUT_FMT_YUV420SP, int32_t qLvl = 80,
            int32_t hstride = 0, int32_t vstride = 0);

    /*
     * output buffer count within limits, so release packet buffer if the
     * frame has been display successfully.
     */
    bool deinitOutputPacket(OutputPacket_t *packetOut);

    bool encodeFrame(char *data, OutputPacket_t *packetOut);
    bool encodeFile(const char *inputFile, const char *outputFile);

    /*
     * designed for Rockchip cameraHal, using user-specified in/output fd
     *
     * param[in]     inInfo  - pointer to input buffer parameters for encoder
     * param[in/out] outInfo - pointer to output buffer parameters for encoder
     */
    bool encode(EncInInfo *inInfo, EncOutInfo *outInfo);

private:
    typedef struct {
        int32_t  fd;
        int32_t  size;
        uint8_t *exifData;
        int32_t  exifSize;
    } OutBuffer_t;

    MppCtx  mMppCtx;
    MppApi *mMppMpi;
    MppBufferGroup mMemGroup;

    bool    mStateReady;
    int32_t mFrameCount;

    int32_t mWidth;
    int32_t mHeight;
    int32_t mHorStride;
    int32_t mVerStride;
    /* coding quality - range from (10 ~ 100) */
    int32_t mQuality;

    QList  *mPackets;

    InputFormat mInputFmt;

    /* global jpeg encoder warpper */
    bool runFrameEnc(MppFrame inFrame, MppPacket &outPacket);

    /* crop and get thumbnail image */
    bool getThumbImage(EncInInfo *inInfo, int32_t outputFd);

    /* encode thumbnail image */
    bool encodeThumbImage(EncInInfo *inInfo, OutputPacket_t *packetOut);

    /* encode big image using user-specified fd */
    bool encodeBigImage(
            EncInInfo *inInfo, OutBuffer_t *outBuffer, OutputPacket_t *packetOut);
};

#endif  // __MPI_JPEG_ENCODER_H__

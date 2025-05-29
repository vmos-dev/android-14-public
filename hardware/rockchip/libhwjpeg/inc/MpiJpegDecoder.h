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

#ifndef __MPI_JPEG_DECODER_H__
#define __MPI_JPEG_DECODER_H__

#include "rk_mpi.h"

class QList;

class MpiJpegDecoder {
public:
    MpiJpegDecoder();
    ~MpiJpegDecoder();

    /* All supported output formats */
    typedef enum {
        OUT_FORMAT_YUV420SP =  MPP_FMT_YUV420SP,
        OUT_FORMAT_ARGB     =  MPP_FMT_ARGB8888,
    } OutputFormat;

    typedef struct {
        /* Input parameters, User-specified output buffer address */
        int32_t outputPhyAddr;

        /* Output parameters */
        int32_t FrameWidth;        // buffer horizontal stride
        int32_t FrameHeight;       // buffer vertical stride
        int32_t DisplayWidth;      // valid width for display
        int32_t DisplayHeight;     // valid height for display
        int32_t ErrorInfo;         // error information
        int32_t OutputSize;

        char   *MemVirAddr;        // output buffer address
        int32_t MemPhyAddr;

        void   *Handle;            // frame handle, used to release cache buffer
    } OutputFrame_t;

    bool prepareDecoder();

    bool flushBuffer();

    /*
     * output buffer count within limits, so release frame buffer if the
     * frame has been display successfully.
     */
    bool deinitOutputFrame(OutputFrame_t *frameOut);

    /* Asynchronous decoding interface */
    bool sendpacket(char* data, size_t size, int32_t outputFd = 0);
    bool getoutframe(OutputFrame_t *frameOut);

    /* Synchronous decoding interface */
    bool decodePacket(char* data, size_t size, OutputFrame_t *frameOut);
    bool decodeFile(const char *inputFile, const char *outputFile);

private:
    MppCtx   mMppCtx;
    MppApi  *mMppMpi;

    int32_t  mWidth;
    int32_t  mHeight;
    bool     mStateReady;
    bool     mOutputCrop;
    int32_t  mOutputFmt;
    int32_t  mPacketCount;
    int32_t  mFrameCount;

    QList   *mFrames;

    MppBufferGroup mMemGroup;

    bool reinitDecoder();

    bool setupOutFrameFromMppFrame(OutputFrame_t *frameOut, MppFrame frame);
    bool cropOutputFrameIfNeccessary(OutputFrame_t *frameOut);
};

#endif  // __MPI_JPEG_DECODER_H__

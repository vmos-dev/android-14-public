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
#define LOG_TAG "MpiJpegDecoder"
#include <utils/Log.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <cutils/properties.h>

#include "QList.h"
#include "JpegParser.h"
#include "Utils.h"
#include "Version.h"
#include "MpiDebug.h"
#include "MpiJpegDecoder.h"

static int32_t gDecDebug = 0;

typedef struct {
    struct timeval start;
    struct timeval end;
} DebugTimeInfo;

static DebugTimeInfo gTimeInfo;

static void time_start_record() {
    if (gDecDebug & DEBUG_TIMING) {
        gettimeofday(&gTimeInfo.start, NULL);
    }
}

static void time_end_record(const char *task) {
    if (gDecDebug & DEBUG_TIMING) {
        gettimeofday(&gTimeInfo.end, NULL);
        ALOGD("%s consumes %ld ms", task,
              (gTimeInfo.end.tv_sec  - gTimeInfo.start.tv_sec)  * 1000 +
              (gTimeInfo.end.tv_usec - gTimeInfo.start.tv_usec) / 1000);
    }
}

MpiJpegDecoder::MpiJpegDecoder() :
    mMppCtx(NULL),
    mMppMpi(NULL),
    mWidth(0),
    mHeight(0),
    mStateReady(false),
    mOutputCrop(false),
    mPacketCount(0),
    mFrameCount(0),
    mFrames(NULL),
    mMemGroup(NULL) {
    ALOGI("libhwjpeg version %s", HWJPEG_FULL_VERSION);

    // set yuv420sp default
    mOutputFmt = OUT_FORMAT_YUV420SP;

    // keep DDR performance for usb camera preview mode
    CommonUtil::setPerformanceMode(1);

    gDecDebug = property_get_int32("hwjpeg_dec_debug", 0);
    if (gDecDebug & DEBUG_OUTPUT_CROP) {
        ALOGD("decoder will crop output.");
        mOutputCrop = true;
    }
}

MpiJpegDecoder::~MpiJpegDecoder() {
    CommonUtil::setPerformanceMode(0);

    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }

    if (mFrames) {
        delete mFrames;
        mFrames = NULL;
    }
    if (mMemGroup) {
        mpp_buffer_group_put(mMemGroup);
        mMemGroup = NULL;
    }
}

bool MpiJpegDecoder::reinitDecoder() {
    MPP_RET err = MPP_OK;
    MppParam param = NULL;
    // non-block call
    MppPollType timeout = MPP_POLL_NON_BLOCK;

    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }

    err = mpp_create(&mMppCtx, &mMppMpi);
    if (err != MPP_OK) {
        ALOGE("failed to create mpp context");
        goto error;
    }

    // NOTE: timeout value please refer to MppPollType definition
    //  0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    if (timeout) {
        param = &timeout;
        err = mMppMpi->control(mMppCtx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (err != MPP_OK) {
            ALOGE("failed to set output timeout %d err %d", timeout, err);
            goto error;
        }
    }

    err = mpp_init(mMppCtx, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);
    if (err != MPP_OK) {
        ALOGE("failed to init mpp");
        goto error;
    }

    /* NOTE: change output format before jpeg decoding */
    if (mOutputFmt < MPP_FMT_BUTT) {
        err = mMppMpi->control(mMppCtx, MPP_DEC_SET_OUTPUT_FORMAT, &mOutputFmt);
        if (err != MPP_OK) {
            ALOGE("failed to set output format %d err %d", mOutputFmt, err);
        }
    }

    return true;

error:
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }
    return false;
}

bool MpiJpegDecoder::prepareDecoder() {
    if (mStateReady) {
        return true;
    }

    if (!reinitDecoder()) {
        ALOGE("failed to init mpp decoder");
        return false;
    }

    mFrames = new QList((node_destructor)mpp_frame_deinit);

    /* memery buffer group */
    mpp_buffer_group_get_internal(&mMemGroup, MPP_BUFFER_TYPE_ION);
    mpp_buffer_group_limit_config(mMemGroup, 0, 20);

    mStateReady = true;

    return true;
}

bool MpiJpegDecoder::flushBuffer() {
    if (mStateReady) {
        mFrames->lock();
        mFrames->flush();
        mFrames->unlock();

        mMppMpi->reset(mMppCtx);
    }
    return true;
}

bool MpiJpegDecoder::setupOutFrameFromMppFrame(OutputFrame_t *frameOut, MppFrame frame) {
    MppBuffer frmBuf = mpp_frame_get_buffer(frame);

    memset(frameOut, 0, sizeof(frameOut));

    frameOut->DisplayWidth  = mpp_frame_get_width(frame);
    frameOut->DisplayHeight = mpp_frame_get_height(frame);
    frameOut->FrameWidth    = mpp_frame_get_hor_stride(frame);
    frameOut->FrameHeight   = mpp_frame_get_ver_stride(frame);
    frameOut->ErrorInfo     = mpp_frame_get_errinfo(frame) |
                              mpp_frame_get_discard(frame);
    frameOut->Handle        = frame;

    ALOGV("get out frame w %d h %d hor %d ver %d err %d",
          frameOut->DisplayWidth, frameOut->DisplayHeight,
          frameOut->FrameWidth, frameOut->FrameHeight, frameOut->ErrorInfo);

    if (frmBuf) {
        int32_t fd = mpp_buffer_get_fd(frmBuf);
        void  *ptr = mpp_buffer_get_ptr(frmBuf);

        frameOut->MemVirAddr = (char*)ptr;
        frameOut->MemPhyAddr = fd;

        switch (mOutputFmt) {
        case OUT_FORMAT_ARGB: {
            frameOut->OutputSize = frameOut->FrameWidth * frameOut->FrameHeight * 4;
        } break;
        case OUT_FORMAT_YUV420SP: {
            frameOut->OutputSize = frameOut->FrameWidth * frameOut->FrameHeight * 3 / 2;
        } break;
        default: {
            frameOut->OutputSize = frameOut->FrameWidth * frameOut->FrameHeight * 3 / 2;
        } break;
        }
    }

    return true;
}

bool MpiJpegDecoder::cropOutputFrameIfNeccessary(OutputFrame_t *frameOut) {
    if (!mOutputCrop) {
        return true;
    }

    if (frameOut->DisplayWidth == frameOut->FrameWidth &&
        frameOut->DisplayHeight == frameOut->FrameHeight) {
        // NO NEED
        return true;
    }

    int32_t srcAddr    = frameOut->MemPhyAddr;
    int32_t dstAddr    = frameOut->MemPhyAddr;
    int32_t srcStride  = frameOut->FrameWidth;
    int32_t srcVStride = frameOut->FrameHeight;
    int32_t srcWidth   = frameOut->DisplayWidth;
    int32_t srcHeight  = frameOut->DisplayHeight;
    int32_t dstWidth   = ALIGN(srcWidth, 8);
    int32_t dstHeight  = ALIGN(srcHeight, 8);

    ALOGV("librga: try crop from [%d, %d] -> [%d %d]",
          srcStride, srcVStride, dstWidth, dstHeight);

    bool ret = CommonUtil::cropImage(
                srcAddr, dstAddr, srcWidth, srcHeight,
                srcStride, srcVStride, dstWidth, dstHeight);
    if (ret) {
        frameOut->DisplayWidth  = dstWidth;
        frameOut->DisplayHeight = dstHeight;
        frameOut->FrameWidth    = dstWidth;
        frameOut->FrameHeight   = dstHeight;
        frameOut->OutputSize    = dstWidth * dstHeight * 3 / 2;;
    }

    return ret;
}

bool MpiJpegDecoder::sendpacket(char *data, size_t size, int32_t outputFd) {
    MPP_RET   err         = MPP_OK;
    /* input packet and output frame */
    MppPacket inPacket    = NULL;
    MppBuffer inPacketBuf = NULL;
    MppFrame  outFrame    = NULL;
    MppBuffer outFrameBuf = NULL;
    MppTask   task        = NULL;

    if (!mStateReady || !data) {
        return false;
    }

    /* dump input data if neccessary */
    if ((gDecDebug & DEBUG_RECORD_IN) && (mPacketCount % 10 == 0)) {
        char fileName[60];
        sprintf(fileName, "/data/video/dec_input_%d.jpg", mPacketCount);

        if (CommonUtil::dumpDataToFile(data, size, fileName)) {
            ALOGD("dump input jpeg to %s", fileName);
        }
    }

    int32_t width  = 0;
    int32_t height = 0;
    int32_t outBufferSize = 0;
    bool outputImport = false;

    // NOTE: the size of output frame depends on input JPEG dimens,
    // so get JPEG dimens from header first.
    if (!jpeg_parser_get_dimens(data, size, &width, &height)) {
        ALOGE("failed to get input jpeg dimens");
        return false;
    }

    /* reinit decoder when get dimensions info-change */
    if (width != mWidth || height != mHeight) {
        if (mWidth != 0 && mHeight != 0) {
            ALOGD("info-change with old dimensions(%dx%d)", mWidth, mHeight);
            ALOGD("info-change with new dimensions(%dx%d)", width, height);
            reinitDecoder();
        }

        mWidth = width;
        mHeight = height;
        ALOGD("get frame info: w %d h %d", width, height);
    }

    err = mpp_buffer_get(mMemGroup, &inPacketBuf, size);
    if (err != MPP_OK) {
        ALOGE("failed to get input buffer, err %d", err);
        goto cleanUp;
    }

    mpp_packet_init_with_buffer(&inPacket, inPacketBuf);
    mpp_buffer_write(inPacketBuf, 0, data, size);
    mpp_packet_set_length(inPacket, size);

    switch (mOutputFmt) {
    case OUT_FORMAT_ARGB: {
        outBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 4;
    } break;
    case OUT_FORMAT_YUV420SP: {
        outBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 2;
    } break;
    default: {
        outBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 2;
    } break;
    }

    if (outputFd > 0) {
        outputImport = CommonUtil::isValidDmaFd(outputFd);
        if (!outputImport) {
            ALOGW("fd(%d) not a valid dma buffer, update to use internal"
                  "output buffer group", outputFd);
        }
    }

    if (outputImport) {
        /* import output fd  */
        MppBufferInfo outputCommit;

        memset(&outputCommit, 0, sizeof(outputCommit));
        outputCommit.type = MPP_BUFFER_TYPE_ION;
        outputCommit.fd = outputFd;
        outputCommit.size = outBufferSize;

        err = mpp_buffer_import(&outFrameBuf, &outputCommit);
        if (err != MPP_OK) {
            ALOGE("failed to import output buffer, err %d", err);
            goto cleanUp;
        }
    } else {
        /* use internal output buffer group */
        err = mpp_buffer_get(mMemGroup, &outFrameBuf, outBufferSize);
        if (err != MPP_OK) {
            ALOGE("failed to get buffer for output frame err %d", err);
            goto cleanUp;
        }
    }

    mpp_frame_init(&outFrame);
    mpp_frame_set_buffer(outFrame, outFrameBuf);

    /* start queue input task */
    err = mMppMpi->poll(mMppCtx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (err != MPP_OK) {
        ALOGE("failed to poll input task");
        goto cleanUp;
    }

    /* input queue */
    err = mMppMpi->dequeue(mMppCtx, MPP_PORT_INPUT, &task);
    if (err != MPP_OK) {
        ALOGE("failed dequeue to input task ");
        goto cleanUp;
    }

    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, inPacket);
    mpp_task_meta_set_frame(task, KEY_OUTPUT_FRAME, outFrame);

    err = mMppMpi->enqueue(mMppCtx, MPP_PORT_INPUT, task);
    if (err != MPP_OK) {
        ALOGE("failed to enqueue input_task");
        goto cleanUp;
    }

    mPacketCount++;

cleanUp:
    if (outFrameBuf) {
        mpp_buffer_put(outFrameBuf);
        outFrameBuf = NULL;
    }

    if (inPacket) {
        mpp_packet_deinit(&inPacket);
        inPacket = NULL;
    }

    if (err != MPP_OK) {
        if (inPacketBuf) {
            mpp_buffer_put(inPacketBuf);
            inPacketBuf = NULL;
        }

        if (outFrame) {
            mpp_frame_deinit(&outFrame);
            outFrame = NULL;
        }
    }

    return !err;
}

bool MpiJpegDecoder::getoutframe(OutputFrame_t *frameOut) {
    bool     ret   = true;
    MPP_RET  err   = MPP_OK;
    MppTask  task  = NULL;
    MppFrame frame = NULL;

    if (!mStateReady)
        return false;

    /* poll and wait here */
    err = mMppMpi->poll(mMppCtx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (err != MPP_OK) {
        ALOGE("failed to poll output task");
        ret = false;
        goto cleanUp;
    }

    /* output queue */
    err = mMppMpi->dequeue(mMppCtx, MPP_PORT_OUTPUT, &task);
    if (err != MPP_OK || !task) {
        ALOGE("failed to dequeue output task");
        ret = false;
        goto cleanUp;
    }

    mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frame);

    /* setup output handle OutputFrame_t from MppFrame */
    setupOutFrameFromMppFrame(frameOut, frame);

    /* output from decoder is aligned by 16, so crop it before display */
    cropOutputFrameIfNeccessary(frameOut);

    /* dump output buffer if neccessary */
    if ((gDecDebug & DEBUG_RECORD_OUT) && mFrameCount % 10 == 0) {
        char fileName[60];
        sprintf(fileName, "/data/video/dec_output_%dx%d_%d.yuv",
                frameOut->FrameWidth, frameOut->FrameHeight, mFrameCount);

        if (CommonUtil::dumpDmaFdToFile(
                frameOut->MemPhyAddr, frameOut->OutputSize, fileName)) {
            ALOGD("dump output yuv [%d %d] to %s",
                  frameOut->FrameWidth, frameOut->FrameHeight, fileName);
        }
    }

    /* output queue */
    err = mMppMpi->enqueue(mMppCtx, MPP_PORT_OUTPUT, task);
    if (err != MPP_OK)
        ALOGE("failed to enqueue output task");

    mFrames->lock();
    mFrames->add_at_tail(&frame, sizeof(frame));
    mFrames->unlock();

    mFrameCount++;

cleanUp:
    /* CleanUp: free input packet buffer */
    err = mMppMpi->dequeue(mMppCtx, MPP_PORT_INPUT, &task);
    if (err == MPP_OK && task) {
        MppPacket packet = NULL;
        mpp_task_meta_get_packet(task, KEY_INPUT_PACKET, &packet);

        mpp_packet_deinit(&packet);

        err = mMppMpi->enqueue(mMppCtx, MPP_PORT_INPUT, task);
        if (err != MPP_OK)
            ALOGE("failed to enqueue input task");

    } else {
        ALOGE("failed to free input packet buffer");
    }

    return ret;
}

bool MpiJpegDecoder::deinitOutputFrame(OutputFrame_t *frameOut) {
    MppFrame frame = NULL;

    if (NULL == frameOut || NULL == frameOut->Handle) {
        ALOGW("deinitFrame found null input");
        return false;
    }

    mFrames->lock();
    mFrames->del_at_tail(&frame, sizeof(frame));
    if (frame != frameOut->Handle) {
        ALOGW("deinit found negative output frame");
    }
    mpp_frame_deinit(&frameOut->Handle);
    mFrames->unlock();

    memset(frameOut, 0, sizeof(frameOut));

    return true;
}

bool MpiJpegDecoder::decodePacket(char* data, size_t size, OutputFrame_t *frameOut) {
    time_start_record();

    if (!sendpacket(data, size, frameOut->outputPhyAddr)) {
        ALOGE("failed to send input packet");
        return false;
    }

    if (!getoutframe(frameOut)) {
        ALOGE("failed to get output frame");
        return false;
    }

    time_end_record("decode packet");

    return true;
}

bool MpiJpegDecoder::decodeFile(const char *inputFile, const char *outputFile) {
    bool     ret  = true;
    /* input data and length */
    char    *data = NULL;
    size_t   size = 0;

    /* output frame handler */
    OutputFrame_t frameOut;

    memset(&frameOut, 0, sizeof(OutputFrame_t));

    if (!CommonUtil::storeFileData(inputFile, &data, &size)) {
        ret = false;
        goto cleanUp;
    }

    if (!decodePacket(data, size, &frameOut)) {
        ALOGE("failed to decode input packet");
        ret = false;
        goto cleanUp;
    }

    ALOGD("get output file %s - dimens %dx%d",
          outputFile, frameOut.FrameWidth, frameOut.FrameHeight);

    // write output frame to destination.
    CommonUtil::dumpDataToFile(
            frameOut.MemVirAddr, frameOut.OutputSize, outputFile);

    deinitOutputFrame(&frameOut);

    flushBuffer();

cleanUp:
    if (data) {
        free(data);
        data = NULL;
    }

    return ret;
}

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
#define LOG_TAG "MpiJpegEncoder"
#include <utils/Log.h>

#include <stdlib.h>
#include <string.h>
#include <cutils/properties.h>

#include "Utils.h"
#include "Version.h"
#include "RKExifWrapper.h"
#include "QList.h"
#include "MpiDebug.h"
#include "MpiJpegEncoder.h"

static int32_t gEncDebug = 0;

typedef struct {
    struct timeval start;
    struct timeval end;
} DebugTimeInfo;

static DebugTimeInfo gTimeInfo;

static void time_start_record() {
    if (gEncDebug & DEBUG_TIMING) {
        gettimeofday(&gTimeInfo.start, NULL);
    }
}

static void time_end_record(const char *task) {
    if (gEncDebug & DEBUG_TIMING) {
        gettimeofday(&gTimeInfo.end, NULL);
        ALOGD("%s consumes %ld ms", task,
              (gTimeInfo.end.tv_sec  - gTimeInfo.start.tv_sec)  * 1000 +
              (gTimeInfo.end.tv_usec - gTimeInfo.start.tv_usec) / 1000);
    }
}

int32_t getFrameSize(int32_t format, int width, int height) {
    int32_t size = 0;
    int32_t hstride = ALIGN(width, 16);
    int32_t vstride = ALIGN(height, 16);

    if (format <= MPP_FMT_YUV420SP_VU)
        size = hstride * hstride * 3 / 2;
    else if (format <= MPP_FMT_YUV422_UYVY) {
        // NOTE: yuyv and uyvy need to double stride
        hstride *= 2;
        size = hstride * vstride;
    } else {
        size = hstride * vstride * 4;
    }
    return size;
}

MpiJpegEncoder::MpiJpegEncoder() :
    mMppCtx(NULL),
    mMppMpi(NULL),
    mMemGroup(NULL),
    mStateReady(false),
    mFrameCount(0),
    mWidth(0),
    mHeight(0),
    mHorStride(0),
    mVerStride(0),
    mQuality(0),
    mPackets(NULL),
    mInputFmt(INPUT_FMT_YUV420SP) {
    ALOGI("libhwjpeg version %s", HWJPEG_FULL_VERSION);

    gEncDebug = property_get_int32("hwjpeg_enc_debug", 0);
}

MpiJpegEncoder::~MpiJpegEncoder() {
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }
    if (mMemGroup) {
        mpp_buffer_group_put(mMemGroup);
        mMemGroup = NULL;
    }

    if (mPackets) {
        delete mPackets;
        mPackets = NULL;
    }
}

bool MpiJpegEncoder::prepareEncoder() {
    MPP_RET err = MPP_OK;
    MppParam param = NULL;

    if (mStateReady)
        return true;

    err = mpp_create(&mMppCtx, &mMppMpi);
    if (err != MPP_OK) {
        ALOGE("failed to create mpp context");
        goto error;
    }

    err = mpp_init(mMppCtx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
    if (err != MPP_OK) {
        ALOGE("failed to init mpp");
        goto error;
    }

    // NOTE: timeout value please refer to MppPollType definition
    // 0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    {
        MppPollType timeout = MPP_POLL_BLOCK;
        param = &timeout;
        err = mMppMpi->control(mMppCtx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (err != MPP_OK) {
            ALOGE("failed to set output timeout %d err %d", timeout, err);
            goto error;
        }

        err = mMppMpi->control(mMppCtx, MPP_SET_INPUT_TIMEOUT, param);
        if (err != MPP_OK) {
            ALOGE("failed to set input timeout %d err %d", timeout, err);
            goto error;
        }
    }

    mPackets = new QList((node_destructor)mpp_packet_deinit);

    /* mpp memery buffer group */
    mpp_buffer_group_get_internal(
            &mMemGroup, (MPP_BUFFER_TYPE_DMA_HEAP | MPP_BUFFER_FLAGS_DMA32));
    if (!mMemGroup) {
        mpp_buffer_group_get_internal(&mMemGroup, MPP_BUFFER_TYPE_ION);
    }

    mStateReady = true;

    return true;

error:
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }
    return false;
}

bool MpiJpegEncoder::flushBuffer() {
    if (mStateReady) {
        mPackets->lock();
        mPackets->flush();
        mPackets->unlock();

        mMppMpi->reset(mMppCtx);
    }
    return true;
}

bool MpiJpegEncoder::updateEncodeCfg(
        int32_t width, int32_t height, InputFormat format,
        int32_t qLvl, int32_t hstride, int32_t vstride) {
    MPP_RET err = MPP_OK;
    MppEncCfg cfg;

    if (!mStateReady) {
        return false;
    }

    if (mWidth == width && mHeight == height && mInputFmt == format)
        return true;

    if (width < 16 || width > 8192 || height < 16 || height > 8192 ) {
        ALOGE("invalid size %dx%d is not in range [16..8192]", width, height);
        return false;
    }

    if (qLvl < 10 || qLvl > 99) {
        ALOGW("invalid quality %d(10 ~ 99), set 80 default", qLvl);
        qLvl = 80;
    }

    hstride = (hstride > 0) ? hstride : width;
    vstride = (vstride > 0) ? vstride : height;

    mpp_enc_cfg_init(&cfg);
    mMppMpi->control(mMppCtx, MPP_ENC_GET_CFG, cfg);

    mpp_enc_cfg_set_s32(cfg, "prep:width", width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", hstride);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", vstride);
    mpp_enc_cfg_set_s32(cfg, "prep:format", format);
    mpp_enc_cfg_set_s32(cfg, "prep:roattion", MPP_ENC_ROT_0);

    /* q_factor range from 1 ~ 99 */
    mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_FIXQP);
    mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", qLvl);
    mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);
    mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 1);

    err = mMppMpi->control(mMppCtx, MPP_ENC_SET_CFG, cfg);
    if (err != MPP_OK) {
        mpp_enc_cfg_deinit(cfg);
        ALOGE("failed to set config, err %d", err);
        return false;
    }

    mWidth     = width;
    mHeight    = height;
    mHorStride = hstride;
    mVerStride = vstride;
    mInputFmt  = format;
    mQuality   = qLvl;

    mpp_enc_cfg_deinit(cfg);

    ALOGD("updateCfg: w %d h %d hor %d ver %d inputFmt %d quality %d",
          mWidth, mHeight, mHorStride, mVerStride, mInputFmt, mQuality);

    return true;
}

bool MpiJpegEncoder::deinitOutputPacket(OutputPacket_t *packetOut) {
    MppPacket packet = NULL;

    if (packetOut == NULL || packetOut->handle == NULL) {
        ALOGW("deinitPacket found null input");
        return false;
    }

    mPackets->lock();
    mPackets->del_at_tail(&packet, sizeof(packet));
    if (packet == packetOut->handle) {
        mpp_packet_deinit(&packet);
    } else {
        ALOGW("deinit found negative output packet");
        mpp_packet_deinit(&packetOut->handle);
    }
    mPackets->unlock();

    memset(packetOut, 0, sizeof(packetOut));

    return true;
}

bool MpiJpegEncoder::runFrameEnc(MppFrame inFrame, MppPacket &outPacket) {
    MPP_RET err  = MPP_OK;
    MppTask task = NULL;

    if (!inFrame) {
        ALOGE("runFrameEnc null input");
        return false;
    }

    /* start queue input task */
    err = mMppMpi->poll(mMppCtx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (err != MPP_OK) {
        ALOGE("failed to poll input task");
        return false;
    }

    /* dequeue input port */
    err = mMppMpi->dequeue(mMppCtx, MPP_PORT_INPUT, &task);
    if (err != MPP_OK) {
        ALOGE("failed dequeue to input task");
        return false;
    }

    mpp_task_meta_set_frame(task, KEY_INPUT_FRAME, inFrame);
    if (outPacket)
        mpp_task_meta_set_packet(task, KEY_OUTPUT_PACKET, outPacket);

    /* enqueue input port */
    err = mMppMpi->enqueue(mMppCtx, MPP_PORT_INPUT, task);
    if (err != MPP_OK) {
        ALOGE("failed to enqueue input task");
        return false;
    }

    task = NULL;

    /* poll and wait here */
    err = mMppMpi->poll(mMppCtx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (err != MPP_OK) {
        ALOGE("failed to poll output task");
        return false;
    }

    /* dequeue output port */
    err = mMppMpi->dequeue(mMppCtx, MPP_PORT_OUTPUT, &task);
    if (err != MPP_OK || !task) {
        ALOGE("failed to dequeue output task");
        return false;
    }

    MppPacket packet = NULL;
    mpp_task_meta_get_packet(task, KEY_OUTPUT_PACKET, &packet);

    /* enqueue output port */
    err = mMppMpi->enqueue(mMppCtx, MPP_PORT_OUTPUT, task);
    if (err != MPP_OK) {
        ALOGE("failed to enqueue output task");
        return false;
    }

    mPackets->lock();
    mPackets->add_at_tail(&packet, sizeof(packet));
    mPackets->unlock();

    outPacket = packet;

    return true;
}

bool MpiJpegEncoder::encodeFrame(char *data, OutputPacket_t *packetOut) {
    MPP_RET    err        = MPP_OK;
    int32_t    size       = 0;
    /* input frame and output packet */
    MppFrame   inFrame    = NULL;
    MppBuffer  inFrameBuf = NULL;
    void      *inFramePtr = NULL;
    MppPacket  outPacket  = NULL;

    if (!mStateReady || !data) {
        return false;
    }

    time_start_record();

    size = getFrameSize(mInputFmt, mHorStride, mVerStride);

    /* dump input data if neccessary */
    if (gEncDebug & DEBUG_RECORD_IN) {
        char fileName[60];
        sprintf(fileName, "/data/video/enc_input_%d.yuv", mFrameCount);

        if (CommonUtil::dumpDataToFile(data, size, fileName)) {
            ALOGD("dump input yuv[%d %d] to %s", mWidth, mHeight, fileName);
        }
    }

    err = mpp_buffer_get(mMemGroup, &inFrameBuf, size);
    if (err != MPP_OK) {
        ALOGE("failed to get buffer for input frame err %d", err);
        goto cleanUp;
    }

    inFramePtr = mpp_buffer_get_ptr(inFrameBuf);
    memcpy(inFrameBuf, data, size);

    err = mpp_frame_init(&inFrame);
    if (err != MPP_OK) {
        ALOGE("failed to init input frame");
        goto cleanUp;
    }

    mpp_frame_set_width(inFrame, mWidth);
    mpp_frame_set_height(inFrame, mHeight);
    mpp_frame_set_hor_stride(inFrame, mHorStride);
    mpp_frame_set_ver_stride(inFrame, mVerStride);
    mpp_frame_set_fmt(inFrame, (MppFrameFormat)mInputFmt);
    mpp_frame_set_buffer(inFrame, inFrameBuf);

    if (runFrameEnc(inFrame, outPacket)) {
        memset(packetOut, 0, sizeof(OutputPacket_t));

        packetOut->data = mpp_packet_get_pos(outPacket);
        packetOut->size = mpp_packet_get_length(outPacket);
        packetOut->handle = outPacket;

        /* dump output packet at mOutputFile if neccessary */
        if (gEncDebug & DEBUG_RECORD_OUT) {
            char fileName[60];
            sprintf(fileName, "/data/video/enc_output_%d.jpg", mFrameCount);

            if (CommonUtil::dumpDataToFile(packetOut->data, packetOut->size, fileName)) {
                ALOGD("dump output jpg to %s", fileName);
            }
        }

        mFrameCount++;

        ALOGV("encoded one frame get output size %d", packetOut->size);
    } else {
        err = MPP_NOK;
    }

cleanUp:
    if (inFrame) {
        mpp_frame_deinit(&inFrame);
        inFrame = NULL;
    }
    if (inFrameBuf) {
        mpp_buffer_put(inFrameBuf);
        inFrameBuf = NULL;
    }

    time_end_record("encode frame");

    return !err;
}

bool MpiJpegEncoder::encodeFile(const char *inputFile, const char *outputFile) {
    bool    ret  = true;
    /* input data and length */
    char   *data = NULL;
    size_t  size = 0;

    /* output packet handler */
    OutputPacket_t packetOut;

    memset(&packetOut, 0, sizeof(packetOut));

    ALOGD("encodeFile start with cfg %dx%d inputFmt %d", mWidth, mHeight, mInputFmt);

    if (!CommonUtil::storeFileData(inputFile, &data, &size)) {
        ret = false;
        goto cleanUp;
    }

    if (!encodeFrame(data, &packetOut)) {
        ALOGE("failed to encode input frame");
        ret = false;
        goto cleanUp;
    }

    // write output packet to destination.
    CommonUtil::dumpDataToFile(packetOut.data, packetOut.size, outputFile);

    ALOGD("get output file %s - size %d", outputFile, packetOut.size);

    deinitOutputPacket(&packetOut);

    flushBuffer();

cleanUp:
    if (data) {
        free(data);
        data = NULL;
    }

    return ret;
}

bool MpiJpegEncoder::getThumbImage(EncInInfo *inInfo, int32_t outputFd) {
    bool ret = true;
    MppBuffer buffer = NULL;

    int32_t srcAddr   = inInfo->inputPhyAddr;
    int32_t dstAddr   = outputFd;
    int32_t srcWidth  = ALIGN(inInfo->width, 2);
    int32_t srcHeight = ALIGN(inInfo->height, 2);
    int32_t dstWidth  = ALIGN(inInfo->thumbWidth, 2);
    int32_t dstHeight = ALIGN(inInfo->thumbHeight, 2);

    float hScale = (float)srcWidth / dstWidth;
    float vScale = (float)srcHeight / dstHeight;

    // librga can't support scale largger than 8
    if (hScale > 8 || vScale > 8) {
        MPP_RET err = MPP_OK;
        int32_t scaleWidth = 0, scaleHeight = 0;

        ALOGD("Big YUV scale[%f,%f], will crop twice instead.", hScale, vScale);

        scaleWidth = ALIGN(srcWidth / 8, 16);
        scaleHeight = ALIGN(srcHeight / 8, 2);

        err = mpp_buffer_get(mMemGroup, &buffer, scaleWidth * scaleHeight * 2);
        if (err != MPP_OK) {
            ALOGE("failed to get scale buffer, err %d", err);
            ret = false;
            goto cleanUp;
        }

        dstAddr = mpp_buffer_get_fd(buffer);

        ret = CommonUtil::cropImage(
                srcAddr, dstAddr, srcWidth, srcHeight,
                srcWidth, srcHeight, scaleWidth, scaleHeight);
        if (!ret) {
            ALOGE("failed to crop scale");
            goto cleanUp;
        }

        srcAddr = dstAddr;
        dstAddr = outputFd;
        srcWidth  = scaleWidth;
        srcHeight = scaleHeight;
    }

    ret = CommonUtil::cropImage(
            srcAddr, dstAddr, srcWidth, srcHeight,
            srcWidth, srcHeight, dstWidth, dstHeight);

cleanUp:
    if (buffer) {
        mpp_buffer_put(buffer);
        buffer = NULL;
    }

    return ret;
}

bool MpiJpegEncoder::encodeThumbImage(EncInInfo *inInfo, OutputPacket_t *packetOut) {
    MPP_RET   err          = MPP_OK;
    /* input frame and output packet */
    MppFrame  inFrame      = NULL;
    MppBuffer inFrameBuf   = NULL;
    int       inFrmFd      = 0;
    MppPacket outPacket    = NULL;
    MppBuffer outPacketBuf = NULL;

    int32_t width   = inInfo->thumbWidth;
    int32_t height  = inInfo->thumbHeight;
    int32_t quant   = inInfo->thumbQLvl;
    InputFormat fmt = inInfo->format;

    ALOGV("encode thumb size w %d h %d fmt %d qlvl %d", width, height, fmt, quant);

    /* update encode quality and config before encode */
    updateEncodeCfg(width, height, fmt, quant);

    mpp_frame_init(&inFrame);
    mpp_frame_set_width(inFrame, width);
    mpp_frame_set_height(inFrame, height);
    mpp_frame_set_hor_stride(inFrame, width);
    mpp_frame_set_ver_stride(inFrame, height);
    mpp_frame_set_fmt(inFrame, (MppFrameFormat)fmt);

    {
        /* we need to cut raw yuv image into small size for thumbnail
           first. since librga can't support scale larger than 16, we
           need crop twice sometimes.
           in this case, we need allocate larger buffer. */
        float hScale = (float)inInfo->width / inInfo->thumbWidth;
        float vScale = (float)inInfo->height / inInfo->thumbHeight;
        int32_t allocWidth = width;
        int32_t allocHeight = height;
        int32_t allocSize = 0;

        if (hScale > 8 || vScale > 8) {
            allocWidth = inInfo->width / 8;
            allocHeight = inInfo->height / 8;
        }

        allocSize = getFrameSize(fmt, allocWidth, allocHeight);

        err = mpp_buffer_get(mMemGroup, &inFrameBuf, allocSize);
        if (err != MPP_OK) {
            ALOGE("failed to get input thumb buffer, err %d", err);
            goto cleanUp;
        }
    }

    inFrmFd = mpp_buffer_get_fd(inFrameBuf);

    /* get thumb image by crop big image */
    if (!getThumbImage(inInfo, inFrmFd)) {
        ALOGE("failed to crop yuv image before thumb encode.");
        goto cleanUp;
    }

    mpp_frame_set_buffer(inFrame, inFrameBuf);

    /* allocate output packet buffer */
    err = mpp_buffer_get(mMemGroup, &outPacketBuf, width * height * 2);
    if (err != MPP_OK) {
        ALOGE("failed to get output thumb buffer, err %d", err);
        goto cleanUp;
    }
    mpp_packet_init_with_buffer(&outPacket, outPacketBuf);
    /* NOTE: It is important to clear output packet length */
    mpp_packet_set_length(outPacket, 0);

    if (runFrameEnc(inFrame, outPacket)) {
        memset(packetOut, 0, sizeof(packetOut));

        packetOut->data = mpp_packet_get_data(outPacket);
        packetOut->size = mpp_packet_get_length(outPacket);
        packetOut->handle = outPacket;

        ALOGD("get thumb jpg output size %d", packetOut->size);
    } else {
        err = MPP_NOK;
    }

cleanUp:
    if (inFrameBuf) {
        mpp_buffer_put(inFrameBuf);
        inFrameBuf = NULL;
    }
    if (outPacketBuf) {
        mpp_buffer_put(outPacketBuf);
        outPacketBuf = NULL;
    }
    if (inFrame) {
        mpp_frame_deinit(&inFrame);
        inFrame = NULL;
    }

    return !err;
}

bool MpiJpegEncoder::encodeBigImage(
        EncInInfo *inInfo, OutBuffer_t *outBuffer, OutputPacket_t *packetOut) {
    MPP_RET   err          = MPP_OK;
    /* input frame and output packet */
    MppFrame  inFrame      = NULL;
    MppBuffer inFrameBuf   = NULL;
    MppPacket outPacket    = NULL;
    MppBuffer outPacketBuf = NULL;

    int32_t width   = inInfo->width;
    int32_t height  = inInfo->height;
    int32_t quant   = inInfo->qLvl;
    InputFormat fmt = inInfo->format;

    ALOGV("encode frame w %d h %d fmt %d qlvl %d", width, height, fmt, quant);

    if (!CommonUtil::isValidDmaFd(inInfo->inputPhyAddr)) {
        ALOGE("fd(%d) not a valid dma buffer", inInfo->inputPhyAddr);
        return false;
    }

    MppEncUserData userData;

    /* update encode quality and config before encode */
    updateEncodeCfg(width, height, fmt, quant);

    mpp_frame_init(&inFrame);
    mpp_frame_set_width(inFrame, width);
    mpp_frame_set_height(inFrame, height);
    mpp_frame_set_hor_stride(inFrame, width);
    mpp_frame_set_ver_stride(inFrame, height);
    mpp_frame_set_fmt(inFrame, (MppFrameFormat)fmt);

    /* set exif data as user meta data */
    if (outBuffer->exifData) {
        MppMeta meta = mpp_frame_get_meta(inFrame);

        // FIXME: remove 0xFFD8 marker
        userData.pdata = outBuffer->exifData + 2;
        userData.len = outBuffer->exifSize - 2;

        mpp_meta_set_ptr(meta, KEY_USER_DATA, &userData);
    }

    {
        /* import input buffer */
        MppBufferInfo input;
        memset(&input, 0, sizeof(MppBufferInfo));

        input.size = getFrameSize(fmt, width, height);
        input.fd   = inInfo->inputPhyAddr;
        input.type = (MppBufferType)(MPP_BUFFER_TYPE_ION | MPP_BUFFER_FLAGS_CACHABLE);

        err = mpp_buffer_import(&inFrameBuf, &input);
        if (err != MPP_OK) {
            ALOGE("failed to import input buffer");
            goto cleanUp;
        }
        mpp_frame_set_buffer(inFrame, inFrameBuf);
    }

    {
        /* import output buffer */
        MppBufferInfo output;
        memset(&output, 0, sizeof(MppBufferInfo));

        output.size = outBuffer->size;
        output.fd   = outBuffer->fd;
        output.type = (MppBufferType)(MPP_BUFFER_TYPE_ION | MPP_BUFFER_FLAGS_CACHABLE);

        err = mpp_buffer_import(&outPacketBuf, &output);
        if (err != MPP_OK) {
            ALOGE("failed to import output buffer");
            goto cleanUp;
        }
        mpp_packet_init_with_buffer(&outPacket, outPacketBuf);
        /* NOTE: It is important to clear output packet length */
        mpp_packet_set_length(outPacket, 0);
    }

    if (runFrameEnc(inFrame, outPacket)) {
        memset(packetOut, 0, sizeof(packetOut));

        packetOut->data = mpp_packet_get_pos(outPacket);
        packetOut->size = mpp_packet_get_length(outPacket);
        packetOut->handle = outPacket;

        ALOGD("encod frame get output size %d", packetOut->size);
    } else {
        err = MPP_NOK;
    }

cleanUp:
    if (inFrameBuf) {
        mpp_buffer_put(inFrameBuf);
        inFrameBuf = NULL;
    }
    if (outPacketBuf) {
        mpp_buffer_put(outPacketBuf);
        outPacketBuf = NULL;
    }
    if (inFrame) {
        mpp_frame_deinit(&inFrame);
        inFrame = NULL;
    }

    return !err;
}

bool MpiJpegEncoder::encode(EncInInfo *inInfo, EncOutInfo *outInfo) {
    bool     ret      = true;
    /* exif information releated */
    uint8_t *exifData = NULL;
    int32_t  exifSize = 0;
    ExifInfo exifInfo;

    OutBuffer_t outBuffer;
    OutputPacket_t packetOut;

    if (!mStateReady) {
        return false;
    }

    /* dump input data if neccessary */
    if (gEncDebug & DEBUG_RECORD_IN) {
        int32_t size = 0;
        char fileName[60];
        sprintf(fileName, "/data/video/enc_input_%d.yuv", mFrameCount);

        size = getFrameSize(inInfo->format, inInfo->width, inInfo->height);

        if (CommonUtil::dumpDataToFile(inInfo->inputVirAddr, size, fileName)) {
            ALOGD("dump input yuv[%d %d] to %s", inInfo->width, inInfo->height, fileName);
        }
    }

    ALOGD("start task: width %d height %d thumbWidth %d thumbHeight %d",
          inInfo->width, inInfo->height, inInfo->thumbWidth, inInfo->thumbHeight);

    time_start_record();

    /* 1. encode thumbnail image */
    if (inInfo->doThumbNail) {
        if (!encodeThumbImage(inInfo, &packetOut)) {
            inInfo->doThumbNail = 0;
            ALOGW("failed to get thumbnail, will remove it.");
        }
    }

    /* 2. create jpeg exif app1 marker data, insert thumb if nessessary */
    memset(&exifInfo, 0, sizeof(exifInfo));

    exifInfo.exifInfo = (RkExifInfo *)inInfo->exifInfo;
    exifInfo.exifInfo->gpsInfo = (RkGPSInfo *)inInfo->gpsInfo;
    if (inInfo->doThumbNail) {
        exifInfo.thumbData = packetOut.data;
        exifInfo.thumbSize = packetOut.size;
    }

    ret = RKExifWrapper::getExifData(exifInfo, &exifData, &exifSize);
    if (!ret || exifSize <= 0) {
        ALOGE("failed to get exif data");
        goto cleanUp;
    } else {
        // release thumbnail buffer
        deinitOutputPacket(&packetOut);
    }

    /* 3. encode big image using user-specified fd */
    memset(&outBuffer, 0, sizeof(outBuffer));

    outBuffer.fd       = outInfo->outputPhyAddr;
    outBuffer.size     = outInfo->outBufLen;
    outBuffer.exifData = exifData;
    outBuffer.exifSize = exifSize;

    ret = encodeBigImage(inInfo, &outBuffer, &packetOut);
    if (!ret) {
        ALOGE("failed to get big image");
        goto cleanUp;
    }

    outInfo->outputVirAddr = (uint8_t *)packetOut.data;
    outInfo->outBufLen = packetOut.size;

    /* dump output buffer if neccessary */
    if (gEncDebug & DEBUG_RECORD_OUT) {
        char fileName[60];
        sprintf(fileName, "/data/video/enc_output_%d.jpg", mFrameCount);

        if (CommonUtil::dumpDataToFile(
                outInfo->outputVirAddr, outInfo->outBufLen, fileName)) {
            ALOGD("dump output jpg to %s", fileName);
        }
    }

    mFrameCount++;

    deinitOutputPacket(&packetOut);

    ALOGD("get output w %d h %d len %d", mWidth, mHeight, outInfo->outBufLen);

cleanUp:
    if (exifData) {
        free(exifData);
        exifData = NULL;
    }

    time_end_record("encodeImage");

    return ret;
}

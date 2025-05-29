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
#define LOG_TAG "Utils"
#include <utils/Log.h>

#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "mpp_mem.h"
#include "Utils.h"
#include "RockchipRga.h"
#include "im2d.h"

using namespace android;

rga_buffer_handle_t importRgaBuffer(
        int32_t width, int32_t height, int32_t format, int32_t fd) {
    im_handle_param_t imParam;
    memset(&imParam, 0, sizeof(im_handle_param_t));

    imParam.width  = width;
    imParam.height = height;
    imParam.format = format;

    return importbuffer_fd(fd, &imParam);
}

void freeRgaBuffer(rga_buffer_handle_t handle) {
    releasebuffer_handle(handle);
}

bool CommonUtil::dumpDataToFile(void *data, size_t size, const char *fileName) {
    FILE *file = NULL;

    file = fopen(fileName, "w+b");
    if (file == NULL) {
        ALOGE("failed to open file %s - %s", fileName, strerror(errno));
        return false;
    }

    fwrite(data, 1, size, file);
    fflush(file);
    fclose(file);

    return true;
}

bool CommonUtil::dumpDmaFdToFile(int fd, size_t size, const char *fileName) {
    FILE *file = NULL;
    void *ptr  = NULL;
    bool  ret  = true;

    file = fopen(fileName, "w+b");
    if (file == NULL) {
        ALOGE("failed to open file %s - %s", fileName, strerror(errno));
        return false;
    }

    ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == NULL) {
        ret = false;
        ALOGE("failed to map fd value %d", fd);
        goto cleanUp;
    }

    fwrite(ptr, 1, size, file);
    fflush(file);

cleanUp:
    if (file != NULL) {
        fclose(file);
        file = NULL;
    }
    return ret;
}

bool CommonUtil::storeFileData(const char *fileName, char **data, size_t *size) {
    FILE   *file    = NULL;
    size_t  fileLen = 0;

    file = fopen(fileName, "rb");
    if (file == NULL) {
        ALOGE("failed to open file %s - %s", fileName, strerror(errno));
        return false;
    }

    fseek(file, 0L, SEEK_END);
    fileLen = ftell(file);
    rewind(file);

    *data = (char *)malloc(fileLen);
    if (*data == NULL) {
        ALOGE("failed to malloc buffer");
        fclose(file);
        return false;
    }

    fread(*data, 1, fileLen, file);
    *size = fileLen;

    fflush(file);
    fclose(file);

    return true;
}

bool CommonUtil::cropImage(
        int src, int dst, int srcWidth, int srcHeight,
        int srcWstride, int srcHstride, int dstWidth, int dstHeight) {
    bool ret = true;
    int srcFormat, dstFormat;
    rga_info_t rgasrc, rgadst;
    rga_buffer_handle_t srcHdl;
    rga_buffer_handle_t dstHdl;

    RockchipRga& rkRga(RockchipRga::get());

    memset(&rgasrc, 0, sizeof(rga_info_t));
    memset(&rgadst, 0, sizeof(rga_info_t));

    srcFormat = dstFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;

    srcHdl = importRgaBuffer(srcWidth, srcHeight, srcFormat, src);
    dstHdl = importRgaBuffer(dstWidth, dstHeight, dstFormat, dst);
    if (!srcHdl || !dstHdl) {
        ALOGE("failed to import rga buffer");
        return false;
    }

    rgasrc.handle = srcHdl;
    rgadst.handle = dstHdl;
    rga_set_rect(&rgasrc.rect, 0, 0, srcWidth, srcHeight,
                 srcWstride, srcHstride, srcFormat);
    rga_set_rect(&rgadst.rect, 0, 0, dstWidth, dstHeight,
                 dstWidth, dstHeight, dstFormat);

    if (rkRga.RkRgaBlit(&rgasrc, &rgadst, NULL)) {
        ALOGE("failed to rga blit");
        ret = false;
    }

    freeRgaBuffer(srcHdl);
    freeRgaBuffer(dstHdl);

    return ret;
}

bool CommonUtil::isValidDmaFd(int fd)  {
    /* detect input file handle */
    int fsFlag = fcntl(fd, F_GETFL, NULL);
    int fdFlag = fcntl(fd, F_GETFD, NULL);
    if (fsFlag == -1 || fdFlag == -1) {
        return false;;
    }

    return true;
}

bool CommonUtil::setPerformanceMode(int on) {
    int32_t fd = -1;

    fd = open("/sys/class/devfreq/dmc/system_status", O_WRONLY);
    if (fd  == -1) {
        ALOGD("failed to open /sys/class/devfreq/dmc/system_status");
    }

    if (fd != -1) {
        ALOGD("%s performance mode", (on == 1) ? "config" : "clear");
        write(fd, (on == 1) ? "p" : "n", 1);
        close(fd);
    } else {
        ALOGD("failed to open /sys/class/devfreq/dmc/system_status");
        return false;
    }

    return true;
}

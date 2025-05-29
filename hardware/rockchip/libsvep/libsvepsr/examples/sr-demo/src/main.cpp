/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <cstring>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>

#include <sw-sync/sync/sync.h>
#include <sw-sync/sw_sync.h>
#include <unistd.h>

#include "SvepSr.h"
#include "buffer/drm_fourcc.h"
#include "buffer/SrBuffer.h"
#include "AsyncWorker.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static char optstr[] = "i:m:f:c:H:";

static void usage(char *name)
{
    fprintf(stderr, "usage: %s [-imfHc]\n", name);
    fprintf(
        stderr,
        "usage: %s -i 1280x720+0+0:1280x720@NV12 -m "
        "+async+osd+spilt=50+en=0+rotate=0 -f /data/1280x720-nv12.yuv -c 100\n",
        name);
    fprintf(stderr, "\n Query options:\n\n");
    fprintf(stderr, "\t-i\t<crop_w>x<crop_h>+<x>+<y>:<stride_w>x<stride_h>@"
                    "<format>[#afbc]\n");
    fprintf(stderr, "\t-m\t[+async][+osd][+spilt=50][+en=0][rotate=x] x: 0=0 "
                    "1=90 2=180 3=270 4=x-filp 5=y-filp \n");
    fprintf(stderr, "\t-f\t<input_image_path>\n");
    fprintf(stderr, "\t-c\t<run_cnt> default cnt=1\n");
    fprintf(stderr, "\t-H\thelp\n");
    exit(0);
}

/*-------------------------------------------
                  Functions
-------------------------------------------*/
static inline int64_t getCurrentTimeUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

struct image_arg
{
    int x;
    int y;
    int crop_w;
    int crop_h;
    int stride_w;
    int stride_h;
    char format_str[5]; /* need to leave room for terminating \0 */
    uint32_t fourcc_format;
    bool afbc;
    bool has_path = false;
    char image_path[80];
};
struct mode_arg
{
    bool async;
    bool osd;
    bool spilt;
    int spilt_rate;
    int enhancement_rate;
    SrRotateMode rotate;
    int run_cnt;
};

struct util_format_info
{
    uint32_t format;
    const char *name;
};

static const struct util_format_info format_info[] = {
  /* Indexed */
    {         DRM_FORMAT_C8,   "C8"},
 /* YUV packed */
    {       DRM_FORMAT_UYVY, "UYVY"},
    {       DRM_FORMAT_VYUY, "VYUY"},
    {       DRM_FORMAT_YUYV, "YUYV"},
    {       DRM_FORMAT_YVYU, "YVYU"},
 /* YUV semi-planar */
    {       DRM_FORMAT_NV12, "NV12"},
    {       DRM_FORMAT_NV21, "NV21"},
    {       DRM_FORMAT_NV16, "NV16"},
    {       DRM_FORMAT_NV61, "NV61"},
 // { DRM_FORMAT_NV15, "NV15"},
  /* YUV planar */
    {     DRM_FORMAT_YUV420, "YU12"},
    {     DRM_FORMAT_YVU420, "YV12"},
 /* RGB16 */
    {   DRM_FORMAT_ARGB4444, "AR12"},
    {   DRM_FORMAT_XRGB4444, "XR12"},
    {   DRM_FORMAT_ABGR4444, "AB12"},
    {   DRM_FORMAT_XBGR4444, "XB12"},
    {   DRM_FORMAT_RGBA4444, "RA12"},
    {   DRM_FORMAT_RGBX4444, "RX12"},
    {   DRM_FORMAT_BGRA4444, "BA12"},
    {   DRM_FORMAT_BGRX4444, "BX12"},
    {   DRM_FORMAT_ARGB1555, "AR15"},
    {   DRM_FORMAT_XRGB1555, "XR15"},
    {   DRM_FORMAT_ABGR1555, "AB15"},
    {   DRM_FORMAT_XBGR1555, "XB15"},
    {   DRM_FORMAT_RGBA5551, "RA15"},
    {   DRM_FORMAT_RGBX5551, "RX15"},
    {   DRM_FORMAT_BGRA5551, "BA15"},
    {   DRM_FORMAT_BGRX5551, "BX15"},
    {     DRM_FORMAT_RGB565, "RG16"},
    {     DRM_FORMAT_BGR565, "BG16"},
 /* RGB24 */
    {     DRM_FORMAT_BGR888, "BG24"},
    {     DRM_FORMAT_RGB888, "RG24"},
 /* RGB32 */
    {   DRM_FORMAT_ARGB8888, "AR24"},
    {   DRM_FORMAT_XRGB8888, "XR24"},
    {   DRM_FORMAT_ABGR8888, "AB24"},
    {   DRM_FORMAT_XBGR8888, "XB24"},
    {   DRM_FORMAT_RGBA8888, "RA24"},
    {   DRM_FORMAT_RGBX8888, "RX24"},
    {   DRM_FORMAT_BGRA8888, "BA24"},
    {   DRM_FORMAT_BGRX8888, "BX24"},
    {DRM_FORMAT_ARGB2101010, "AR30"},
    {DRM_FORMAT_XRGB2101010, "XR30"},
    {DRM_FORMAT_ABGR2101010, "AB30"},
    {DRM_FORMAT_XBGR2101010, "XB30"},
    {DRM_FORMAT_RGBA1010102, "RA30"},
    {DRM_FORMAT_RGBX1010102, "RX30"},
    {DRM_FORMAT_BGRA1010102, "BA30"},
    {DRM_FORMAT_BGRX1010102, "BX30"},
 // { DRM_FORMAT_XRGB16161616F, "XR4H"},
  // { DRM_FORMAT_XBGR16161616F, "XB4H"},
  // { DRM_FORMAT_ARGB16161616F, "AR4H"},
  // { DRM_FORMAT_ABGR16161616F, "AB4H"},
};

uint32_t util_format_fourcc(const char *name)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(format_info); i++)
        if (!strcmp(format_info[i].name, name)) return format_info[i].format;

    return 0;
}

int parse_input_image_info(struct image_arg *pipe, const char *arg)
{
    /* Parse the input image info. */
    char *end;
    pipe->crop_w = strtoul(arg, &end, 10);
    if (*end != 'x') return -EINVAL;

    arg          = end + 1;
    pipe->crop_h = strtoul(arg, &end, 10);
    if (*end != '+') return -EINVAL;

    arg     = end + 1;
    pipe->x = strtoul(arg, &end, 10);
    if (*end != '+') return -EINVAL;

    arg     = end + 1;
    pipe->y = strtoul(arg, &end, 10);
    if (*end != ':') return -EINVAL;

    arg            = end + 1;
    pipe->stride_w = strtoul(arg, &end, 10);
    if (*end != 'x') return -EINVAL;

    arg            = end + 1;
    pipe->stride_h = strtoul(arg, &end, 10);
    if (*end != '@') return -EINVAL;

    if (*end == '@')
    {
        strncpy(pipe->format_str, end + 1, 4);
        pipe->format_str[4] = '\0';
    }
    else
    {
        strcpy(pipe->format_str, "NV12");
    }

    pipe->fourcc_format = util_format_fourcc(pipe->format_str);
    if (pipe->fourcc_format == 0)
    {
        fprintf(stderr, "unknown format %s\n", pipe->format_str);
        return -EINVAL;
    }

    arg = end + 5;
    if (*arg == '#')
    {
        if (!strcmp(arg, "#afbc"))
        {
            pipe->afbc = true;
        }
    }

    return 0;
}

int parse_input_image_path(struct image_arg *pipe, const char *arg)
{
    if (arg == NULL) return -EINVAL;

    if (strlen(arg) > sizeof(pipe->image_path))
    {
        fprintf(stderr, "%s is too long, max is %zu\n", arg,
                strnlen(pipe->image_path, 80));
        return -EINVAL;
    }
    strncpy(pipe->image_path, arg, strlen(arg));
    pipe->has_path = true;
    return 0;
}

int parse_sr_mode(struct mode_arg *pipe, const char *arg)
{
    char *end;

    if (*arg != '+') return -EINVAL;

    arg++;

    if (*arg == 'a')
    {
        if (!strncmp(arg, "async", 5))
        {
            pipe->async = true;
            arg         = arg + 5;
            if (*arg == '+')
            {
                arg++;
            }
        }
    }

    if (*arg == 'o')
    {
        if (!strncmp(arg, "osd", 3))
        {
            pipe->osd = true;
            arg       = arg + 3;
            if (*arg == '+')
            {
                arg++;
            }
        }
    }

    if (*arg == 's')
    {
        if (!strncmp(arg, "spilt", 5))
        {
            pipe->spilt = true;
            arg         = arg + 5;
            if (*arg == '=')
            {
                arg++;
                pipe->spilt_rate = strtoul(arg, &end, 10);
                arg              = end;
            }
            else
            {
                pipe->spilt_rate = 50;
            }

            if (*arg == '+')
            {
                arg++;
            }
        }
    }

    if (*arg == 'e')
    {
        if (!strncmp(arg, "en", 2))
        {
            arg = arg + 2;
            if (*arg == '=')
            {
                arg++;
                pipe->enhancement_rate = strtoul(arg, &end, 10);
                arg                    = end;
            }
            else
            {
                pipe->enhancement_rate = 0;
            }

            if (*arg == '+')
            {
                arg++;
            }
        }
    }

    if (*arg == 'r')
    {
        if (!strncmp(arg, "rotate", 6))
        {
            arg = arg + 6;
            if (*arg == '=')
            {
                arg++;
                int rotate = strtoul(arg, &end, 10);
                arg        = end;
                switch (rotate)
                {
                    case 0: pipe->rotate = SR_ROTATE_0; break;
                    case 1: pipe->rotate = SR_ROTATE_90; break;
                    case 2: pipe->rotate = SR_ROTATE_180; break;
                    case 3: pipe->rotate = SR_ROTATE_270; break;
                    case 4: pipe->rotate = SR_REFLECT_X; break;
                    case 5: pipe->rotate = SR_REFLECT_Y; break;
                    default: pipe->rotate = SR_ROTATE_0; break;
                }
            }
            else
            {
                pipe->rotate = SR_ROTATE_0;
            }

            if (*arg == '+')
            {
                arg++;
            }
        }
    }

    return 0;
}

// 解析输入参数
int parse_argv(int argc, char **argv, image_arg *input_image, mode_arg *mode)
{
    int c;
    unsigned int args = 0;
    opterr            = 0;
    bool exit         = false;
    mode->run_cnt     = 1;
    while ((c = getopt(argc, argv, optstr)) != -1)
    {
        args++;
        switch (c)
        {
            case 'i':
                if (parse_input_image_info(input_image, optarg) < 0)
                {
                    fprintf(stderr, "parse_input_image_info fail!\n");
                    exit = true;
                }
                break;
            case 'm':
                if (parse_sr_mode(mode, optarg) < 0)
                {
                    fprintf(stderr, "parse_sr_mode fail!\n");
                    exit = true;
                }
                break;
            case 'f':
                if (parse_input_image_path(input_image, optarg) < 0)
                {
                    fprintf(stderr, "parse_input_image_path fail!\n");
                    exit = true;
                }
                break;
            case 'c': mode->run_cnt = atoi(optarg); break;
            case 'H': exit = true; break;
            default: exit = true; break;
        }
    }
    if (args == 0 || exit)
    {
        fprintf(
            stderr,
            "cmd_parse: crop[%d,%d,%d,%d] image[%d,%d,%s] afbc=%d path=%s "
            "async=%d osd=%d spilt=%d spil-rate=%d en-rate=%d rotate=0x%x\n",
            input_image->x, input_image->y, input_image->crop_w,
            input_image->crop_h, input_image->stride_w, input_image->stride_h,
            input_image->format_str, input_image->afbc, input_image->image_path,
            mode->async, mode->osd, mode->spilt, mode->spilt_rate,
            mode->enhancement_rate, mode->rotate);
        usage(argv[0]);
        return -1;
    }

    fprintf(stderr,
            "cmd_parse: crop[%d,%d,%d,%d] image[%d,%d,%s] afbc=%d path=%s "
            "async=%d osd=%d spilt=%d spil-rate=%d en-rate=%d rotate=0x%x\n",
            input_image->x, input_image->y, input_image->crop_w,
            input_image->crop_h, input_image->stride_w, input_image->stride_h,
            input_image->format_str, input_image->afbc, input_image->image_path,
            mode->async, mode->osd, mode->spilt, mode->spilt_rate,
            mode->enhancement_rate, mode->rotate);
    return 0;
}

int main(int argc, char **argv)
{
    image_arg input_image;
    mode_arg mode;
    memset(&input_image, 0x00, sizeof(input_image));
    memset(&mode, 0x00, sizeof(mode));
    mode.rotate = SrRotateMode::SR_ROTATE_0;
    if (parse_argv(argc, argv, &input_image, &mode))
    {
        return -1;
    }

    // 1. 获得SR实例
    std::shared_ptr<SvepSr> sr_module = std::shared_ptr<SvepSr>(new SvepSr());
    if (sr_module == NULL)
    {
        fprintf(stderr, "Sr init fail check\n");
        return -1;
    }

    SrError error = SrError::None;
    // 2. 初始化
    error = sr_module->Init(SR_VERSION, false);
    if (error != SrError::None)
    {
        fprintf(stderr, "Sr Init fail.\n");
        return -1;
    }

    // 3. 设置SR强度
    if (mode.enhancement_rate > 0)
    {
        error = sr_module->SetEnhancementRate(mode.enhancement_rate);
        if (error != SrError::None)
        {
            fprintf(stderr, "Sr SetEnhancementRate fail.\n");
            return -1;
        }
    }

    // 4. 设置SR对比模式为扫描模式
    if (mode.spilt)
    {
        error = sr_module->SetContrastMode(mode.spilt, mode.spilt_rate);
        if (error != SrError::None)
        {
            fprintf(stderr, "Sr SetContrastMode fail.\n");
            return -1;
        }
    }

    // 5. 设置SR OSD模式与字符串
    if (mode.osd)
    {
        error = sr_module->SetOsdMode(SR_OSD_ENABLE_VIDEO, SR_OSD_VIDEO_STR);
        if (error != SrError::None)
        {
            fprintf(stderr, "Sr SetOsdMode fail.\n");
            return -1;
        }
    }

    // 6. 设置SR rotate 模式
    if (mode.rotate >= SR_ROTATE_0)
    {
        error = sr_module->SetRotateMode(mode.rotate);
        if (error != SrError::None)
        {
            fprintf(stderr, "Sr SetOsdMode fail.\n");
            return -1;
        }
    }

    // 7. 申请src内存
    SrBuffer *src_buffer = new SrBuffer(
        input_image.stride_w, input_image.stride_h, input_image.stride_w,
        input_image.fourcc_format, "SrTestSrcBuffer");
    if (src_buffer->Init())
    {
        fprintf(stderr, "Alloc src buffer fail,  check error : %s\n",
                strerror(errno));
        return -1;
    }

    // 8. 从外部获取源图像数据，并安装申请格式打印输出
    if (input_image.has_path == true)
    {
        src_buffer->FillFromFile(input_image.image_path);
        src_buffer->DumpData();
    }

    // 7. 设置源图像参数
    SrImageInfo src;
    src.mBufferInfo_.iFd_           = src_buffer->GetFd();
    src.mBufferInfo_.iWidth_        = src_buffer->GetWidth();
    src.mBufferInfo_.iHeight_       = src_buffer->GetHeight();
    src.mBufferInfo_.iFormat_       = src_buffer->GetFourccFormat();
    src.mBufferInfo_.iStride_       = src_buffer->GetStride();
    src.mBufferInfo_.iHeightStride_ = src_buffer->GetHeightStride();
    src.mBufferInfo_.uBufferId_     = src_buffer->GetBufferId();
    src.mBufferInfo_.iSize_         = src_buffer->GetSize();

    src.mCrop_.iLeft_               = input_image.x;
    src.mCrop_.iTop_                = input_image.y;
    src.mCrop_.iRight_              = input_image.x + input_image.crop_w;
    src.mCrop_.iBottom_             = input_image.y + input_image.crop_h;

    if (input_image.afbc)
    {
        src.mBufferInfo_.uMask_ = SR_AFBC_FORMATE;
    }

    // 8. 获取SR处理模式与目标图像参数信息
    SrModeUsage sr_mode_usage = SR_MODE_NONE;
    SrMode sr_mde = SrMode::UN_SUPPORT;
    error         = sr_module->MatchSrMode(&src, sr_mode_usage, &sr_mde);
    if (error != SrError::None)
    {
        fprintf(stderr, "SwitchSrModeAndGetDstInfo fail, error=%d\n", error);
        return -1;
    }

    SrImageInfo target_image_info;
    error = sr_module->GetDetImageInfo(&target_image_info);
    if (error != SrError::None)
    {
        fprintf(stderr, "SwitchSrModeAndGetDstInfo fail, error=%d\n", error);
        return -1;
    }

    // 9. 申请目标图像内存
    SrBuffer *dst_buffer = new SrBuffer(
        target_image_info.mBufferInfo_.iWidth_,
        target_image_info.mBufferInfo_.iHeight_,
        target_image_info.mBufferInfo_.iStride_,
        target_image_info.mBufferInfo_.iHeightStride_,
        target_image_info.mBufferInfo_.iFormat_, 0, "SrTestDstBuffer");
    if (dst_buffer->Init())
    {
        fprintf(stderr, "Alloc dst buffer error : %s\n", strerror(errno));
        return -1;
    }

    // 10. 执行SR-SR算法
    int cnt          = 0;
    int releaseFence = -1;
    if (mode.async)
    {
        AsyncWorker worker;
        while (cnt++ < mode.run_cnt)
        {
            // 11. 更新 src 参数
            SrImageInfo src;
            src.mBufferInfo_.iFd_           = src_buffer->GetFd();
            src.mBufferInfo_.iWidth_        = src_buffer->GetWidth();
            src.mBufferInfo_.iHeight_       = src_buffer->GetHeight();
            src.mBufferInfo_.iFormat_       = src_buffer->GetFourccFormat();
            src.mBufferInfo_.iStride_       = src_buffer->GetStride();
            src.mBufferInfo_.iHeightStride_ = src_buffer->GetHeightStride();
            src.mBufferInfo_.uBufferId_     = src_buffer->GetBufferId();
            src.mBufferInfo_.iSize_         = src_buffer->GetSize();

            src.mCrop_.iLeft_               = input_image.x;
            src.mCrop_.iTop_                = input_image.y;
            src.mCrop_.iRight_  = input_image.x + input_image.crop_w;
            src.mCrop_.iBottom_ = input_image.y + input_image.crop_h;

            if (input_image.afbc)
            {
                src.mBufferInfo_.uMask_ = SR_AFBC_FORMATE;
            }

            // 12. 更新 dst 参数
            SrImageInfo dst;
            dst.mBufferInfo_.iFd_           = dst_buffer->GetFd();
            dst.mBufferInfo_.iWidth_        = dst_buffer->GetWidth();
            dst.mBufferInfo_.iHeight_       = dst_buffer->GetHeight();
            dst.mBufferInfo_.iFormat_       = dst_buffer->GetFourccFormat();
            dst.mBufferInfo_.iStride_       = dst_buffer->GetStride();
            dst.mBufferInfo_.iHeightStride_ = dst_buffer->GetHeightStride();
            dst.mBufferInfo_.uBufferId_     = dst_buffer->GetBufferId();
            dst.mBufferInfo_.iSize_         = dst_buffer->GetSize();

            dst.mCrop_.iLeft_               = target_image_info.mCrop_.iLeft_;
            dst.mCrop_.iTop_                = target_image_info.mCrop_.iTop_;
            dst.mCrop_.iRight_              = target_image_info.mCrop_.iRight_;
            dst.mCrop_.iBottom_             = target_image_info.mCrop_.iBottom_;

            // 13. RunAsync
            error = sr_module->RunAsync(&src, &dst, &releaseFence);
            if (error != SrError::None)
            {
                fprintf(stderr, "Sr RunAsync fail\n");
                return -1;
            }

            // 14. 将 ReleaseFence 传递给下一个线程处理
            if (releaseFence > 0) worker.Queue(releaseFence);
        }
        worker.WaitFinish();
    }
    else
    { // async==false
        int64_t start_us = getCurrentTimeUs();
        // 11. 更新 src 参数
        SrImageInfo src;
        src.mBufferInfo_.iFd_           = src_buffer->GetFd();
        src.mBufferInfo_.iWidth_        = src_buffer->GetWidth();
        src.mBufferInfo_.iHeight_       = src_buffer->GetHeight();
        src.mBufferInfo_.iFormat_       = src_buffer->GetFourccFormat();
        src.mBufferInfo_.iStride_       = src_buffer->GetStride();
        src.mBufferInfo_.iHeightStride_ = src_buffer->GetHeightStride();
        src.mBufferInfo_.uBufferId_     = src_buffer->GetBufferId();
        src.mBufferInfo_.iSize_         = src_buffer->GetSize();

        src.mCrop_.iLeft_               = input_image.x;
        src.mCrop_.iTop_                = input_image.y;
        src.mCrop_.iRight_              = input_image.x + input_image.crop_w;
        src.mCrop_.iBottom_             = input_image.y + input_image.crop_h;

        if (input_image.afbc)
        {
            src.mBufferInfo_.uMask_ = SR_AFBC_FORMATE;
        }

        // 12. 更新 dst 参数
        SrImageInfo dst;
        dst.mBufferInfo_.iFd_           = dst_buffer->GetFd();
        dst.mBufferInfo_.iWidth_        = dst_buffer->GetWidth();
        dst.mBufferInfo_.iHeight_       = dst_buffer->GetHeight();
        dst.mBufferInfo_.iFormat_       = dst_buffer->GetFourccFormat();
        dst.mBufferInfo_.iStride_       = dst_buffer->GetStride();
        dst.mBufferInfo_.iHeightStride_ = dst_buffer->GetHeightStride();
        dst.mBufferInfo_.uBufferId_     = dst_buffer->GetBufferId();
        dst.mBufferInfo_.iSize_         = dst_buffer->GetSize();

        dst.mCrop_.iLeft_               = target_image_info.mCrop_.iLeft_;
        dst.mCrop_.iTop_                = target_image_info.mCrop_.iTop_;
        dst.mCrop_.iRight_              = target_image_info.mCrop_.iRight_;
        dst.mCrop_.iBottom_             = target_image_info.mCrop_.iBottom_;

        // 13. RunAsync
        error = sr_module->Run(&src, &dst);
        if (error != SrError::None)
        {
            fprintf(stderr, "Sr Run fail\n");
            return -1;
        }
        int64_t elapse_us = getCurrentTimeUs() - start_us;
        fprintf(stderr, "Sr Run success: Time = %.2fms, FPS = %.2f, cnt=%d\n",
                elapse_us / 1000.f, 1000.f * 1000.f / elapse_us, cnt);
    }

    fprintf(stderr, "Please enter a word to dump dst data\n");
    getchar();
    // 13. 抓打印输出图像
    dst_buffer->DumpData();
    fprintf(stderr, "Please enter a release sr resource and exit!\n");
    getchar();
    delete src_buffer;
    delete dst_buffer;
    return 0;
}

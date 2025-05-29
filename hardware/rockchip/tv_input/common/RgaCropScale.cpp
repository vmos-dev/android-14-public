/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
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

#include "RgaCropScale.h"
#include <utils/Singleton.h>
#include <RockchipRga.h>
#include <im2d.h>

namespace android {
namespace tvinput {

//#define TARGET_RK3588 1

#if (defined(TARGET_RK32) || defined(TARGET_RK3368) || defined(TARGET_RK3588))
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (4096)

#else
#define RGA_VER (1.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (4096)

#endif

#if defined(TARGET_RK3588)
//#include <im2d_api/im2d.h>
#endif

int RgaCropScale::rga_copy(int src_fd, void* src_vir_addr, uint64_t src_phy_addr,
        int src_width, int src_height, int src_format,
        int dst_fd, void* dst_vir_addr, uint64_t dst_phy_addr) {
    im_handle_param_t src_param;
    memset(&src_param, 0, sizeof(src_param));
    src_param.width = src_width;
    src_param.height = src_height;
    if (src_format == HAL_PIXEL_FORMAT_YCbCr_422_SP) {
        src_param.format = RK_FORMAT_YCbCr_422_SP;
    } else {
        src_param.format = src_format;
    }
    rga_buffer_handle_t src_handle;
    if (src_fd != -1) {
        src_handle = importbuffer_fd(src_fd, &src_param);
    } else if (src_vir_addr) {
        src_handle = importbuffer_virtualaddr(src_vir_addr, &src_param);
    } else if (src_phy_addr != -1) {
        src_handle = importbuffer_physicaladdr(src_phy_addr, &src_param);
    } else {
        ALOGE("%s import src failed due to input null", __FUNCTION__);
        return -1;
    }
    if (src_handle <= 0) {
        ALOGE("%s import %d src failed %dx%d, format=%d, %s",
            __FUNCTION__, src_fd, src_width, src_height, src_format, imStrError());
        return -1;
    }
    rga_buffer_t src = wrapbuffer_handle(src_handle, src_width, src_height, src_param.format);
    if (src.width == 0 || src.height == 0) {
        ALOGE("%s src %dx%d-%d%d, format=%d, %s",
            __FUNCTION__, src.width, src.height, src_width, src_height, src_format, imStrError());
        releasebuffer_handle(src_handle);
        return -1;
    }

    int dst_width = src_param.width;
    int dst_height = src_param.height;
    int dst_format = src_param.format;
    im_handle_param_t dst_param;
    memset(&dst_param, 0, sizeof(dst_param));
    dst_param.width = dst_width;
    dst_param.height = dst_height;
    dst_param.format = dst_format;
    rga_buffer_handle_t dst_handle;
    if (dst_fd != -1) {
        dst_handle = importbuffer_fd(dst_fd, &dst_param);
    } else if (dst_vir_addr) {
        dst_handle = importbuffer_virtualaddr(dst_vir_addr, &dst_param);
    } else if (dst_phy_addr != -1) {
        dst_handle = importbuffer_physicaladdr(dst_phy_addr, &dst_param);
    } else {
        ALOGE("%s import dst failed due to input null", __FUNCTION__);
        return -1;
    }
    if (dst_handle <= 0) {
        ALOGE("%s import %d dst failed %dx%d, format=%d, %s",
            __FUNCTION__, dst_fd, dst_width, dst_height, dst_format, imStrError());
        releasebuffer_handle(src_handle);
        return -1;
    }
    rga_buffer_t dst = wrapbuffer_handle(dst_handle, dst_width, dst_height, dst_param.format);
    if (dst.width == 0 || dst.height == 0) {
        ALOGE("%s dst %dx%d-%d%d, format=%d, %s",
            __FUNCTION__, dst.width, dst.height, dst_width, dst_height, dst_format, imStrError());
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
        return -1;
    }

    im_rect src_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    imcrop(src, dst, src_rect);
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    return 0;
}

int RgaCropScale::CropScaleNV12Or21(struct Params* in, struct Params* out)
{
    rga_info_t src, dst;
#if defined(TARGET_RK3588)
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
    im_handle_param_t param;
    memset(&param, 0, sizeof(im_handle_param_t));
    memset(&src_handle, 0, sizeof(rga_buffer_handle_t));
    memset(&dst_handle, 0, sizeof(rga_buffer_handle_t));
#endif
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));

    if (!in || !out)
        return -1;

    if((out->width > RGA_VIRTUAL_W) || (out->height > RGA_VIRTUAL_H)){
        ALOGE("%s(%d): out wxh %dx%d beyond rga capability",
            __FUNCTION__, __LINE__,
            out->width, out->height);
        return -1;
    }

    /*if ((in->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        in->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
        (out->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        out->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
        ALOGE("%s(%d): only accept NV12 or NV21 now. in fmt %d, out fmt %d",
            __FUNCTION__, __LINE__,
            in->fmt, out->fmt);
        return -1;
    }*/
    RockchipRga& rkRga(RockchipRga::get());

#if defined(TARGET_RK3588)
    param.width = in->width;
    param.height = in->height;
    param.format = in->fmt;
#endif
    if (in->fd == -1) {
        src.fd = -1;
        src.virAddr = (void*)in->vir_addr;
#if defined(TARGET_RK3588)
        LOGD("@%s,src virtual:%p",__FUNCTION__,src.virAddr);
        src_handle = importbuffer_virtualaddr(src.virAddr, &param);
#endif
    } else {
        src.fd = in->fd;
#if defined(TARGET_RK3588)
        src_handle = importbuffer_fd(src.fd, &param);
        LOGD("@%s,src fd:%d,width:%d,height:%d,format:%d",__FUNCTION__,src.fd,param.width,param.height,param.format);
#endif
    }
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

#if defined(TARGET_RK3588)
    param.width = out->width;
    param.height = out->height;
    param.format = out->fmt;
#endif
    if (out->fd == -1 ) {
        dst.fd = -1;
        dst.virAddr = (void*)out->vir_addr;
#if defined(TARGET_RK3588)
        LOGD("@%s,dst virtual:%p",__FUNCTION__,src.virAddr);
        dst_handle = importbuffer_virtualaddr(dst.virAddr, &param);
#endif
    } else {
        dst.fd = out->fd;
#if defined(TARGET_RK3588)
        dst_handle = importbuffer_fd(dst.fd, &param);
        LOGD("@%s,dst fd:%d,width:%d,height:%d,format:%d",__FUNCTION__,dst.fd,param.width,param.height,param.format);
#endif
    }
    dst.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

    rga_set_rect(&src.rect,
                in->offset_x,
                in->offset_y,
                in->width,
                in->height,
                in->width_stride,
                in->height_stride,
                in->fmt);

    rga_set_rect(&dst.rect,
                out->offset_x,
                out->offset_y,
                out->width,
                out->height,
                out->width_stride,
                out->height_stride,
                out->fmt);
    if (in->mirror)
        src.rotation = DRM_RGA_TRANSFORM_FLIP_H;

#if defined(TARGET_RK3588)
            src.handle = src_handle;
            src.fd = 0;
            dst.handle = dst_handle;
            dst.fd = 0;
#endif

    if (rkRga.RkRgaBlit(&src, &dst, NULL)) {
        ALOGE("%s:rga blit failed", __FUNCTION__);
#if defined(TARGET_RK3588)
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
#endif
        return -1;
    }
#if defined(TARGET_RK3588)
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
#endif
    return 0;
}

int RgaCropScale::rga_nv12_scale_crop(
		int src_width, int src_height,
		unsigned long src_fd, unsigned long dst_fd,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat)
{
    int ret = 0;
    rga_info_t src,dst;
    int zoom_cropW,zoom_cropH;
    int ratio = 0;
    int zoom_top_offset=0,zoom_left_offset=0;

    RockchipRga& rkRga(RockchipRga::get());

    memset(&src, 0, sizeof(rga_info_t));
    if (isYuyvFormat) {
        src.fd = -1;
        src.virAddr = (void*)src_fd;
    } else {
        src.fd = src_fd;
    }
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));
    dst.fd = dst_fd;
    dst.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

    if((dst_width > RGA_VIRTUAL_W) || (dst_height > RGA_VIRTUAL_H)){
        ALOGE("(dst_width > RGA_VIRTUAL_W) || (dst_height > RGA_VIRTUAL_H), switch to arm ");
        ret = -1;
        goto END;
    }

    //need crop ? when cts FOV,don't crop
    if(isNeedCrop && (src_width*100/src_height) != (dst_width*100/dst_height)) {
        ratio = ((src_width*100/dst_width) >= (src_height*100/dst_height))?
                (src_height*100/dst_height):
                (src_width*100/dst_width);
        zoom_cropW = (ratio*dst_width/100) & (~0x01);
        zoom_cropH = (ratio*dst_height/100) & (~0x01);
        zoom_left_offset=((src_width-zoom_cropW)>>1) & (~0x01);
        zoom_top_offset=((src_height-zoom_cropH)>>1) & (~0x01);
    }else{
        zoom_cropW = src_width;
        zoom_cropH = src_height;
        zoom_left_offset=0;
        zoom_top_offset=0;
    }

    if(zoom_val > 100){
        zoom_cropW = zoom_cropW*100/zoom_val & (~0x01);
        zoom_cropH = zoom_cropH*100/zoom_val & (~0x01);
        zoom_left_offset = ((src_width-zoom_cropW)>>1) & (~0x01);
        zoom_top_offset= ((src_height-zoom_cropH)>>1) & (~0x01);
    }

    //usb camera height align to 16,the extra eight rows need to be croped.
    if(!is16Align){
        zoom_top_offset = zoom_top_offset & (~0x07);
    }

    rga_set_rect(&src.rect, zoom_left_offset, zoom_top_offset,
                zoom_cropW, zoom_cropH, src_width,
                src_height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    if (isDstNV21)
        rga_set_rect(&dst.rect, 0, 0, dst_width, dst_height,
                    dst_width, dst_height,
                    HAL_PIXEL_FORMAT_YCrCb_420_SP);
    else
        rga_set_rect(&dst.rect, 0,0,dst_width,dst_height,
                    dst_width,dst_height,
                    HAL_PIXEL_FORMAT_YCrCb_NV12);

    if (mirror)
        src.rotation = DRM_RGA_TRANSFORM_FLIP_H;
    //TODO:sina,cosa,scale_mode,render_mode
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed", __FUNCTION__);
        goto END;
    }

    END:
    return ret;
}

} /* namespace camera2 */
} /* namespace android */

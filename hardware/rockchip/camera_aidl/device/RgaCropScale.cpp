/*
 * Copyright (c) 2023, Fuzhou Rockchip Electronics Co., Ltd
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
#include <im2d_api/im2d.h>
#include "im2d_api/im2d.hpp"
#include "im2d_api/im2d_common.h"

namespace android {
namespace camera2 {
#if !defined(TARGET_RK312X)
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (4096)
#else
#define RGA_VER (1.0)
#define RGA_ACTIVE_W (2048)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (2048)
#define RGA_VIRTUAL_H (2048)
#endif

int RgaCropScale::CropScaleNV12Or21(struct Params* in, struct Params* out)
{
    rga_info_t src, dst;
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
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

    if ((in->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        in->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
        (out->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        out->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
        ALOGE("%s(%d): only accept NV12 or NV21 now. in fmt %d, out fmt %d",
            __FUNCTION__, __LINE__,
            in->fmt, out->fmt);
        return -1;
    }
    RockchipRga& rkRga(RockchipRga::get());
    im_handle_param_t param;
    param.width = in->width;
    param.height = in->height;
    param.format = in->fmt;

    if (in->fd == -1) {
        src.fd = -1;
        src.virAddr = (void*)in->vir_addr;
        src_handle = importbuffer_virtualaddr(src.virAddr, &param);
        ALOGD("@%s,src virtual:%p,width:%d,height:%d",__FUNCTION__,src.virAddr,param.width,param.height);
    } else {
        src.fd = in->fd;
        src_handle = importbuffer_fd(src.fd, &param);
        ALOGD("@%s, src fd:%d,width:%d,height:%d",__FUNCTION__,src.fd,param.width,param.height);
    }
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

    param.width = out->width;
    param.height = out->height;
    param.format = out->fmt;

    if (out->fd == -1 ) {
        dst.fd = -1;
        dst.virAddr = (void*)out->vir_addr;
        dst_handle = importbuffer_virtualaddr(dst.virAddr, &param);
        ALOGD("@%s,dst virtual:%p,width:%d,height:%d",__FUNCTION__,dst.virAddr,param.width,param.height);
    } else {
        dst.fd = out->fd;
        dst_handle = importbuffer_fd(dst.fd, &param);
        ALOGD("@%s, dst fd:%d,width:%d,height:%d",__FUNCTION__,dst.fd,param.width,param.height);
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

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;

    if (rkRga.RkRgaBlit(&src, &dst, NULL)) {
        ALOGE("%s:rga blit failed", __FUNCTION__);
        return -1;
    }
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    return 0;
}

int RgaCropScale::rga_scale_crop(
		int src_width, int src_height,
		unsigned long src_fd, int src_format,unsigned long dst_fd,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat)
{
    int ret = 0;
    rga_info_t src,dst;
    int zoom_cropW,zoom_cropH;
    int ratio = 0;
    int zoom_top_offset=0,zoom_left_offset=0;
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;

    RockchipRga& rkRga(RockchipRga::get());

    im_handle_param_t param;
    param.width = src_width;
    param.height = src_height;
    param.format = src_format;

    memset(&src, 0, sizeof(rga_info_t));
    if (isYuyvFormat) {
        src.fd = -1;
        src.virAddr = (void*)src_fd;
        src_handle = importbuffer_virtualaddr(src.virAddr, &param);
    } else {
        src.fd = src_fd;
        src_handle = importbuffer_fd(src.fd, &param);
    }
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));
    dst.fd = dst_fd;
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }

    dst_handle = importbuffer_fd(dst.fd, &param);
    ALOGV("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
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
                src_height, src_format);
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

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed", __FUNCTION__);
        goto END;
    }

    END:
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    return ret;
}

int RgaCropScale::rga_scale_crop_use_handle(
        int src_width, int src_height,
        unsigned int src_handle, int src_format,unsigned int dst_handle,
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

    im_handle_param_t param;
    param.width = src_width;
    param.height = src_height;
    param.format = src_format;

    memset(&src, 0, sizeof(rga_info_t));
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }
    //ALOGE("@%s， dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
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
                src_height, src_format);
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

    ALOGV("src_handle = %d, dst_handle(%d)", src_handle, dst_handle);

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed", __FUNCTION__);
    }
END:

    return ret;
}

static void empty_structure(rga_buffer_t *src, rga_buffer_t *dst, rga_buffer_t *pat,
                                im_rect *srect, im_rect *drect, im_rect *prect, im_opt_t *opt) {
    if (src != NULL)
        memset(src, 0, sizeof(*src));
    if (dst != NULL)
        memset(dst, 0, sizeof(*dst));
    if (pat != NULL)
        memset(pat, 0, sizeof(*pat));
    if (srect != NULL)
        memset(srect, 0, sizeof(*srect));
    if (drect != NULL)
        memset(drect, 0, sizeof(*drect));
    if (prect != NULL)
        memset(prect, 0, sizeof(*prect));
    if (opt != NULL)
        memset(opt, 0, sizeof(*opt));
}

int RgaCropScale::Im2dBlit(struct Params* in, struct Params* out)
{
	im_rect         src_rect;
	im_rect         dst_rect;
	rga_buffer_t     src;
	rga_buffer_t     dst;
	int ret = 0;
	memset(&src_rect, 0, sizeof(src_rect));
	memset(&dst_rect, 0, sizeof(dst_rect));
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));

	im_handle_param_t param_in,param_out;

	rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
	param_in.width = in->width;
	param_in.height = in->height;
	param_in.format = in->fmt;

	param_out.width = out->width;
	param_out.height = out->height;
	param_out.format = out->fmt;

	if (in->fd == -1) {
	   src_handle = importbuffer_virtualaddr((void*)in->vir_addr, &param_in);
	} else {
	   src_handle = importbuffer_fd(in->fd, &param_in);
	}

	if (out->fd == -1) {
	   dst_handle = importbuffer_virtualaddr((void*)out->vir_addr, &param_out);
	} else {
	   dst_handle = importbuffer_fd(out->fd, &param_out);
	}

	src = wrapbuffer_handle(src_handle, in->width, in->height, in->fmt);
	dst = wrapbuffer_handle(dst_handle, out->width, out->height, out->fmt);

	if(src.width == 0 || dst.width == 0) {
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
		return -1;
	}

	dst.width = src.width;
	dst.height = src.height;

	im_opt_t opt;
	rga_buffer_t pat;
	im_rect srect;
	im_rect drect;
	im_rect prect;
	int usage = 0;
	empty_structure(NULL, NULL, &pat, &srect, &drect, &prect, &opt);

	usage |= IM_SYNC;
	if (in->blend > 0 && (in->translate_x > 0 || in->translate_y  > 0)) {
		usage |= IM_ALPHA_BLEND_DST_OVER;
		prect.x = 0;
		prect.y = 0;
		prect.width = src.width;
		prect.height = src.height;

		drect.x = in->translate_x;
		drect.y = in->translate_y;
		drect.width = dst.width;  //640
		drect.height = dst.height;//480
		ret = improcess(dst, dst, src, drect, drect, prect, usage);
	}

    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
	return ret;
}

} /* namespace camera2 */
} /* namespace android */

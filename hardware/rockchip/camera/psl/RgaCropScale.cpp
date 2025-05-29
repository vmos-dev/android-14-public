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
#include "LogHelper.h"
#include <utils/Singleton.h>
#include <RockchipRga.h>
#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <hardware/hardware_rockchip.h>
#endif


namespace android {
namespace camera2 {

#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <im2d_api/im2d.h>
#endif
int RgaCropScale::CropScaleNV12Or21(struct Params* in, struct Params* out)
{
	rga_info_t src, dst;
#if defined(ANDROID_VERSION_ABOVE_12_X)
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

	if((out->width > RGA_ACTIVE_W) || (out->height > RGA_ACTIVE_H)){
			ALOGE("%s(%d): out wxh %dx%d beyond rga capability",
                 __FUNCTION__, __LINE__,
                 out->width, out->height);
            return -1;
	}

    if ((in->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        in->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
		in->fmt != HAL_PIXEL_FORMAT_RGBA_8888) ||
        (out->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        out->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
		out->fmt != HAL_PIXEL_FORMAT_RGBA_8888)) {
        ALOGE("%s(%d): only accept NV12 or NV21 now. in fmt %d, out fmt %d",
             __FUNCTION__, __LINE__,
             in->fmt, out->fmt);
        return -1;
    }
	RockchipRga& rkRga(RockchipRga::get());

#if defined(ANDROID_VERSION_ABOVE_12_X)
	param.width = in->width;
	param.height = in->height;
	param.format = in->fmt;
#endif
	if (in->fd == -1) {
		src.fd = -1;
		src.virAddr = (void*)in->vir_addr;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		LOGD("@%s,src virtual:%p",__FUNCTION__,src.virAddr);
		src_handle = importbuffer_virtualaddr(src.virAddr, &param);
#endif
	} else {
		src.fd = in->fd;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		src_handle = importbuffer_fd(src.fd, &param);
		LOGD("@%s,src fd:%d,width:%d,height:%d,format:%d",__FUNCTION__,src.fd,param.width,param.height,param.format);
#endif
	}
	src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

    if (in->rotation != 0) {
        int zoom_cropW = in->width,zoom_cropH = in->height;
        int ratio = 0;
        int zoom_top_offset = in->offset_y,zoom_left_offset = in->offset_x;
        switch (in->rotation) {
            case 90:
                src.rotation = DRM_RGA_TRANSFORM_ROT_90;
                ratio = out->width * 1000 / out->height;
                zoom_cropH = in->height & (~0x01);
                zoom_cropW = in->width & (~0x01);
                zoom_left_offset=((in->width-zoom_cropW)>>1) & (~0x01);
                zoom_top_offset=((in->height-zoom_cropH)>>1) & (~0x01);
                in->width = zoom_cropW;
                in->height = zoom_cropH;
                in->offset_x = zoom_left_offset + in->offset_x;
                in->offset_y = zoom_top_offset + in->offset_y;
                break;
            case 180:
                src.rotation = DRM_RGA_TRANSFORM_ROT_180;
                break;
            case 270:
                src.rotation = DRM_RGA_TRANSFORM_ROT_270;
                ratio = out->width * 1000 / out->height;
                zoom_cropH = in->height & (~0x01);
                zoom_cropW = in->width & (~0x01);
                zoom_left_offset=((in->width-zoom_cropW)>>1) & (~0x01);
                zoom_top_offset=((in->height-zoom_cropH)>>1) & (~0x01);
                in->width = zoom_cropW;
                in->height = zoom_cropH;
                in->offset_x = zoom_left_offset + in->offset_x;
                in->offset_y = zoom_top_offset + in->offset_y;
                break;
        }
        LOGE("crop:%dx%d, offset:%dx%d", zoom_cropW, zoom_cropH, zoom_left_offset, zoom_top_offset);
    }

#if defined(ANDROID_VERSION_ABOVE_12_X)
	param.width = out->width;
	param.height = out->height;
	param.format = out->fmt;
#endif
	if (out->fd == -1 ) {
		dst.fd = -1;
		dst.virAddr = (void*)out->vir_addr;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		LOGD("@%s,dst virtual:%p",__FUNCTION__,src.virAddr);
		dst_handle = importbuffer_virtualaddr(dst.virAddr, &param);
#endif
	} else {
		dst.fd = out->fd;
#if defined(ANDROID_VERSION_ABOVE_12_X)
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
    if (in->mirror) {
        src.rotation |= src.rotation ?
				DRM_RGA_TRANSFORM_FLIP_H << 4 :
				DRM_RGA_TRANSFORM_FLIP_H;
        LOGD("---zc rga mirror====");
    }
    if (in->flip) {
        LOGD("---zc rga flip=====");
        src.rotation |= src.rotation ?
				DRM_RGA_TRANSFORM_FLIP_V << 4 :
				DRM_RGA_TRANSFORM_FLIP_V;
    }


#if defined(ANDROID_VERSION_ABOVE_12_X)
    src.handle = src_handle;
    src.fd = -1;
    src.in_fence_fd = -1;
    dst.handle = dst_handle;
    dst.fd = -1;
    dst.in_fence_fd = -1;
#endif

	if (rkRga.RkRgaBlit(&src, &dst, NULL)) {
	    ALOGE("%s:rga blit failed", __FUNCTION__);
#if defined(ANDROID_VERSION_ABOVE_12_X)
	    releasebuffer_handle(src_handle);
	    releasebuffer_handle(dst_handle);
#endif
	    return -1;
	}
#if defined(ANDROID_VERSION_ABOVE_12_X)
	releasebuffer_handle(src_handle);
	releasebuffer_handle(dst_handle);
#endif
    return 0;
}

/* split buffer to left right to do crop scale */
int RgaCropScale::WidthSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout) {
                /* test start */
    char *src = rgain->vir_addr;
    char *dst = rgaout->vir_addr;
    int src_fd = rgain->fd;
    int dst_fd = rgaout->fd;
    unsigned int in_w, in_h, in_offset_x, in_offset_y, out_w, out_h, out_offset_x, out_offset_y;
    unsigned int in_w_stride, in_h_stride, out_w_stride, out_h_stride;

    ALOGD("@%s: do split copy/scale start!", __FUNCTION__);
    in_w = rgain->width;
    in_h = rgain->height;
    in_offset_x = rgain->offset_x;
    in_offset_y = rgain->offset_y;
    out_w = rgaout->width;
    out_h = rgaout->height;
    out_offset_x = rgaout->offset_x;
    out_offset_y = rgaout->offset_y;
    in_w_stride = rgain->width_stride;
    in_h_stride = rgain->height_stride;
    out_w_stride = rgaout->width_stride;
    out_h_stride = rgaout->height_stride;

    rgain->width = in_w / 2;

    rgaout->width = out_w / 2;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: first split copy/scale failed!", __FUNCTION__);
        return -1;
    }

    ALOGD("@%s: do second split copy/ scale", __FUNCTION__);


    rgain->offset_x = in_offset_x + in_w / 2;
    rgaout->offset_x = out_w_stride / 2;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    ALOGD("@%s: do split copy/scale end!", __FUNCTION__);
    return 0;
}

/* split buffer to up and down to do crop scale */
int RgaCropScale::HeightSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout) {
                /* test start */
    char *src = rgain->vir_addr;
    char *dst = rgaout->vir_addr;
    int src_fd = rgain->fd;
    int dst_fd = rgaout->fd;
    unsigned int in_w, in_h, in_offset_x, in_offset_y, out_w, out_h, out_offset_x, out_offset_y;
    unsigned int in_w_stride, in_h_stride, out_w_stride, out_h_stride;

    ALOGD("@%s: do split copy/scale start!", __FUNCTION__);
    in_w = rgain->width;
    in_h = rgain->height;
    in_offset_x = rgain->offset_x;
    in_offset_y = rgain->offset_y;
    out_w = rgaout->width;
    out_h = rgaout->height;
    out_offset_x = rgaout->offset_x;
    out_offset_y = rgaout->offset_y;
    in_w_stride = rgain->width_stride;
    in_h_stride = rgain->height_stride;
    out_w_stride = rgaout->width_stride;
    out_h_stride = rgaout->height_stride;

    rgain->height = in_h / 2;
    rgain->width_stride = in_w_stride;
    rgain->height_stride = in_h_stride;

    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: first split copy/scale failed!", __FUNCTION__);
        return -1;
    }

    ALOGD("@%s: do second split copy/ scale", __FUNCTION__);

    rgain->offset_y = in_offset_y + in_h / 2;
    rgaout->offset_y = out_h_stride / 2;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    ALOGD("@%s: do split copy/scale end!", __FUNCTION__);
    return 0;
}

/* split buffer to up and down to do crop scale */
int RgaCropScale::WHSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout) {
                /* test start */
    char *src = rgain->vir_addr;
    char *dst = rgaout->vir_addr;
    int src_fd = rgain->fd;
    int dst_fd = rgaout->fd;
    unsigned int in_w, in_h, in_offset_x, in_offset_y, out_w, out_h, out_offset_x, out_offset_y;
    unsigned int in_w_stride, in_h_stride, out_w_stride, out_h_stride;

    ALOGD("@%s: do split copy/scale start!", __FUNCTION__);

    /* left top */
    ALOGD("@%s: left top split copy/scale start!", __FUNCTION__);

    in_w = rgain->width;
    in_h = rgain->height;
    in_offset_x = rgain->offset_x;
    in_offset_y = rgain->offset_y;
    out_w = rgaout->width;
    out_h = rgaout->height;
    out_offset_x = rgaout->offset_x;
    out_offset_y = rgaout->offset_y;
    in_w_stride = rgain->width_stride;
    in_h_stride = rgain->height_stride;
    out_w_stride = rgaout->width_stride;
    out_h_stride = rgaout->height_stride;

    rgain->width = in_w / 2;
    rgain->height = in_h / 2;
    rgain->width_stride = in_w_stride;
    rgain->height_stride = in_h_stride;


    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: first split copy/scale failed!", __FUNCTION__);
        return -1;
    }

    ALOGD("@%s: do right top split copy/ scale", __FUNCTION__);
    rgain->offset_x = in_offset_x + in_w / 2;
    rgain->offset_y = in_offset_y;

    rgaout->offset_x = out_w_stride / 2;
    rgaout->offset_y = 0;
    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }

    ALOGD("@%s: do left bottom split copy/ scale", __FUNCTION__);
    rgain->offset_x = in_offset_x;
    rgain->offset_y = in_offset_y + in_h / 2;

    rgaout->offset_x = 0;
    rgaout->offset_y = out_h_stride / 2;
    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }

    ALOGD("@%s: do right bottom split copy/ scale", __FUNCTION__);
    rgain->offset_x = in_offset_x + in_w / 2;
    rgain->offset_y = in_offset_y + in_h / 2;

    rgaout->offset_x = out_w_stride / 2;;
    rgaout->offset_y = out_h_stride / 2;
    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }

    ALOGD("@%s: do split copy/scale end!", __FUNCTION__);
    return 0;
}

} /* namespace camera2 */
} /* namespace android */

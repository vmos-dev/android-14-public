/*
 * Copyright (C) 2023 The Android Open Source Project
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

#define APK_VERSION	"V1.3"
#define LOG_TAG "data_bridge"
//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/v4l2-subdev.h>

#include "data_bridge.h"

#include <ui/Fence.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>

#include <utils/Singleton.h>

#define VIRTUAL_CAMERA
#define RGA_PROC 


#include <android/hardware_buffer_jni.h>
#include <vndk/hardware_buffer.h>

#include <condition_variable>
#include <unordered_map>
#include <thread>

#ifdef RGA_PROC
#include <RockchipRga.h>
#include <im2d_api/im2d.h>
#include "im2d_api/im2d.hpp"
#include "im2d_api/im2d_common.h"
#endif

//#define LOG_TIME

#define LOGE(msg,...)   ALOGE("%s(%d): " msg ,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define LOGD(msg,...)   ALOGD("%s(%d): " msg ,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define LOGV(msg,...)   ALOGV("%s(%d): " msg ,__FUNCTION__,__LINE__,##__VA_ARGS__)
using namespace std;
using namespace android;

sp<GraphicBuffer> GraphicBuffer_Init(int width, int height,int format);
int rga_scale_crop(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat,int src_sw = 0, int src_sh = 0);
int rga_scale_crop_withCache(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat,int src_sw = 0, int src_sh = 0);
int rga_scale_crop_rgba8888(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat);
int rga_scale_crop_dstfd(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,buffer_handle_t dst_buf_handle,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat);
int rga_scale_crop_dstfd_withCache(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,buffer_handle_t dst_buf_handle,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat);

int rga_fill_in(int index,
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat,int src_sw = 0, int src_sh = 0);

uint8_t *rgba_data_1080x1920 = (uint8_t*) malloc((int)1080*1920*4);
uint8_t *nv12_data_1920x1080 = (uint8_t*) malloc((int)1920*1080*1.5);
uint8_t *nv12_data_3840x2160 = (uint8_t*) malloc((int)3840*2160*1.5);
uint8_t *nv12_data_1792x3040 = (uint8_t*) malloc((int)1792*3040*1.5);

static const int kReqWaitTimeoutMs = 66;   // 33ms
bool mProcessingRequest = false;

std::condition_variable mRequestDoneCond;     // signaled when a new request is submitted

sp<GraphicBuffer> sPushBuffer1920x1080;
sp<GraphicBuffer> sPushBuffer3840x2160;
std::mutex mLock;
std::unordered_map<int, rga_buffer_handle_t> kMapRgaHandler;
std::unordered_map<int, std::condition_variable> kMapCondition;
void releaseCachedRgaHandler(){
    ALOGD("%s size:%d",__FUNCTION__,kMapRgaHandler.size());
    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
    for (auto it= kMapRgaHandler.begin(); it != kMapRgaHandler.end(); ++it) {
        rga_buffer_handle_t rga_handler = it->second;
        releasebuffer_handle(rga_handler);
        ALOGD("releasebuffer_handle :%d",rga_handler);
    }
    kMapRgaHandler.clear();
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s %d use %ldms", __FUNCTION__,kMapRgaHandler.size(),get_time_diff_ms(&last_tm,&curr_tm));
}
void signalRequestDone() {    
    mRequestDoneCond.notify_one();        
}

#ifdef VIRTUAL_CAMERA
#include <rockchip/hardware/hdmi/1.0/IHdmi.h>
#include <rockchip/hardware/hdmi/1.0/IHdmiCallback.h>

class FrameWarpperImp :public rockchip::hardware::hdmi::V1_0::IFrameWarpper{
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();

    public:
     android::hardware::Return<void> onFrame(const ::rockchip::hardware::hdmi::V1_0::FrameInfo& FrameInfo, onFrame_cb _hidl_cb) override{
        int deviceId = atoi(FrameInfo.deviceId.c_str());
#ifdef LOG_TIME
        struct timespec last_tm;
        struct timespec curr_tm;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
#endif
        //ALOGD("@%s(%d,%d) usage:%" PRIx64 " frameId:%d deviceId:%d",__FUNCTION__,(int)FrameInfo.width,(int)FrameInfo.height,static_cast<uint64_t>(FrameInfo.usage)
        //, (int)FrameInfo.frameId,deviceId);
        buffer_handle_t handle = nullptr;
        
        android::Rect bounds(android::ui::Size(FrameInfo.width, FrameInfo.height));
        android::status_t err = mapper.importBuffer(
                           FrameInfo.buffer.getNativeHandle(), FrameInfo.width, FrameInfo.height, 1u,
                           HAL_PIXEL_FORMAT_YCrCb_NV12,
                            static_cast<uint64_t>(FrameInfo.usage), FrameInfo.stride, &handle);
        if(err != android::NO_ERROR){
            ALOGE("@%s(%d) importBuffer error",__FUNCTION__,__LINE__);
        }
#ifdef LOG_TIME
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s  importbuffer use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
#endif

        {
            std::unique_lock<std::mutex> lk(mLock);
#ifdef LOG_TIME
            clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
            ALOGD("%s lock use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
#endif
                if(FrameInfo.width == 1920){
                    if(sPushBuffer1920x1080 != nullptr && handle !=nullptr){
                        bool mirror = false;
                        bool isNeedCrop = true;
                        bool isDstNV21 = false;
                        bool is16Align = true;
                        bool isYuyvFormat = true;
                        rga_scale_crop_dstfd(1920,1080,sPushBuffer1920x1080,HAL_PIXEL_FORMAT_YCrCb_420_SP,
                            handle,1920,1080,100,mirror,isNeedCrop,isDstNV21,is16Align,isYuyvFormat);
#ifdef LOG_TIME
                        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
                        ALOGD("%s rga_scale_crop_dstfd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
#endif
                    }else{
                        ALOGE("sPushBuffer1920x1080:%x",(void*)sPushBuffer1920x1080.get());
                        ALOGE("handle:%x",(void*)handle);
                    }
                }else if(FrameInfo.width == 3840){
                    if(sPushBuffer3840x2160 != nullptr && handle !=nullptr){
                        bool mirror = false;
                        bool isNeedCrop = true;
                        bool isDstNV21 = false;
                        bool is16Align = true;
                        bool isYuyvFormat = true;
                        rga_scale_crop_dstfd_withCache(3840,2160,sPushBuffer3840x2160,HAL_PIXEL_FORMAT_YCrCb_420_SP,
                            handle,3840,2160,100,mirror,isNeedCrop,isDstNV21,is16Align,isYuyvFormat);
                    }else{
                        ALOGE("sPushBuffer3840x2160:%x",(void*)sPushBuffer3840x2160.get());
                        ALOGE("handle:%x",(void*)handle);
                    }
                }else{
                    ALOGE("%s Invalid buffer (%dx%d)",__FUNCTION__,(int)FrameInfo.width,(int)FrameInfo.height);
                }
            lk.unlock();
        }
#ifdef LOG_TIME
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s unlock use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
#endif
        ::rockchip::hardware::hdmi::V1_0::FrameInfo info;
        mapper.freeBuffer(handle);
        info.frameId = FrameInfo.frameId;
#ifdef LOG_TIME
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s freeBuffer use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
#endif
        _hidl_cb(info);
        return android::hardware::Void();
    }

    ~FrameWarpperImp(){
        ALOGD("%s",__FUNCTION__);
    }
    FrameWarpperImp(){
        ALOGD("%s",__FUNCTION__);
    }
};
FrameWarpperImp *frameWarpperImp;
#endif
void init()
{
    ALOGD("%s",__FUNCTION__);
#ifdef VIRTUAL_CAMERA
    android::sp<rockchip::hardware::hdmi::V1_0::IHdmi> client = rockchip::hardware::hdmi::V1_0::IHdmi::getService();
    if(client.get()!= nullptr){        
        if (frameWarpperImp != nullptr)
        {
            client->setFrameDecorator(nullptr);
            frameWarpperImp = nullptr;
        }        
        frameWarpperImp = new FrameWarpperImp();
        ALOGD("setFrameDecorator");
        client->setFrameDecorator(frameWarpperImp);
        mProcessingRequest = true;
    }
#endif
}
void deinit(){
    ALOGD("%s",__FUNCTION__);
#ifdef VIRTUAL_CAMERA
    android::sp<rockchip::hardware::hdmi::V1_0::IHdmi> client = rockchip::hardware::hdmi::V1_0::IHdmi::getService();
    ALOGE("client.get():%p",client.get());
    if(client.get()!= nullptr){
        ALOGD("setFrameDecorator nullptr");
        client->setFrameDecorator(nullptr);
        //delete frameWarpperImp ;
        frameWarpperImp = nullptr;
        mProcessingRequest = false;
    }
#endif
    releaseCachedRgaHandler();

}
void set_rgba_1080x1920(uint8_t* rgba,int size)
{
    ALOGD("%s",__FUNCTION__);
    memcpy(rgba_data_1080x1920,rgba,size);
}

void set_nv12_1920x1080(uint8_t* nv12,int size)
{
    ALOGD("%s",__FUNCTION__);
    memcpy(nv12_data_1920x1080,nv12,size);
}

void* get_nv12_1920x1080()
{
    ALOGD("%s",__FUNCTION__);
    return nv12_data_1920x1080;
}



void set_nv12_3840x2160(uint8_t* nv12,int size)
{
    ALOGD("%s",__FUNCTION__);
    memcpy(nv12_data_3840x2160,nv12,size);
}

void set_nv12_1792x3040(uint8_t* nv12,int size)
{
    ALOGD("%s",__FUNCTION__);
    memcpy(nv12_data_1792x3040,nv12,size);
}



void* get_nv12_3840x2160()
{
    ALOGD("%s",__FUNCTION__);
    return nv12_data_3840x2160;
}

#ifdef RGA_PROC

sp<GraphicBuffer> GraphicBuffer_Init(int width, int height,int format) {
    sp<GraphicBuffer> gb(new GraphicBuffer(width,height,format,
                                           GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN));
    if (gb->initCheck()) {
        printf("GraphicBuffer check error : %s\n",strerror(errno));
        return NULL;
    } else
        printf("GraphicBuffer check %s \n","ok");

    return gb;
}

#define RGA_VIRTUAL_W (4096)
#define RGA_VIRTUAL_H (4096)
int rga_fill_in(int index,
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat,int src_sw, int src_sh)
{
    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);

    int ret = 0;
    rga_info_t src,dst;
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;    
    RockchipRga& rkRga(RockchipRga::get());

    im_handle_param_t param;
    param.width = src_width;
    param.height = src_height;
    param.format = src_format;

    memset(&src, 0, sizeof(rga_info_t));
    int src_fd,dst_fd;
    ret = rkRga.RkRgaGetBufferFd(src_buf->handle, &src_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    src.fd = src_fd;
    src_handle = importbuffer_fd(src_fd, &param);
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));

    ret = rkRga.RkRgaGetBufferFd(dst_buf->handle, &dst_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    dst.fd = dst_fd;
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }

    dst_handle = importbuffer_fd(dst_fd, &param);
    ALOGD("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
    dst.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

    if((dst_width > RGA_VIRTUAL_W) || (dst_height > RGA_VIRTUAL_H)){
        ALOGE("(dst_width > RGA_VIRTUAL_W) || (dst_height > RGA_VIRTUAL_H), switch to arm ");
        ret = -1;
        goto END;
    }
    

    rga_set_rect(&src.rect, 0, 0,
                src_width, src_height, src_width,
                src_height, src_format);
    int offset_x,offset_y;
    if(index == 2){
        offset_x = 0;
        offset_y = 0;
    }
    if(index == 3){
        offset_x = src_width;
        offset_y = 0;
    }
    if(index == 4){
        offset_x = 0;
        offset_y = src_height;
    }
    if(index == 5){
        offset_x = src_width;
        offset_y = src_height;
    }
    ALOGD("%s index:%d (%d,%d)",__FUNCTION__,index,offset_x,offset_y);
    if (isDstNV21)
        rga_set_rect(&dst.rect, offset_x, offset_y, src_width, dst_height,
                    dst_width, dst_height,
                    HAL_PIXEL_FORMAT_YCrCb_420_SP);
    else
        rga_set_rect(&dst.rect, offset_x,offset_y,src_width,src_height,
                    dst_width,dst_height,
                    HAL_PIXEL_FORMAT_YCrCb_NV12);

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;
    dst.core = 0x03;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto END;
    }

    END:
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    LOGD("%s use %ldms", __FUNCTION__, get_time_diff_ms(&last_tm,&curr_tm));

    return ret;
}
int rga_scale_crop(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat,int src_sw, int src_sh)
{
    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
    int ret = 0;
    rga_info_t src,dst;
    int zoom_cropW,zoom_cropH;
    int ratio = 0;
    int zoom_top_offset=0,zoom_left_offset=0;
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
    //ALOGE("src_sw:%d,src_sh:%d",src_sw, src_sh);
    RockchipRga& rkRga(RockchipRga::get());

    im_handle_param_t param;
    param.width = src_width;
    param.height = src_height;
    param.format = src_format;

    memset(&src, 0, sizeof(rga_info_t));
    int src_fd,dst_fd;
    ret = rkRga.RkRgaGetBufferFd(src_buf->handle, &src_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s RkRgaGetBufferFd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    src.fd = src_fd;
    src_handle = importbuffer_fd(src_fd, &param);
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));

    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s importbuffer_fd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );

    ret = rkRga.RkRgaGetBufferFd(dst_buf->handle, &dst_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s RkRgaGetBufferFd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    dst.fd = dst_fd;
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }

    dst_handle = importbuffer_fd(dst_fd, &param);
    //ALOGD("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
    dst.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s importbuffer_fd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
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
    if (src_sh != 0)
    {
        zoom_cropH  = src_sh;
    }

    if (src_sw != 0)
    {
        zoom_cropW  = src_sw;
    }
    //ALOGE("zoom_cropW:%d,zoom_cropH:%d",zoom_cropW,zoom_cropH);
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
    dst.core = 0x03;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto END;
    }
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s RkRgaBlit use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    END:

    releasebuffer_handle(src_handle);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s releasebuffer_handle use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    releasebuffer_handle(dst_handle);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s releasebuffer_handle use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    return ret;
}

int rga_scale_crop_withCache(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat,int src_sw, int src_sh)
{
    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
    int ret = 0;
    rga_info_t src,dst;
    int zoom_cropW,zoom_cropH;
    int ratio = 0;
    int zoom_top_offset=0,zoom_left_offset=0;
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
    //ALOGE("src_sw:%d,src_sh:%d",src_sw, src_sh);
    RockchipRga& rkRga(RockchipRga::get());

    im_handle_param_t param;
    param.width = src_width;
    param.height = src_height;
    param.format = src_format;

    memset(&src, 0, sizeof(rga_info_t));
    int src_fd,dst_fd;
    ret = rkRga.RkRgaGetBufferFd(src_buf->handle, &src_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }
    src_handle = kMapRgaHandler[src_fd];
    //ALOGD("%s src [%d]:[%d]",__FUNCTION__,src_fd,src_handle);
    if(src_handle == 0){

        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s RkRgaGetBufferFd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
        src_handle = importbuffer_fd(src_fd, &param);
        kMapRgaHandler[src_fd] = src_handle;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s importbuffer_fd src_fd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    }

    src.fd = src_fd;
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));

    ret = rkRga.RkRgaGetBufferFd(dst_buf->handle, &dst_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    dst.fd = dst_fd;    
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }
    dst_handle = kMapRgaHandler[dst_fd];
    if(dst_handle == 0){
        dst_handle = importbuffer_fd(dst_fd, &param);
        kMapRgaHandler[dst_fd] = dst_handle;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s importbuffer_fd dst_fd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    }
    //ALOGD("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
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
    if (src_sh != 0)
    {
        zoom_cropH  = src_sh;
    }

    if (src_sw != 0)
    {
        zoom_cropW  = src_sw;
    }
    //ALOGE("zoom_cropW:%d,zoom_cropH:%d",zoom_cropW,zoom_cropH);
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
    dst.core = 0x03;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto END;
    }
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    END:
    return ret;
}
int rga_scale_crop_rgba8888(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,sp<GraphicBuffer> dst_buf,
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
    int src_fd,dst_fd;
    ret = rkRga.RkRgaGetBufferFd(src_buf->handle, &src_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    src.fd = src_fd;
    src_handle = importbuffer_fd(src_fd, &param);
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));

    ret = rkRga.RkRgaGetBufferFd(dst_buf->handle, &dst_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    dst.fd = dst_fd;
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }

    dst_handle = importbuffer_fd(dst_fd, &param);
    ALOGD("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
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

    ALOGE("zoom_cropW:%d,zoom_cropH:%d",zoom_cropW,zoom_cropH);
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

    src.rotation = HAL_TRANSFORM_ROT_90;
    //TODO:sina,cosa,scale_mode,render_mode

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;
    dst.core = 0x03;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto END;
    }

    END:
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    return ret;
}

int rga_scale_crop_dstfd(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,buffer_handle_t dst_buf_handle,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat)
{
    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
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
    int src_fd,dst_fd;
    ret = rkRga.RkRgaGetBufferFd(src_buf->handle, &src_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    src.fd = src_fd;
    src_handle = importbuffer_fd(src_fd, &param);
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));

    ret = rkRga.RkRgaGetBufferFd(dst_buf_handle, &dst_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    dst.fd = dst_fd;
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }

    dst_handle = importbuffer_fd(dst_fd, &param);
    //ALOGD("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
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
        src.rotation = DRM_RGA_TRANSFORM_FLIP_V;
    //TODO:sina,cosa,scale_mode,render_mode

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;
    dst.core = 0x03;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto END;
    }

    END:
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    //LOGD("%s use %ldms", __FUNCTION__, get_time_diff_ms(&last_tm,&curr_tm));

    return ret;
}
int rga_scale_crop_dstfd_withCache(
		int src_width, int src_height,
		sp<GraphicBuffer> src_buf, int src_format,buffer_handle_t dst_buf_handle,
		int dst_width, int dst_height,
		int zoom_val, bool mirror, bool isNeedCrop,
		bool isDstNV21, bool is16Align, bool isYuyvFormat)
{
    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
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
    int src_fd,dst_fd;
    ret = rkRga.RkRgaGetBufferFd(src_buf->handle, &src_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    src.fd = src_fd;
    src_handle = kMapRgaHandler[src_fd];
    ALOGD("%s src [%d]:[%d]",__FUNCTION__,src_fd,src_handle);
    if(src_handle == 0){

        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s RkRgaGetBufferFd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
        src_handle = importbuffer_fd(src_fd, &param);
        kMapRgaHandler[src_fd] = src_handle;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s importbuffer_fd src_fd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    }
    src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);
    memset(&dst, 0, sizeof(rga_info_t));

    ret = rkRga.RkRgaGetBufferFd(dst_buf_handle, &dst_fd);
    if (ret){
        ALOGE("%s: get buffer fd fail: %s, buffer_handle_t=%p",__FUNCTION__, strerror(errno), (void*)(src_buf->handle));
        return ret;
    }

    dst.fd = dst_fd;
    param.width = dst_width;
    param.height = dst_height;
    if (isDstNV21){
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }
    dst_handle = kMapRgaHandler[dst_fd];
    if(dst_handle == 0){
        dst_handle = importbuffer_fd(dst_fd, &param);
        kMapRgaHandler[dst_fd] = dst_handle;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
        ALOGD("%s importbuffer_fd dst_fd use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );
    }
    //ALOGD("@%s, dst fd:%d,width:%d,height:%d,isDstNV21:%d",__FUNCTION__,dst.fd,param.width,param.height,isDstNV21);
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
        src.rotation = DRM_RGA_TRANSFORM_FLIP_V;
    //TODO:sina,cosa,scale_mode,render_mode

    src.handle = src_handle;
    src.fd = 0;
    dst.handle = dst_handle;
    dst.fd = 0;
    dst.core = 0x03;
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if (ret) {
        ALOGE("%s:rga blit failed %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto END;
    }

    END:
    // releasebuffer_handle(src_handle);
    // releasebuffer_handle(dst_handle);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    ALOGD("%s use %ldms", __FUNCTION__, get_time_diff_ms(&last_tm,&curr_tm));

    return ret;
}

#endif

void rga_proc(){
    ALOGD("%s",__FUNCTION__);
#ifdef RGA_PROC
    int ret;
    bool mirror = false;
    bool isNeedCrop = false;
    bool isDstNV21 = false;
    bool is16Align = true;
    bool isYuyvFormat = true;
    int src_width = 1080;
    int src_height = 1920;
    int dst_width = 1920;
    int dst_height = 1080;
    sp<GraphicBuffer> src_buf;
    sp<GraphicBuffer> dst_buf;
    src_buf = GraphicBuffer_Init(src_width, src_height, HAL_PIXEL_FORMAT_RGBA_8888);
    dst_buf = GraphicBuffer_Init(dst_width, dst_height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    char* buf = NULL;
    ret = src_buf->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)&buf);
    if (ret) {
        ALOGE("lock buffer error : %s\n",strerror(errno));
        return ;
    } else
        ALOGD("lock buffer %s \n","ok");

    memcpy(buf, rgba_data_1080x1920,(int) 1080*1920*4);

    ret = src_buf->unlock();
    if (ret) {
        ALOGE("unlock buffer error : %s\n",strerror(errno));
        return ;
    } else
        ALOGD("unlock buffer %s \n","ok");

    rga_scale_crop_rgba8888(src_width,src_height,src_buf,HAL_PIXEL_FORMAT_RGBA_8888,
    dst_buf,dst_width,dst_height,100,mirror,isNeedCrop,isDstNV21,is16Align,isYuyvFormat);
#if 1
    char* outbuf = NULL;
    if (dst_buf != NULL) {
        ret = dst_buf->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN, (void**)&outbuf);
        FILE* fp =NULL;
        char filename[128];
        filename[0] = 0x00;
        sprintf(filename, "/sdcard/Download/camera_dump_%dx%d.yuv",
                dst_width, dst_height);
        fp = fopen(filename, "wb+");
        if (fp != NULL) {
            fwrite(outbuf, 1, dst_width*dst_height*1.5, fp);
            fclose(fp);
            ALOGI("Write success YUV data to %s",filename);
        } else {
            ALOGE("Create %s failed(%ld, %s)",filename,(long)fp, strerror(errno));
        }
        ret = dst_buf->unlock();
    }
#endif

#endif
}
void rga_proc(JNIEnv *env, jobject hardware_buffer){

    struct timespec last_tm;
    struct timespec curr_tm;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);

    AHardwareBuffer *hardwareBuffer = AHardwareBuffer_fromHardwareBuffer(env, hardware_buffer);
    GraphicBuffer* gbuffer = GraphicBuffer::fromAHardwareBuffer(hardwareBuffer);

    int width = gbuffer->getWidth();
    int height = gbuffer->getHeight();
    int layers = gbuffer->getLayerCount();
    int format = uint32_t(gbuffer->getPixelFormat());
    int usage = gbuffer->getUsage();
    int stride = gbuffer->getStride();
    // ALOGD("%s width:%d,height:%d,layers:%d,format:%d,usage:%d,stride:%d",__FUNCTION__,
    // width,height,layers,format,usage,stride);
    if(sPushBuffer1920x1080 == nullptr){
        sPushBuffer1920x1080 = GraphicBuffer_Init(1920, 1080, HAL_PIXEL_FORMAT_YCrCb_420_SP);
    }
    if(sPushBuffer3840x2160 == nullptr){
        sPushBuffer3840x2160 = GraphicBuffer_Init(3840, 2160, HAL_PIXEL_FORMAT_YCrCb_420_SP);
    }
    //sp<GraphicBuffer> dst_buf = GraphicBuffer_Init(width, height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    { //camera buffer copy to sPushBuffer1920x1080 for virtual camera MAX 1920x1080 
        int dst_width = 1920;
        int dst_height = 1080;
        bool mirror = false;
        bool isNeedCrop = true;
        bool isDstNV21 = false;
        bool is16Align = true;
        bool isYuyvFormat = true;
        //ALOGD("rga_scale_crop begin.");
         std::unique_lock<std::mutex> lk(mLock);
        rga_scale_crop_withCache(width,height,gbuffer,HAL_PIXEL_FORMAT_YCrCb_420_SP,
        sPushBuffer1920x1080,dst_width,dst_height,100,mirror,isNeedCrop,isDstNV21,is16Align,isYuyvFormat);
         lk.unlock();
    }
    clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
    //ALOGD("%s use %ldms", __FUNCTION__,get_time_diff_ms(&last_tm,&curr_tm) );

    // { //camera buffer copy to sPushBuffer3840x2160 for virtual camera MAX 4096x3072
    //     int dst_width = 3840;
    //     int dst_height = 2160;
    //     bool mirror = false;
    //     bool isNeedCrop = true;
    //     bool isDstNV21 = false;
    //     bool is16Align = true;
    //     bool isYuyvFormat = true;
    //     //ALOGD("rga_scale_crop begin.");
    //     std::unique_lock<std::mutex> lk(mLock);
    //     rga_scale_crop(width,height,gbuffer,HAL_PIXEL_FORMAT_YCrCb_420_SP,
    //     sPushBuffer3840x2160,dst_width,dst_height,100,mirror,isNeedCrop,isDstNV21,is16Align,isYuyvFormat);
    //     lk.unlock();
    // }
    signalRequestDone();
    //ALOGD("rga_scale_crop done.");
#if 0
    char* outbuf = NULL;
    if (sPushBuffer1920x1080 != NULL) {
        static int sNum = 0;
        int ret = sPushBuffer1920x1080->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN, (void**)&outbuf);
        FILE* fp =NULL;
        char filename[128];
        filename[0] = 0x00;
        sprintf(filename, "/sdcard/Download/camera_dump_%d_%dx%d.yuv",sNum++,
                dst_width, dst_height);
        fp = fopen(filename, "wb+");
        if (fp != NULL) {
            fwrite(outbuf, 1, dst_width*dst_height*1.5, fp);
            fclose(fp);
            ALOGI("Write success YUV data to %s",filename);
        } else {
            ALOGE("Create %s failed(%ld, %s)",filename,(long)fp, strerror(errno));
        }
        ret = sPushBuffer1920x1080->unlock();
    }
#endif
}

void rga_proc_fill_in(JNIEnv *env, jobject hardware_buffer,int index){
    AHardwareBuffer *hardwareBuffer = AHardwareBuffer_fromHardwareBuffer(env, hardware_buffer);
    GraphicBuffer* gbuffer = GraphicBuffer::fromAHardwareBuffer(hardwareBuffer);

    int width = gbuffer->getWidth();
    int height = gbuffer->getHeight();
    int layers = gbuffer->getLayerCount();
    int format = uint32_t(gbuffer->getPixelFormat());
    int usage = gbuffer->getUsage();
    int stride = gbuffer->getStride();
    // ALOGD("%s width:%d,height:%d,layers:%d,format:%d,usage:%d,stride:%d",__FUNCTION__,
    // width,height,layers,format,usage,stride);

    if(sPushBuffer3840x2160 == nullptr){
        sPushBuffer3840x2160 = GraphicBuffer_Init(3840, 2160, HAL_PIXEL_FORMAT_YCrCb_420_SP);
    }
    
    { //camera buffer copy to sPushBuffer3840x2160 for virtual camera MAX 4096x3072
        int dst_width = 3840;
        int dst_height = 2160;
        bool mirror = false;
        bool isNeedCrop = true;
        bool isDstNV21 = false;
        bool is16Align = true;
        bool isYuyvFormat = true;
        rga_fill_in(index,width,height,gbuffer,HAL_PIXEL_FORMAT_YCrCb_420_SP,
        sPushBuffer3840x2160,dst_width,dst_height,100,mirror,isNeedCrop,isDstNV21,is16Align,isYuyvFormat);
    }
}
void get_mipi_status(){
    ALOGD("%s",__FUNCTION__);
#ifdef VIRTUAL_CAMERA
    android::sp<rockchip::hardware::hdmi::V1_0::IHdmi> client = rockchip::hardware::hdmi::V1_0::IHdmi::getService();
    if(client.get()!= nullptr){
        rockchip::hardware::hdmi::V1_0::HdmiStatus status;
        ALOGD("getMipiStatus");
        client->getMipiStatus([&]( ::rockchip::hardware::hdmi::V1_0::HdmiStatus _hdmiStatus){
            ALOGD("getMipiStatus callback");
            status = _hdmiStatus;
        });
        ALOGD("status:%d,width:%d,height:%d, %.2f fps",(int)status.status,(int)status.width,(int)status.height,status.fps);
    }
#endif
}

void get_hdmirx_status(){
    ALOGD("%s",__FUNCTION__);
#ifdef VIRTUAL_CAMERA
    android::sp<rockchip::hardware::hdmi::V1_0::IHdmi> client = rockchip::hardware::hdmi::V1_0::IHdmi::getService();
    if(client.get()!= nullptr){
        rockchip::hardware::hdmi::V1_0::HdmiStatus status;
        ALOGD("getHdmiRxStatus");
        client->getHdmiRxStatus([&]( ::rockchip::hardware::hdmi::V1_0::HdmiStatus _hdmiStatus){
            ALOGD("getHdmiRxStatus callback");
            status = _hdmiStatus;
        });
        ALOGD("status:%d,width:%d,height:%d, %.2f fps",(int)status.status,(int)status.width,(int)status.height,status.fps);
    }
#endif
}

#ifdef VIRTUAL_CAMERA
class HdmiCallback : public rockchip::hardware::hdmi::V1_0::IHdmiCallback {
    struct bridge_callback* mCallback;
public:
    HdmiCallback(struct bridge_callback *callback){
        ALOGD("%s",__FUNCTION__);
        mCallback = callback;
    }
    android::hardware::Return<void> onConnect(const android::hardware::hidl_string& deviceId) {
        ALOGD("%s",__FUNCTION__);
        mCallback->connect(deviceId.c_str());
        return android::hardware::Void();
    }
    android::hardware::Return<void> onFormatChange(const android::hardware::hidl_string& deviceId,uint32_t width,uint32_t height){
        ALOGD("%s",__FUNCTION__);
        mCallback->format_change(deviceId.c_str(),width,height);
        return android::hardware::Void();
    }
    android::hardware::Return<void> onDisconnect(const android::hardware::hidl_string& deviceId) {
        ALOGD("%s",__FUNCTION__);
        mCallback->disconnect(deviceId.c_str());
        return android::hardware::Void();
    }
};
HdmiCallback *hdmiCallback;
#endif

void set_hdmi_callback(struct bridge_callback *callback){
    ALOGD("%s",__FUNCTION__);
#ifdef VIRTUAL_CAMERA
    android::sp<rockchip::hardware::hdmi::V1_0::IHdmi> client = rockchip::hardware::hdmi::V1_0::IHdmi::getService();
    if(client.get()!= nullptr){
        if (hdmiCallback != nullptr)
        {
            ALOGD("unregisterListener");
            client->unregisterListener(hdmiCallback);
            hdmiCallback = nullptr;
			hdmiCallback = new HdmiCallback(callback);
			ALOGD("registerListener");
			client->registerListener(hdmiCallback);
        }else{
		    ALOGD("set callback nullptr");
			client->registerListener(nullptr);
		}

    }
#endif
}

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


#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-video-producer"

#include "rockchip/utils/drmdebug.h"
#include "rockchip/producer/drmvideoproducer.h"
#include <utils/Trace.h>

#include <dlfcn.h>
#include <fcntl.h>
#ifdef USE_LIBPQ_HWPQ
#include <hardware/hwcomposer_defs.h>
#include <Pq.h>
#endif
namespace android {

#if defined(__arm64__) || defined(__aarch64__)
#define RK_LIB_VT_PATH "/vendor/lib64/librkvt.so"
#else
#define RK_LIB_VT_PATH "/vendor/lib/librkvt.so"
#endif

// Next Hdr
typedef int (*rk_vt_open_func)(void);
typedef int (*rk_vt_close_func)(int fd);
typedef int (*rk_vt_connect_func)(int fd, int tunnel_id, int role);
typedef int (*rk_vt_disconnect_func)(int fd, int tunnel_id, int role);
typedef int (*rk_vt_acquire_buffer_func)(int fd,
                                         int tunnel_id,
                                         int timeout_ms,
                                         vt_buffer_t **buffer,
                                         int64_t *expected_present_time);
typedef int (*rk_vt_release_buffer_func)(int fd, int tunnel_id, vt_buffer_t *buffer);

struct rkvt_ops {
    int (*rk_vt_open)(void);
    int (*rk_vt_close)(int fd);
    int (*rk_vt_connect)(int fd, int tunnel_id, int role);
    int (*rk_vt_disconnect)(int fd, int tunnel_id, int role);
    int (*rk_vt_acquire_buffer)(int fd,
                                     int tunnel_id,
                                     int timeout_ms,
                                     vt_buffer_t **buffer,
                                     int64_t *expected_present_time);
    int (*rk_vt_release_buffer)(int fd, int tunnel_id, vt_buffer_t *buffer);
};

static struct rkvt_ops g_rkvt_ops;
static void * g_rkvt_lib_handle = NULL;

DrmVideoProducer::DrmVideoProducer()
  : Worker("DVPWorker", HAL_PRIORITY_URGENT_DISPLAY),
    bInit_(false),
    iTunnelFd_(-1){

    }

DrmVideoProducer::~DrmVideoProducer(){
  std::lock_guard<std::mutex> lock(mtx_);

  if(iTunnelFd_ > 0){
    int ret = g_rkvt_ops.rk_vt_close(iTunnelFd_);
    if (ret < 0) {
      HWC2_ALOGE("rk_vt_close fail ret=%d", ret);
    }
  }
}

// Init video tunel.
int DrmVideoProducer::Init(){
  std::lock_guard<std::mutex> lock(mtx_);

  if(bInit_)
    return 0;

  if(InitLibHandle()){
    HWC2_ALOGE("init fail, disable VideoProducer function.");
    return -1;
  }

  if(iTunnelFd_ < 0){
    iTunnelFd_ = g_rkvt_ops.rk_vt_open();
    if(iTunnelFd_ < 0){
        HWC2_ALOGE("rk_vt_open fail ret=%d", iTunnelFd_);
        return -1;
    }
  }

  HWC2_ALOGI("Init success fd=%d", iTunnelFd_);
  bInit_ = true;
  return InitWorker();
}

int DrmVideoProducer::InitLibHandle(){
  g_rkvt_lib_handle = dlopen(RK_LIB_VT_PATH, RTLD_NOW);
  if (g_rkvt_lib_handle == NULL) {
      HWC2_ALOGE("cat not open %s\n", RK_LIB_VT_PATH);
      return -1;
  }else{
      g_rkvt_ops.rk_vt_open = (rk_vt_open_func)dlsym(g_rkvt_lib_handle, "rk_vt_open");
      g_rkvt_ops.rk_vt_close = (rk_vt_close_func)dlsym(g_rkvt_lib_handle, "rk_vt_close");
      g_rkvt_ops.rk_vt_connect = (rk_vt_connect_func)dlsym(g_rkvt_lib_handle, "rk_vt_connect");
      g_rkvt_ops.rk_vt_disconnect = (rk_vt_disconnect_func)dlsym(g_rkvt_lib_handle, "rk_vt_disconnect");
      g_rkvt_ops.rk_vt_acquire_buffer = (rk_vt_acquire_buffer_func)dlsym(g_rkvt_lib_handle, "rk_vt_acquire_buffer");
      g_rkvt_ops.rk_vt_release_buffer = (rk_vt_release_buffer_func)dlsym(g_rkvt_lib_handle, "rk_vt_release_buffer");

      if(g_rkvt_ops.rk_vt_open == NULL||
         g_rkvt_ops.rk_vt_close == NULL ||
         g_rkvt_ops.rk_vt_connect == NULL ||
         g_rkvt_ops.rk_vt_disconnect == NULL ||
         g_rkvt_ops.rk_vt_acquire_buffer == NULL ||
         g_rkvt_ops.rk_vt_release_buffer == NULL){
        HWC2_ALOGD_IF_ERR("cat not dlsym open=%p close=%p connect=%p disconnect=%p acquire_buffer=%p release_buffer=%p\n",
                          g_rkvt_ops.rk_vt_open,
                          g_rkvt_ops.rk_vt_close,
                          g_rkvt_ops.rk_vt_connect,
                          g_rkvt_ops.rk_vt_disconnect,
                          g_rkvt_ops.rk_vt_acquire_buffer,
                          g_rkvt_ops.rk_vt_release_buffer);
        return -1;
      }
  }
  HWC2_ALOGI("InitLibHandle %s success!\n", RK_LIB_VT_PATH);
  return 0;
}


bool DrmVideoProducer::IsValid(){
  return bInit_;
}

// Create tunnel connection.
int DrmVideoProducer::CreateConnection(int display_id, int tunnel_id, android_dataspace_t dataspace ){
  std::lock_guard<std::mutex> lock(mtx_);

  if(!bInit_){
    HWC2_ALOGE(" fail, display-id=%d bInit_=%d tunnel-fd=%d", display_id, bInit_, iTunnelFd_);
    return -1;
  }

  if(mMapCtx_.count(tunnel_id)){
    std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
    ctx->iDataSpace_ = dataspace;
    if(!ctx->AddConnRef(display_id)){
      HWC2_ALOGI("display-id=%d tunnel_id=%d success, connections size=%d", display_id, tunnel_id, ctx->ConnectionCnt());
    }
    return 0;
  }

  int ret = g_rkvt_ops.rk_vt_connect(iTunnelFd_, tunnel_id, RKVT_ROLE_CONSUMER);
  if (ret < 0) {
      return ret;
  }

  HWC2_ALOGI("display-id=%d tunnel_id=%d success", display_id, tunnel_id);
  mMapCtx_[tunnel_id] = std::make_shared<VpContext>(tunnel_id);
  std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
  ctx->AddConnRef(display_id);
  ctx->iDataSpace_ = dataspace;
  Signal();
  return 0;
}

// Destory Connection
int DrmVideoProducer::DestoryConnection(int display_id, int tunnel_id){
  std::lock_guard<std::mutex> lock(mtx_);

  if(!bInit_){
    HWC2_ALOGE("fail, display=%d bInit_=%d tunnel-fd=%d", display_id, bInit_, iTunnelFd_);
    return -1;
  }

  if(mMapCtx_.count(tunnel_id) == 0){
    HWC2_ALOGE("display_id=%d mMapCtx_ can't find tunnel_id=%d", display_id, tunnel_id);
    return -1;
  }

  std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
  ctx->ReleaseConnRef(display_id);
  if(ctx->ConnectionCnt() > 0){
    HWC2_ALOGD_IF_DEBUG("display=%d tunnel_id=%d connection cnt=%d, no need to destory. ",
                         display_id, tunnel_id, ctx->ConnectionCnt());
    return 0;
  }
  HWC2_ALOGD_IF_DEBUG("display=%d tunnel_id=%d connection cnt=%d, going to destory. ",
                        display_id, tunnel_id, ctx->ConnectionCnt());
  return 0;
}

#ifdef USE_LIBPQ_HWPQ

static bool IsYuvFormat(int format, uint32_t fourcc_format){

  switch(fourcc_format){
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV12_10:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YVU422:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_YVU444:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_YUV420_8BIT:
    case DRM_FORMAT_YUV420_10BIT:
    case DRM_FORMAT_NV24:
    case DRM_FORMAT_NV42:
    case DRM_FORMAT_NV15:
    case DRM_FORMAT_NV20:
    case DRM_FORMAT_NV30:
    case DRM_FORMAT_Y210:
    case DRM_FORMAT_VUY888:
    case DRM_FORMAT_VUY101010:
      return true;
    default:
      break;
  }

  switch(format){
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
    case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP_10:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP_10:
    case HAL_PIXEL_FORMAT_YCBCR_422_I:
    case HAL_PIXEL_FORMAT_YUV420_8BIT_I:
    case HAL_PIXEL_FORMAT_YUV420_10BIT_I:
    case HAL_PIXEL_FORMAT_Y210:
      return true;
    default:
      return false;
  }
}

static bool Is10bitYuv(int format,uint32_t fourcc_format){
  switch(fourcc_format){
    case DRM_FORMAT_NV12_10:
    case DRM_FORMAT_YUV420_10BIT:
    case DRM_FORMAT_VUY101010:
    case DRM_FORMAT_Y210:
    case DRM_FORMAT_NV30:
    case DRM_FORMAT_NV20:
    case DRM_FORMAT_NV15:
      return true;
    default:
      break;
  }

  switch(format){
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP_10:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP_10:
    case HAL_PIXEL_FORMAT_YUV420_10BIT_I:
      return true;
    default:
      return false;
  }
}

std::shared_ptr<DrmBuffer> DrmVideoProducer::DoHwPq(std::shared_ptr<VpContext> ctx, std::shared_ptr<DrmBuffer> buffer){

  int left,top,right,bottom;
  buffer->GetCrop(&left,&top,&right,&bottom);

  int act_w = right - left;
  int act_h = bottom - top;

  int ret = 0;
  // 0. Check if HwPq is Ready
  if(hwpq_ == NULL){
    hwpq_ = std::make_shared<Pq>();
    if(hwpq_ != NULL){
      ret = hwpq_->Init(PQ_VERSION);
      if(ret!=0){
        HWC2_ALOGE("Pq module Init Failed, ret=%d", ret);
        hwpq_ = NULL;
      }
    }
  }
  if(hwpq_ == NULL){
    HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" Pq module not ready! Pq::Get() return NULL",
      ctx->GetTunnelId(), buffer->GetExternalId());
    return NULL;
  }
  // 1. 初始化Ctx
  // 2. Fill buffer Info
  HwPqImageInfo src;
  src.mBufferInfo_.iFd_     = buffer->GetFd();
  src.mBufferInfo_.iWidth_  = buffer->GetWidth();
  src.mBufferInfo_.iHeight_ = buffer->GetHeight();
  src.mBufferInfo_.iFormat_ = buffer->GetFormat();
  src.mBufferInfo_.iStride_ = buffer->GetStride();
  src.mBufferInfo_.iHeightStride_ = buffer->GetHeightStride();
  src.mBufferInfo_.uBufferId_ = buffer->GetBufferId();
  src.mBufferInfo_.uDataSpace_ = (uint64_t)ctx->iDataSpace_;
    
  src.mCrop_.iLeft_  = (int)left;
  src.mCrop_.iTop_   = (int)top;
  src.mCrop_.iRight_ = (int)right;
  src.mCrop_.iBottom_= (int)bottom;
  // src.mVirtualAddress_ = buffer->Lock();

  HwPqImageInfo dst;
  dst.mBufferInfo_.iFd_ = -1;
  std::shared_ptr<DrmBuffer> dst_buffer;

  // 3. Set buffer Info
  bool needAllocNewBuffer = false;
  ret = hwpq_->SetHwPqSrcImage(src, dst, &needAllocNewBuffer);
  if(ret){
    HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" Pq SetSrcImage fail ret = %d",
        ctx->GetTunnelId(), buffer->GetExternalId(), ret);
    // buffer->Unlock();
    return NULL;
  }

  //判断是否需要新buffer的条件：宽高format存在差异
  if(needAllocNewBuffer){
    dst_buffer = std::make_shared<DrmBuffer>(dst.mBufferInfo_.iWidth_,
                                              dst.mBufferInfo_.iHeight_,
                                              dst.mBufferInfo_.iFormat_,
                                              RK_GRALLOC_USAGE_STRIDE_ALIGN_64 |
                                              MALI_GRALLOC_USAGE_NO_AFBC,
                                              "HWPQ-target");
    ret = dst_buffer->Init();
    if(ret){
          HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" Create DstBuffer fail! ret = %d",
        ctx->GetTunnelId(), buffer->GetExternalId(), ret);
      return NULL;
    }
    //更新buffer参数
    dst.mBufferInfo_.iFd_     = dst_buffer->GetFd();
    dst.mBufferInfo_.iWidth_  = dst_buffer->GetWidth();
    dst.mBufferInfo_.iHeight_ = dst_buffer->GetHeight();
    dst.mBufferInfo_.iFormat_ = dst_buffer->GetFormat();
    dst.mBufferInfo_.iStride_ = dst_buffer->GetStride();
    dst.mBufferInfo_.iHeightStride_ = dst_buffer->GetStride();
    dst.mBufferInfo_.uBufferId_ = dst_buffer->GetBufferId();
  }else{
    dst.mBufferInfo_ = src.mBufferInfo_;
  }

  if(dst.mBufferInfo_.iFd_<0){
    HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" dst buffer not set, dst.mBufferInfo_.iFd_=%d",
    ctx->GetTunnelId(), buffer->GetExternalId(), dst.mBufferInfo_.iFd_);
    return NULL;
  }

  //Alloc rk_hwpq_reg变量
  if(dst_buffer == NULL)
    dst.mRkHwpqReg_ = buffer->GetHwPqRegs().get();
  else
    dst.mRkHwpqReg_ = dst_buffer->GetHwPqRegs().get();

  //设置目标属性
  ret = hwpq_->SetHwPqDstImage(dst);
  if(ret){
    HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" Pq SetHwPqDstImage fail, ret = %d",
        ctx->GetTunnelId(), buffer->GetExternalId(), ret);
    return NULL;
  }
  //pq前需要等待AcquireFence
  ret = ctx->WaitAcquireFence(buffer->GetExternalId(),3000);
  if(ret){
    HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" buffer not signaled after 3000ms, ret = %d",
        ctx->GetTunnelId(), buffer->GetExternalId(), ret);
    return NULL;
  }
  //执行PQ
  int output_fence = -1;
  ret = hwpq_->RunHwPqAsync(&output_fence);
  if(ret){
    HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" .hwPq Run fail ret = %d",
        ctx->GetTunnelId(), buffer->GetExternalId(), ret);
    return NULL;
  }
  ctx->SetAcquireFence(buffer->GetExternalId(),output_fence);

  if(dst_buffer){
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.dump", value, "false");
    if(!strcmp(value, "true")){
      ctx->WaitAcquireFence(buffer->GetExternalId(),3000);
      dst_buffer->DumpData();
    }
    //Update Crop form Query Result
    dst_buffer->SetCrop(dst.mCrop_.iLeft_, dst.mCrop_.iTop_, dst.mCrop_.iRight_, dst.mCrop_.iBottom_);
    return dst_buffer;
  }else{
    return NULL;
  } 
}
#endif

// Get Last video buffer
std::shared_ptr<DrmBuffer> DrmVideoProducer::AcquireBuffer(int display_id,
                                                           int tunnel_id,
                                                           vt_rect_t *dis_rect,
                                                           int timeout_ms,
                                                           bool wait_fence
                                                           ){
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(mtx_);

  if(!bInit_){
    HWC2_ALOGE("fail, display=%d bInit_=%d tunnel-fd=%d", display_id, bInit_, iTunnelFd_);
    return NULL;
  }

  if(!mMapCtx_.count(tunnel_id)){
    HWC2_ALOGE("display=%d mMapCtx_ can't find tunnel_id=%d", display_id, tunnel_id);
    return NULL;
  }

  std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];

  std::shared_ptr<DrmBuffer> acquired_buffer=NULL;

  //if there is any buffer in list
  if(ctx->lBuffer_.size()>0){
    for(auto &b:ctx->lBuffer_){
      HWC2_ALOGD_IF_VERBOSE("tunnel_id=%d, display=%d, lBuffer_ have buffer:%" PRIu64 ,
                            tunnel_id, display_id, b->GetExternalId());
    }

    acquired_buffer = ctx->lBuffer_.back();

#ifdef USE_LIBPQ_HWPQ
    if(acquired_buffer->HasHwPqRegs()){
      HWC2_ALOGD_IF_VERBOSE("tunnel_id=%d, display=%d, Force wait fence for HWPQ buffer");
      wait_fence = true;
    }
#endif

    //if wait fence is acquired
    if(wait_fence){
      int ret = 0;
      //if we have two buffer, try new one first.
      if(ctx->lBuffer_.size()==2){
        //check if new buffer is signaled
        ret = ctx->WaitAcquireFence(acquired_buffer->GetExternalId(),0);
        //if not, use old buffer.
        if(ret)
          acquired_buffer = ctx->lBuffer_.front();
      }
      //wait acquire fence
      ret = ctx->WaitAcquireFence(acquired_buffer->GetExternalId(),3000);
      if(ret){
        HWC2_ALOGE("display-id=%d tunnel_id=%d Buffer 0x%" PRIx64" is not signaled after 3000ms!!!",display_id,tunnel_id,acquired_buffer->GetExternalId());
        return NULL;
      }
    }
    HWC2_ALOGD_IF_VERBOSE("tunnel_id=%d, display=%d, acquired buffer:%" PRIu64 " ,add Release fence Reference",
                          tunnel_id, display_id, acquired_buffer->GetExternalId());
    //release fence add refCount
    ctx->AddReleaseFenceRefCnt(display_id,acquired_buffer->GetExternalId());

    return acquired_buffer;
  }else{
    return NULL;
  }
}

// Release video buffer
int DrmVideoProducer::SignalReleaseFence(int display_id, int tunnel_id, uint64_t buffer_id){
  ATRACE_CALL();

  std::lock_guard<std::mutex> lock(mtx_);
  if(!bInit_){
    HWC2_ALOGE(" fail, display=%d bInit_=%d tunnel_id=%d",
              display_id, bInit_, tunnel_id);
    return -1;
  }

  if(!mMapCtx_.count(tunnel_id)){
    HWC2_ALOGE("display=%d mMapCtx_ can't find tunnel_id=%d", display_id, tunnel_id);
    return -1;
  }

  std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
  return ctx->SignalReleaseFence(display_id, buffer_id);;
}

void DrmVideoProducer::Routine(){
  ATRACE_CALL();
  int ret;

  //Check tunnel status。
  if(!bInit_ || mMapCtx_.size()==0){
    Lock();
    WaitForSignalOrExitLocked(-1);
    Unlock();
  }

  std::vector<int> tunnelShouldRelease;
  std::vector<int> tunnelShouldAcquire;

  std::unique_lock<std::mutex> lock(mtx_);


  //打印释放失败的tunnel，以警告可能存在的内存泄露
  printPendingReleaseTunnel();

  //检查当前tunnel状态
  for(auto &map_ctx:mMapCtx_){
    //如果引用计数为0，则释放tunnel
    if(map_ctx.second->ConnectionCnt()==0){
      tunnelShouldRelease.push_back(map_ctx.first);
    }else{
      tunnelShouldAcquire.push_back(map_ctx.first);
    }
  }


  //释放需要释放的tunnel
  for(auto tunnel_id:tunnelShouldRelease){
    HWC2_ALOGD_IF_DEBUG("Tunnel %d is not connected, release tunnel",tunnel_id);
    std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
    while(ctx->lBuffer_.size()>0){
      //Signal Release Fence 
      uint64_t buffer_id = ctx->lBuffer_.front()->GetExternalId();
      ctx->SignalReleaseFence(-1, buffer_id);

      ctx->lBuffer_.pop_front();
    }
    int ret = g_rkvt_ops.rk_vt_disconnect(iTunnelFd_, ctx->GetTunnelId(), RKVT_ROLE_CONSUMER);
    if (ret < 0) {
      HWC2_ALOGE("rk_vt_disconnect fail TunnelId=%d, ret=%d.", ctx->GetTunnelId(), ret);
      mPendingReleaseTunnel_.push_back(tunnel_id);
      mMapCtx_.erase(tunnel_id);
      continue;
    }
    mMapCtx_.erase(tunnel_id);
    HWC2_ALOGD_IF_DEBUG("tunnel_id=%d disconnect success! ", tunnel_id);
  }

  if(tunnelShouldAcquire.size()>0){
    //如果上次获取失败，等待10ms防止死循环
    if(!bLastAcquireSucceed){
      usleep(10000);
    }
    //reset bLastAcquireSucceed flag
    bLastAcquireSucceed=false;
  }

  //当前活动的tunnel，进行acquire buffer操作
  for(auto tunnel_id:tunnelShouldAcquire){
    std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];

    // 请求最新帧
    vt_buffer_t *acquire_buffer = NULL;
    int64_t queue_timestamp = 0;
    uint64_t buffer_id = 0;
    int acquire_fence_fd=-1;

    //固定等待时间为50ms，如果存在fps<20的情况再另做修改
    int waitTime_ms = 50;

    //解开锁等待vtunnl送buffer
    lock.unlock();
    ret = g_rkvt_ops.rk_vt_acquire_buffer(iTunnelFd_, ctx->GetTunnelId(), waitTime_ms, &acquire_buffer, &queue_timestamp);
    //不管是否获取成功均需要重新锁定不然有重复unlock风险
    lock.lock();
    if(ret != 0){
      HWC2_ALOGE("tunnel_id=%d rk_vt_acquire_buffer failed! ret:%d", ctx->GetTunnelId(), ret);
      continue;
    }
    bLastAcquireSucceed = true;
    buffer_id = acquire_buffer->buffer_id;
    acquire_fence_fd = acquire_buffer->rdy_render_fence_fd;

    // 获取 buffer cache信息
    std::shared_ptr<DrmBuffer> buffer = ctx->GetBufferCache(acquire_buffer);
    if(!buffer->initCheck()){
      HWC2_ALOGE("tunnel_id=%d buffer_id:0x%" PRIx64 " DrmBuffer import fail, "
                 "acquire_buffer=%p present_time=%" PRIi64 ,
                 ctx->GetTunnelId(), acquire_buffer->buffer_id,
                 acquire_buffer, queue_timestamp);

      //release之后buffer_id不一定还能访问到，提前存一份buffer_id
      uint64_t buffer_id = acquire_buffer->buffer_id;
      ret = g_rkvt_ops.rk_vt_release_buffer(iTunnelFd_, ctx->GetTunnelId(),acquire_buffer);
      if(ret){
        HWC2_ALOGE("tunnel_id=%d BufferId=0x%" PRIx64" release buffer failed, ret=%d.", 
                   ctx->GetTunnelId(), buffer_id, ret);
        ctx->mReleaseFailedBuffer_.push_back(buffer_id);
      }
      continue;
    }

    //Setup Acquire Fence
    if(acquire_buffer->rdy_render_fence_fd>0){
      ctx->SetAcquireFence(acquire_buffer->buffer_id,acquire_buffer->rdy_render_fence_fd);
      acquire_buffer->rdy_render_fence_fd=-1;
    }

    //将当前buffer的生产者queue buffer时间戳和hwc acquire buffer时间戳记录下来
    ctx->SetTimeStamp(acquire_buffer->buffer_id, queue_timestamp);

    // 创建 ReleaseFence
    ret = ctx->AddReleaseFence(acquire_buffer->buffer_id);
    if(ret){
      HWC2_ALOGE("tunnel_id=%d BufferId=0x%" PRIx64" AddReleaseFence fail, ret=%d.",
                  ctx->GetTunnelId(), acquire_buffer->buffer_id, ret);

      //release之后buffer_id不一定还能访问到，提前存一份buffer_id
      uint64_t buffer_id = acquire_buffer->buffer_id;

      ret = g_rkvt_ops.rk_vt_release_buffer(iTunnelFd_, ctx->GetTunnelId(),acquire_buffer);
      if(ret){
        HWC2_ALOGE("tunnel_id=%d BufferId=0x%" PRIx64" release buffer failed, ret=%d.", 
                    ctx->GetTunnelId(), buffer_id, ret);
        ctx->mReleaseFailedBuffer_.push_back(buffer_id);
      }
      continue;
    }

    ret = ctx->AddReleaseFenceRefCnt(-1, acquire_buffer->buffer_id);
    if(ret){
      HWC2_ALOGE("tunnel_id=%d BufferId=0x%" PRIx64" AddReleaseFenceRefCnt fail, ret=%d.", 
                  ctx->GetTunnelId(), acquire_buffer->buffer_id, ret);
    }
    
    int disableReleaseFence = hwc_get_int_property("vendor.hwc.disable_releaseFence","0");
    if(disableReleaseFence)
      iFenceMode_ = DisableReleaseFence;
    else
      iFenceMode_ = EnableReleaseFence;

    //If Release Fence is Enabled,
    if(iFenceMode_==EnableReleaseFence){

      sp<ReleaseFence> release_fence = ctx->GetReleaseFence(acquire_buffer->buffer_id);
      if(release_fence != NULL){
        acquire_buffer->fence_fd = dup(release_fence->getFd());
        HWC2_ALOGD_IF_DEBUG("tunnel_id=%d buffer_id=0x%" PRIx64" release fence:%d acquire_buffer->fence_fd=%d",
                            ctx->GetTunnelId(),acquire_buffer->buffer_id, release_fence->getFd(),acquire_buffer->fence_fd);
      }
    }else{
      acquire_buffer->fence_fd = -1;
    }

    //Now we can release Buffer
    ret = g_rkvt_ops.rk_vt_release_buffer(iTunnelFd_, ctx->GetTunnelId(), acquire_buffer);
    if(ret){
      HWC2_ALOGE("tunnel_id=%d, buffer_id=0x%" PRIx64" Buffer release failed, ret=%d.",
                 ctx->GetTunnelId(),buffer_id, ret);
      ctx->mReleaseFailedBuffer_.push_back(buffer_id);
    }
    ctx->ReleaseBufferInfo(buffer_id);
    ctx->PrintReleaseFailedBuffer();

#ifdef USE_LIBPQ_HWPQ
    if(gIsRK3576()){
      int hwpq_mode = hwc_get_int_property("persist.vendor.tvinput.rkpq.mode","0");
      if(hwpq_mode == 2){
        HWC2_ALOGD_IF_DEBUG("persist.vendor.tvinput.rkpq.mode =%d Do pq",hwpq_mode);
        auto hwpq_buffer=DoHwPq(ctx, buffer);

        if(hwpq_buffer!=NULL){
          hwpq_buffer->SetExternalId(buffer->GetExternalId());
          buffer = hwpq_buffer;
        }
      }else{
        buffer->RemoveHwPqRegs();
      }
    }
#endif

    //Add buff to list
    ctx->lBuffer_.push_back(buffer);
    //if more than 2 buffer release old one
    while(ctx->lBuffer_.size()>2){      
      uint64_t buffer_id = ctx->lBuffer_.front()->GetExternalId();

      //Signal Release Fence 
      ctx->SignalReleaseFence(-1, buffer_id);

      ctx->lBuffer_.pop_front();
    }

    HWC2_ALOGD_IF_DEBUG("Buffer acquire success. tunnel_id=%d buffer_id:0x%" PRIx64 " fence_fd:%d",
                        ctx->GetTunnelId(), buffer_id, acquire_fence_fd);
  }

}

int DrmVideoProducer::SetProducerFps(int tunnel_id, float fps){
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock{mtx_};

  //Update tunnel fps for
  if(!mMapCtx_.count(tunnel_id)){
    HWC2_ALOGE("mMapCtx_ can't find tunnel_id=%d", tunnel_id);
    return -1;
  }
  mMapCtx_[tunnel_id]->SetProducerFps(fps);

  return 0;
}

float DrmVideoProducer::GetProducerFps(int tunnel_id){
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock{mtx_};

  if(!bInit_){
    HWC2_ALOGE(" fail, bInit_=%d tunnel_id=%d"
              , bInit_, tunnel_id);
    return 60;
  }
  if(!mMapCtx_.count(tunnel_id)){
    HWC2_ALOGE("mMapCtx_ can't find tunnel_id=%d", tunnel_id);
    return 60;
  }

  // 获取 VideoProducer 上下文
  std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
  return ctx->GetProducerFps();
}

void DrmVideoProducer::PrintTimeStamp(int display_id, int tunnel_id, uint64_t buffer_id){
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock{mtx_};

  if(!bInit_){
    HWC2_ALOGE(" fail, bInit_=%d tunnel_id=%d"
              , bInit_, tunnel_id);
    return;
  }

  if(!mMapCtx_.count(tunnel_id)){
    HWC2_ALOGE("mMapCtx_ can't find tunnel_id=%d", tunnel_id);
    return;
  }

  std::shared_ptr<VpContext> ctx = mMapCtx_[tunnel_id];
  ctx->VpPrintTimestamp(display_id, buffer_id);

  return;
}

void DrmVideoProducer::printPendingReleaseTunnel(){
  if(mPendingReleaseTunnel_.size()!=0){
    char buf[200]={0};
    int printCount = mPendingReleaseTunnel_.size();

    if(printCount>5)
      printCount=5;

    for(int i=0;i<printCount;i++)
      sprintf(buf,"%s,%d",buf,mPendingReleaseTunnel_[i]);

    HWC2_ALOGW("Tunnel_id=%s...(total %zu tunnel(s)) pervious disconnect failed!",buf,mPendingReleaseTunnel_.size());
  }
}

};
/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

// #define ENABLE_DEBUG_LOG
#define LOG_TAG "drm_hwc2_gralloc"

#include <log/log.h>
#include <inttypes.h>
#include <errno.h>

#if USE_GRALLOC_4
#include "rockchip/drmgralloc4.h"
#else
#include "gralloc_priv.h"
#include "include/gralloc/formats.h"
#endif
#include "rockchip/drmgralloc.h"
#include "drm_fourcc.h"
#include "xf86drmMode.h"
#include <errno.h>
#include <string.h>
namespace android {

DrmGralloc::DrmGralloc(){
#if USE_GRALLOC_4
  gralloc4::init_env_property();
#else
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                         (const hw_module_t **)&gralloc_);
  if(ret)
    ALOGE("hw_get_module fail");
#endif
}

DrmGralloc::~DrmGralloc(){
  if(drmDeviceFd_>0)
    close(drmDeviceFd_);

}

int DrmGralloc::importBuffer(buffer_handle_t rawHandle, buffer_handle_t* outHandle)
{
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
#if USE_GRALLOC_4
  int err = gralloc4::importBuffer(rawHandle, outHandle);
  if (err != android::OK)
  {
      ALOGE("Failed to import buffer, err : %d", err);
      return -1;
  }
  return err;
#else
  int ret = 0;
  native_handle_t* copy_handle = native_handle_clone(rawHandle);
  if(copy_handle == NULL){
      ALOGE("%s : native_handle_clone fail, handle=%p", __FUNCTION__, rawHandle);
      return -1;
  }

  if(gralloc_ && gralloc_->perform)
      ret = gralloc_->registerBuffer(gralloc_, copy_handle);
  else
      ret = -EINVAL;

  if(ret != 0)
  {
      native_handle_close(copy_handle);
      native_handle_delete(copy_handle);
      ALOGE("%s:cann't import handle=%p.", __FUNCTION__, rawHandle);
      return -1;
  }

  *outHandle = copy_handle;

  return 0;
#endif
}

int DrmGralloc::freeBuffer(buffer_handle_t handle)
{
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
#if USE_GRALLOC_4

  int err = gralloc4::freeBuffer(handle);
  if (err != android::OK)
  {
      ALOGE("Failed to get buffer width, err : %d", err);
      return -1;
  }
  return err;
#else

  int ret = 0;

  if(gralloc_ && gralloc_->perform)
      ret = gralloc_->unregisterBuffer(gralloc_, handle);
  else
      ret = -EINVAL;

  if(ret != 0)
  {
      ALOGE("%s:cann't import handle=%p.", __FUNCTION__, handle);
      return -1;
  }

  native_handle_close(handle);
  native_handle_delete(const_cast<native_handle_t*>(handle));
  return 0;
#endif
}


void DrmGralloc::set_drm_version(int drm_device, int version){
#if USE_GRALLOC_4
    gralloc4::set_drm_version(version);
#endif
    drmDeviceFd_ = drm_device;
    drmVersion_ = version;
    return;
}

int DrmGralloc::hwc_get_handle_width(buffer_handle_t hnd)
{

#if USE_GRALLOC_4
    uint64_t width;

    int err = gralloc4::get_width(hnd, &width);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer width, err : %d", err);
        return -1;
    }

    return (int)width;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_WIDTH;
    int width = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &width);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return width;
#endif
}

int DrmGralloc::hwc_get_handle_height(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t height;

    int err = gralloc4::get_height(hnd, &height);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer height, err : %d", err);
        return -1;
    }

    return (int)height;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_HEIGHT;
    int height = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &height);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return height;
#endif
}

int DrmGralloc::hwc_get_handle_stride(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int pixel_stride;

    int err = gralloc4::get_pixel_stride(hnd, &pixel_stride);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer pixel_stride, err : %d", err);
        return -1;
    }

    return pixel_stride;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_STRIDE;
    int stride = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return stride;
#endif
}

int DrmGralloc::hwc_get_handle_height_stride(buffer_handle_t hnd){
#if USE_GRALLOC_4
    uint64_t height_stride;

    int err = gralloc4::get_height_stride(hnd, &height_stride);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer pixel_stride, err : %d", err);
        return -1;
    }

    return (int)height_stride;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_HEIGHT;
    int height = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &height);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return height;
#endif
}

int DrmGralloc::hwc_get_handle_byte_stride_workround(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int byte_stride;

    int err = gralloc4::get_byte_stride_workround(hnd, &byte_stride);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer byte_stride, err : %d", err);
        return -1;
    }

    return byte_stride;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE;
    int byte_stride = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    // Kernel 4.19 存在需要 workaround 的逻辑
    if(gIsDrmVerison419()){
      int format = hwc_get_handle_format(hnd);
      if(format >= 0){
        if(format == HAL_PIXEL_FORMAT_YUV420_8BIT_I
            || format == HAL_PIXEL_FORMAT_YUV420_10BIT_I
            || format == HAL_PIXEL_FORMAT_Y210){
            ALOGW_IF(LogLevel(android::DBG_DEBUG),"%s,line=%d ,vop driver workround: byte stride %d => %d",
                      __FUNCTION__,__LINE__,byte_stride,byte_stride * 2 / 3);
            byte_stride = byte_stride * 2 / 3;
        }else if(format == HAL_PIXEL_FORMAT_YCBCR_422_I){
            ALOGW_IF(LogLevel(android::DBG_DEBUG),"%s,line=%d ,vop driver workround: byte stride %d => %d",
                      __FUNCTION__,__LINE__,byte_stride, byte_stride / 2);
            byte_stride = byte_stride / 2;
        }
      }
    }
    return byte_stride;
#endif
}

int DrmGralloc::hwc_get_handle_byte_stride(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int byte_stride;

    int err = gralloc4::get_byte_stride(hnd, &byte_stride);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer byte_stride, err : %d", err);
        return -1;
    }

    return byte_stride;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE;
    int byte_stride = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    // Kernel 4.19 存在需要 workaround 的逻辑
    if(gIsDrmVerison419()){
      int format = hwc_get_handle_format(hnd);
      if(format >= 0){
        if(format == HAL_PIXEL_FORMAT_YUV420_8BIT_I
            || format == HAL_PIXEL_FORMAT_YUV420_10BIT_I
            || format == HAL_PIXEL_FORMAT_Y210){
            ALOGW_IF(LogLevel(android::DBG_DEBUG),"%s,line=%d ,vop driver workround: byte stride %d => %d",
                      __FUNCTION__,__LINE__,byte_stride,byte_stride * 2 / 3);
            byte_stride = byte_stride * 2 / 3;
        }else if(format == HAL_PIXEL_FORMAT_YCBCR_422_I){
            ALOGW_IF(LogLevel(android::DBG_DEBUG),"%s,line=%d ,vop driver workround: byte stride %d => %d",
                      __FUNCTION__,__LINE__,byte_stride, byte_stride / 2);
            byte_stride = byte_stride / 2;
        }
      }
    }
    return byte_stride;
#endif
}

int DrmGralloc::hwc_get_handle_format(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int format_requested;

    int err = gralloc4::get_format_requested(hnd, &format_requested);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer format_requested, err : %d", err);
        return -1;
    }

    return format_requested;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT;
    int format = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return format;
#endif
}

uint64_t DrmGralloc::hwc_get_handle_usage(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t usage;

    int err = gralloc4::get_usage(hnd, &usage);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer usage, err : %d", err);
        return -1;
    }

    return usage;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_USAGE;
    uint64_t usage = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &usage);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return (uint64_t)usage;
#endif
}

int DrmGralloc::hwc_get_handle_size(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t allocation_size;

    int err = gralloc4::get_allocation_size(hnd, &allocation_size);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer allocation_size, err : %d", err);
        return -1;
    }

    return (int)allocation_size;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_SIZE;
    int size = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &size);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return size;
#endif
}

int DrmGralloc::hwc_get_handle_attibute(buffer_handle_t hnd, attribute_flag_t flag)
{
    switch ( flag )
    {
        case ATT_WIDTH:
            return hwc_get_handle_width(hnd);
        case ATT_HEIGHT:
            return hwc_get_handle_height(hnd);
        case ATT_STRIDE:
            return hwc_get_handle_stride(hnd);
        case ATT_FORMAT:
            return hwc_get_handle_format(hnd);
        case ATT_HEIGHT_STRIDE:
            return hwc_get_handle_height_stride(hnd);
        case ATT_SIZE:
            return hwc_get_handle_size(hnd);
        case ATT_BYTE_STRIDE:
            return hwc_get_handle_byte_stride(hnd);
        case ATT_BYTE_STRIDE_WORKROUND:
            return hwc_get_handle_byte_stride_workround(hnd);
        default:
            LOG_ALWAYS_FATAL("unexpected flag : %d", flag);
            return -1;
    }
}

/*
@func getHandlePrimeFd:get prime_fd  from handle.Before call this api,As far as now, we
    need register the buffer first.May be the register is good for processer I think

@param hnd:
@return fd: prime_fd. and driver can call the dma_buf_get to get the buffer

*/
int DrmGralloc::hwc_get_handle_primefd(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int share_fd;

    int err = gralloc4::get_share_fd(hnd, &share_fd);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer share_fd, err : %d", err);
        return -1;
    }

    return (int)share_fd;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD;
    int fd = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &fd);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return fd;
#endif
}

int DrmGralloc::hwc_get_handle_name(buffer_handle_t hnd, std::string &name){
#if USE_GRALLOC_4

    int err = gralloc4::get_name(hnd, name);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer share_fd, err : %d", err);
        return -1;
    }

    return (int)err;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;

    struct rk_ashmem_t rk_ashmem;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        HWC2_ALOGE("cann't get value from gralloc");
        return ret;
    }
    std::string layer_name(rk_ashmem.LayerName, maxLayerNameLength);
    name = layer_name;
    return ret;
#endif

}

int DrmGralloc::hwc_get_handle_buffer_id(buffer_handle_t hnd, uint64_t *buffer_id){
#if USE_GRALLOC_4

    int err = gralloc4::get_buffer_id(hnd, buffer_id);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer share_fd, err : %d", err);
        return -1;
    }

    return (int)err;
#else   // USE_GRALLOC_4
  int ret = 0;
	int op = GRALLOC_MODULE_PERFORM_GET_BUFFER_ID;

	if(gralloc_ && gralloc_->perform)
	{
		ret = gralloc_->perform(gralloc_, op, hnd, buffer_id);
	}
	else
	{
		ret = -EINVAL;
	}

	if(ret != 0)
	{
		ALOGE("%s: cann't get buffer_id", __FUNCTION__);
	}
  return -1;
#endif

}

uint32_t DrmGralloc::hwc_get_handle_phy_addr(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    return 0;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PHY_ADDR;
    uint32_t phy_addr = 0;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &phy_addr);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return phy_addr;
#endif
}

uint64_t DrmGralloc::hwc_get_handle_format_modifier(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t format_modifier = 0;
    format_modifier = gralloc4::get_format_modifier(hnd);
    return format_modifier;
#else // #if USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT;
    uint64_t internal_format = 0;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &internal_format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    if((internal_format & MALI_GRALLOC_INTFMT_EXT_MASK) == MALI_GRALLOC_INTFMT_AFBC_BASIC){
      if(gIsRK3576()){
        return DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_32x8);
      }else{
        return DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_16x16);
      }
    }

    return internal_format & MALI_GRALLOC_INTFMT_EXT_MASK;
#endif
}

uint32_t DrmGralloc::hwc_get_handle_fourcc_format(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint32_t fourcc_format = 0;
    fourcc_format = gralloc4::get_fourcc_format(hnd);
    return fourcc_format;
#else // #if USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT;
    int format = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return hwc_get_fourcc_from_hal_format(format);
#endif
}

int DrmGralloc::hwc_get_handle_plane_bytes_stride(buffer_handle_t hnd, std::vector<uint32_t> &byte_strides){
#if USE_GRALLOC_4
    int ret = gralloc4::get_plane_bytes_tride(hnd, byte_strides);
    if(ret){
      ALOGE("%s: fail to get plane_bytes_stride( buffer, ret : %d",  __FUNCTION__, ret);
      return ret;
    }
    return ret;
#else // #if USE_GRALLOC_4
    return -1;
#endif
}


void* DrmGralloc::hwc_get_handle_lock(buffer_handle_t hnd, int width, int height){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  void* cpu_addr = NULL;
#if USE_GRALLOC_4
  int ret = gralloc4::lock(hnd,GRALLOC_USAGE_SW_READ_MASK,0,0,width,height,(void **)&cpu_addr);
  if(ret != 0)
  {
    ALOGE("%s: fail to lock buffer, ret : %d",  __FUNCTION__, ret);
  }
#else // #if USE_GRALLOC_4
  if(gralloc_)
    gralloc_->lock(gralloc_,
                        hnd,
                        GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                        0,
                        0,
                        width,
                        height,
                        (void **)&cpu_addr);
#endif
  return cpu_addr;
}

int DrmGralloc::hwc_get_handle_unlock(buffer_handle_t hnd){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  int ret = 0;
#if USE_GRALLOC_4
  gralloc4::unlock(hnd);
#else   // USE_GRALLOC_4
  ret = gralloc_->unlock(gralloc_, hnd);
#endif  // USE_GRALLOC_4
  return ret;
}

// HAL_PIXEL_FORMAT_BGR_888 定义在 Android 12
// hardware/rockchip/libhardware_rockchip/include/hardware/hardware_rockchip.h 文件
// 其他平台可能存在缺少定义的问题
#ifndef HAL_PIXEL_FORMAT_BGR_888
#define HAL_PIXEL_FORMAT_BGR_888  29
#endif

uint32_t DrmGralloc::hwc_get_fourcc_from_hal_format(int hal_format){
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGBA_1010102:
      return DRM_FORMAT_ABGR2101010;
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGR_888:
      return DRM_FORMAT_RGB888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    //Fix color error in NenaMark2.
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_RGB565;
    case HAL_PIXEL_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
    case HAL_PIXEL_FORMAT_YCbCr_444_888:
      return DRM_FORMAT_NV24;
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
      return DRM_FORMAT_NV16;
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
      return DRM_FORMAT_NV12;
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
      // DrmVersion:
      // 3.0.0 = Kernel 5.10
      // 2.0.0 = Kernel 4.19 Vop driver 不支持 NV15格式
      if(drmVersion_ == 3){
        return DRM_FORMAT_NV15;
      }else{
        return DRM_FORMAT_NV12_10;
      }
    // From hardware/rockchip/libgralloc/gralloc_drm_rockchip.cpp rk_drm_gralloc_select_format()
    case HAL_PIXEL_FORMAT_YUV420_8BIT_I:  // NV12 AFBC,  MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I
      // DrmVersion:
      // 3.0.0 = Kernel 5.10
      // 2.0.0 = Kernel 4.19 Vop driver 不支持 YUV420_8BIT 格式
      if(drmVersion_ == 3){
        return DRM_FORMAT_YUV420_8BIT;
      }else{
        return DRM_FORMAT_NV12;
      }
    case HAL_PIXEL_FORMAT_YUV420_10BIT_I: // NV12_10bit AFBC MALI_GRALLOC_FORMAT_INTERNAL_Y210
      // DrmVersion:
      // 3.0.0 = Kernel 5.10
      // 2.0.0 = Kernel 4.19 Vop driver 不支持 YUV420_8BIT 格式
      if(drmVersion_ == 3){
        return DRM_FORMAT_YUV420_10BIT;
      }else{
        return DRM_FORMAT_NV12_10;
      }
    case HAL_PIXEL_FORMAT_YCbCr_422_I:    // MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT
      // RK3528 Android 13 + kernel 5.10 版本，修正使用 DRM_FORMAT_YUYV
      return DRM_FORMAT_YUYV;
    // case HAL_PIXEL_FORMAT_Y210:           // MALI_GRALLOC_FORMAT_INTERNAL_Y210
    //   return DRM_FORMAT_Y210;
    default:
      ALOGE("Cannot convert hal format to drm format %u, use default format RGBA8888", hal_format);
      return HAL_PIXEL_FORMAT_BGRA_8888;
  }

}
int DrmGralloc::hwc_get_gemhandle_from_fd(uint64_t buffer_fd,
                                          uint64_t buffer_id,
                                          uint32_t *out_gem_handle){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  auto mapGemHandle = mapGemHandles_.find(buffer_id);
  if(mapGemHandle == mapGemHandles_.end()){
    HWC2_ALOGD_IF_VERBOSE("Call drmPrimeFDToHandle buf_fd=%" PRIu64 " buf_id=0x%" PRIx64, buffer_fd, buffer_id);
    uint32_t gem_handle;
    int ret = drmPrimeFDToHandle(drmDeviceFd_, buffer_fd, &gem_handle);
    if (ret) {
      HWC2_ALOGE("failed to import prime fd %" PRIu64 " ret=%d, error=%s", buffer_fd, ret, strerror(errno));
      return ret;
    }
    auto ptrGemHandle = std::make_shared<GemHandle>(drmDeviceFd_,gem_handle);
    auto res = mapGemHandles_.insert(std::pair<uint64_t, std::shared_ptr<GemHandle>>(buffer_id, ptrGemHandle));
    if(res.second==false){
        HWC2_ALOGE("mapGemHandles_ insert fail. maybe buffer_id 0x%" PRIx64 " has existed", buffer_id);
        return -1;
    }
    HWC2_ALOGD_IF_VERBOSE("Get GemHandle buf_fd=%" PRIu64 " buf_id=0x%" PRIx64 " GemHandle=%d", buffer_fd, buffer_id, gem_handle);
    *out_gem_handle = gem_handle;
    return 0;
  }

  HWC2_ALOGD_IF_VERBOSE("Cache GemHandle buf_fd=%" PRIu64 " buf_id=0x%" PRIx64 " GemHandle=%d", buffer_fd, buffer_id,mapGemHandle->second->GetGemHandle());
  mapGemHandle->second->AddRefCnt();
  *out_gem_handle = mapGemHandle->second->GetGemHandle();
  return 0;
}

int DrmGralloc::hwc_free_gemhandle(uint64_t buffer_id){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  auto mapGemHandle = mapGemHandles_.find(buffer_id);
  if(mapGemHandle == mapGemHandles_.end()){
    HWC2_ALOGI("Can't find buf_id=0x%" PRIx64 " GemHandle.", buffer_id);
    return -1;
  }

  if(mapGemHandle->second->CanRelease()){
    mapGemHandles_.erase(mapGemHandle);
    HWC2_ALOGD_IF_VERBOSE("Release GemHandle buf_id=0x%" PRIx64 " success!", buffer_id);
    return 0;
  }
  HWC2_ALOGD_IF_VERBOSE("Sub GemHandle RefCnt buf_id=0x%" PRIx64 " success!", buffer_id);
  return 0;
}

int64_t DrmGralloc::hwc_get_offset_of_dynamic_hdr_metadata(buffer_handle_t hnd){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  int64_t offset = -1;
#if USE_GRALLOC_4
  offset = gralloc4::get_video_metadata_offset(hnd);
  return offset;
#else // #if USE_GRALLOC_4
	int ret = 0;
	int op = GRALLOC_MODULE_PERFORM_GET_OFFSET_OF_DYNAMIC_HDR_METADATA;

	if(gralloc_ && gralloc_->perform)
	{
		ret = gralloc_->perform(gralloc_, op, hnd, &offset);
	}
	else
	{
		ret = -EINVAL;
	}

	if(ret != 0)
	{
		ALOGE("%s: cann't get dynamic_hdr_metadata", __FUNCTION__);
	}
#endif
  return offset;
}

#ifdef RK3528
int DrmGralloc::lock_rkvdec_scaling_metadata(buffer_handle_t hnd, metadata_for_rkvdec_scaling_t** metadata)
{
    std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_LOCK_RKVDEC_SCALING_METADATA;

    if(gralloc_ && gralloc_->perform)
    {
        ret = gralloc_->perform(gralloc_, op, hnd, metadata);
    }
    else
    {
        ret = -EINVAL;
    }

    if(ret != 0)
    {
        HWC2_ALOGE("%s:cann't lock rkdevc_scaling_metadata from gralloc", __FUNCTION__);
        goto exit;
    }

exit:
    return ret;
}

int DrmGralloc::unlock_rkvdec_scaling_metadata(buffer_handle_t hnd)
{
    std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_UNLOCK_RKVDEC_SCALING_METADATA;

    if(gralloc_ && gralloc_->perform)
    {
        ret = gralloc_->perform(gralloc_, op, hnd);
    }
    else
    {
        ret = -EINVAL;
    }

    if(ret != 0)
    {
        ALOGE("%s:cann't unlock rkdevc_scaling_metadata from gralloc", __FUNCTION__);
        goto exit;
    }

exit:
    return ret;
}
#endif


bool DrmGralloc::is_yuv_format(int hal_format, uint32_t fourcc_format){
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
      return true;
    default:
      break;
  }

  switch(hal_format){
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

  return false;
}

int DrmGralloc::hwc_fbid_get_and_cached(buffer_id_t buffer_id,
    int fd, uint32_t width, uint32_t height, uint32_t fourcc_format,
    uint32_t bo_handles[4], uint32_t pitches[4],
    uint32_t offsets[4], uint64_t modifier[4], uint32_t* out_fb_id,
    uint32_t flags){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  if(buffer_id <= 0){
      HWC2_ALOGE("FbIdCache: Invalid buffer_id = 0x%" PRIx64 ", get fb_id fail!",
                          buffer_id);
      *out_fb_id = 0;
      return -1;
  }

  // 创建 DrmFbIdInfo 信息
  DrmFbIdInfo fb_id_info(width, height, fourcc_format, bo_handles, pitches, offsets,
                         modifier, flags);

  // 若 cache 中找到 BufferId
  if (mapFbIdCacheMap_.count(buffer_id)) {
    // 遍历已Cache的fbid资源
    for (auto &cache_info : mapFbIdCacheMap_[buffer_id].mapCacheInfo) {
      //对fb_id的参数与cache进行比对
      if (cache_info.second.mInfo == fb_id_info) {
        cache_info.second.iRefCount++;
        *out_fb_id = cache_info.first;
        HWC2_ALOGD_IF_DEBUG("FbIdCache: Has cached! buffer_id=0x%" PRIx64 ", fb_id=%u ref=%d",
                            buffer_id, *out_fb_id, cache_info.second.iRefCount);
        return 0;
      }
    }
  }else{ // 若没有找到，则创建对应cache资源
    DrmFbIdCaches buffer_cache_info;
    buffer_cache_info.iLayerRefCount = 0;
    mapFbIdCacheMap_[buffer_id] = buffer_cache_info;
  }

  //如果找不到cache，则进行import
  uint32_t fb_id = 0;
  int ret = drmModeAddFB2WithModifiers(fd, width, height, fourcc_format, bo_handles,
                                   pitches, offsets, modifier, &fb_id, flags);
  HWC2_ALOGD_IF_DEBUG("FbIdCache: import : buffer_id=0x%" PRIx64 " fd=%d,w=%d,h=%d, "
             "bo->format=%c%c%c%c,gem_handle=%d %d %d %d, pitches[0]=%d %d %d %d, "
             "offset[0]=%d %d %d %d fb_id=%d, modifier=0x%" PRIx64,
    buffer_id, fd, width, height,
    fourcc_format, fourcc_format >> 8, fourcc_format >> 16, fourcc_format >> 24,
    bo_handles[0], bo_handles[1], bo_handles[2], bo_handles[3],
    pitches[0], pitches[1], pitches[2], pitches[3],
    offsets[0], offsets[1], offsets[2], offsets[3],
    fb_id, modifier[0]);
  if (ret) {
    HWC2_ALOGE("FbIdCache: import failed: buffer_id=0x%" PRIx64 " fd=%d,w=%d,h=%d, "
              "bo->format=%c%c%c%c,gem_handle=%d %d %d %d, pitches[0]=%d %d %d %d, "
              "offset[0]=%d %d %d %d fb_id=%d, modifier=0x%" PRIx64 " ret=%d" ,
      buffer_id, fd, width, height,
      fourcc_format, fourcc_format >> 8, fourcc_format >> 16, fourcc_format >> 24,
      bo_handles[0], bo_handles[1], bo_handles[2], bo_handles[3],
      pitches[0], pitches[1], pitches[2], pitches[3],
      offsets[0], offsets[1], offsets[2], offsets[3],
      fb_id, modifier[0], ret);
    return ret;
  }

  // 增加对GemHandle的引用计数
  uint32_t out_gem_handle = 0;
  ret = hwc_get_gemhandle_from_fd(0, buffer_id, &out_gem_handle);
  if(ret){
    HWC2_ALOGD_IF_ERR("GemHandle RefCnt++: buffer_id=0x%" PRIx64 ", fb_id:%u fail!", buffer_id,
                        fb_id);
  }

  HWC2_ALOGD_IF_DEBUG("FbIdCache: Import buffer_id=0x%" PRIx64 ", fb_id:%u", buffer_id,
                      fb_id);
  DrmFbIdCache cache_info(fb_id_info, fb_id);
  cache_info.iRefCount = 1;
  mapFbIdCacheMap_[buffer_id].mapCacheInfo[fb_id] = cache_info;
  //增加fb_id到buffer_id反向查询map，提高 hwc_fbid_rm_cache 效率
  mapFbIdBufferId_[fb_id] = buffer_id;
  *out_fb_id = fb_id;
  return 0;
}

int DrmGralloc::hwc_fbid_rm_cache(int fd, uint32_t fb_id){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  int ret;
  //从 fb_id 反查 buffer_id
  if(!mapFbIdBufferId_.count(fb_id)){
    HWC2_ALOGE("FbIdCache: fb_id=%u is not in list, please check!",fb_id);
    return -1;
  }

  // 若没有找到 buffer_id 则说明异常
  uint64_t buffer_id = mapFbIdBufferId_[fb_id];
  if(mapFbIdCacheMap_.count(buffer_id) == 0){
    HWC2_ALOGE("FbIdCache: fbid=%d buffer_id=0x%" PRIx64 " is not in list, please check! ",
                fb_id, buffer_id);
    return -1;
  }

  auto &fb_id_caches = mapFbIdCacheMap_[buffer_id];
  if(fb_id_caches.mapCacheInfo.count(fb_id) == 0){
    HWC2_ALOGE("FbIdCache: fb_id=%" PRIu32 " can't find any cache, please check!", fb_id);
    return -1;
  }

  auto &cache_info = fb_id_caches.mapCacheInfo[fb_id];
  cache_info.iRefCount--;
  HWC2_ALOGD_IF_DEBUG("FbIdCache: buffer_id=0x%" PRIx64 " fb_id=%u, Hwc2LayerdRef = %d , ref = %d",
                        buffer_id, fb_id,
                        mapFbIdCacheMap_[buffer_id].iLayerRefCount,
                        cache_info.iRefCount);

  // 若 fbid 引用计数非0,对应 hwc2Layer 引用计数非0
  // 则直接返回，完成减引用操作
  if(cache_info.iRefCount == 0 &&
     fb_id_caches.iLayerRefCount == 0){
    //如果 fb_id_list 无 Hwc2Layer引用且 fb_id 无引用，则删除对应 cache 资源
    HWC2_ALOGD_IF_DEBUG("remove fb_id=%" PRIu32, fb_id);
    int ret = drmModeRmFB(fd, fb_id);
    if(ret){
      HWC2_ALOGE("FbIdCache: drmModeRmFB fail ret = %d, fb_id=%d ", ret, fb_id);
    }
    // 减少GemHandle的引用计数
    ret = hwc_free_gemhandle(buffer_id);
    if(ret){
      HWC2_ALOGD_IF_ERR("GemHandle RefCnt--: buffer_id=0x%" PRIx64 ", fb_id:%u fail!", buffer_id,
                          fb_id);
    }
    fb_id_caches.mapCacheInfo.erase(fb_id);
    mapFbIdBufferId_.erase(fb_id);

    if(fb_id_caches.mapCacheInfo.size() == 0){
      //如果无图层引用此buffer_id且此buffer_id下无有效fb_id，则删除buffer_id
      mapFbIdCacheMap_.erase(buffer_id);
            HWC2_ALOGD_IF_DEBUG("FbIdCache: remove buffer_id=0x%" PRIx64 , buffer_id);
    }
  }

  return 0;
}

int DrmGralloc::hwc_fbid_add_layer_ref_count(uint64_t buffer_id){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);
  if(mapFbIdCacheMap_.count(buffer_id) == 0){
      DrmFbIdCaches buffer_cache_info;
      buffer_cache_info.iLayerRefCount = 0;
      mapFbIdCacheMap_[buffer_id] = buffer_cache_info;
  }

  mapFbIdCacheMap_[buffer_id].iLayerRefCount++;
  HWC2_ALOGD_IF_DEBUG("FbIdCache: buffer_id=0x%" PRIx64 " LayerRefCount=%d",
                      buffer_id, mapFbIdCacheMap_[buffer_id].iLayerRefCount);
  return 0;
}

int DrmGralloc::hwc_fbid_dec_layer_ref_count(uint64_t buffer_id){
  std::unique_lock<std::recursive_mutex> lock(mRecursiveMutex);

  if(mapFbIdCacheMap_.count(buffer_id)==0){
    HWC2_ALOGE("FbIdCache: buffer_id=0x%" PRIx64 " is not in list.",buffer_id);
    return -1;
  }

  mapFbIdCacheMap_[buffer_id].iLayerRefCount--;

  if(mapFbIdCacheMap_[buffer_id].iLayerRefCount == 0){
    std::vector<int> fb_id_to_delete;
    for (const auto& cache_info : mapFbIdCacheMap_[buffer_id].mapCacheInfo) {
        if(cache_info.second.iRefCount==0){
            fb_id_to_delete.push_back(cache_info.second.mFbId);
        }
    }

    if(fb_id_to_delete.size() > 0){
      for (const int fb_id : fb_id_to_delete) {
        int ret = drmModeRmFB(get_drm_device(), fb_id);
        if(ret){
          HWC2_ALOGE("FbIdCache: drmModeRmFB failed, ret = %d",ret);
        }else{
          HWC2_ALOGD_IF_DEBUG("FbIdCache: drmModeRmFB fbid = %u success", fb_id);
        }

        // 减少GemHandle的引用计数
        ret = hwc_free_gemhandle(buffer_id);
        if(ret){
          HWC2_ALOGD_IF_ERR("GemHandle RefCnt--: buffer_id=0x%" PRIx64 ", fb_id:%u fail!", buffer_id,
                              fb_id);
        }
        mapFbIdCacheMap_[buffer_id].mapCacheInfo.erase(fb_id);
        mapFbIdBufferId_.erase(fb_id);
      }
    }

    if(mapFbIdCacheMap_[buffer_id].mapCacheInfo.size()==0){
      mapFbIdCacheMap_.erase(buffer_id);
      HWC2_ALOGD_IF_DEBUG("FbIdCache: remove buffer_id:0x%" PRIx64 ,buffer_id);
    }
  }

  if(LogLevel(DBG_VERBOSE)){
    HWC2_ALOGD_IF_VERBOSE("FbIdCache-dump: mapFbIdCacheMap_ size=%zu", mapFbIdCacheMap_.size());
    for(auto &fb_id_caches : mapFbIdCacheMap_){
        HWC2_ALOGD_IF_VERBOSE("FbIdCache-dump: \t buffer-id=0x%" PRIx64 " hwc2LayerRef=%d fb_id_map size=%zu",
                             fb_id_caches.first,
                             fb_id_caches.second.iLayerRefCount,
                             fb_id_caches.second.mapCacheInfo.size());
      for(auto &fb_id_cache : fb_id_caches.second.mapCacheInfo){
        HWC2_ALOGD_IF_VERBOSE("FbIdCache-dump: \t\t fb_id=%" PRIu32 ", ref=%d",
                            fb_id_cache.second.mFbId,
                            fb_id_cache.second.iRefCount);
      }
    }
  }
  return 0;
}

}

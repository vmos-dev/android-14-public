/*
 * Copyright (C) 2023 Rockchip Electronics Co.Ltd.
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

#include <set>


#include <rockchip/utils/drmdebug.h>
#include <rockchip/utils/rgautils.h>
#include <rockchip/utils/rgaformats.h>
#include <3rd/hal/drmhwc2_hal_format.h>
#include <drmlayer.h>

#include <system/graphics.h>


namespace hwc_rga_utils{

using android::DBG_ERROR;

int HwcGetRgaCompatibleFormat(int format) {
  if (format == 0) return format;

  if ((format >> 8) != 0) {
    return format;
  } else {
    return format << 8;
  }
  return format;
}

int HwcGetRgaFormatFromAndroid(int format) {
  switch (format) {
    case HAL_PIXEL_FORMAT_RGB_565:
      return RK_FORMAT_RGB_565;
    case HAL_PIXEL_FORMAT_RGB_888:
      return RK_FORMAT_RGB_888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return RK_FORMAT_RGBA_8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return RK_FORMAT_RGBX_8888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return RK_FORMAT_BGRA_8888;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
      return RK_FORMAT_YCrCb_420_SP;
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
      return RK_FORMAT_YCbCr_420_SP;
    case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO:
      return RK_FORMAT_YCbCr_420_SP;
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
      return RK_FORMAT_YCbCr_420_SP_10B;  // 0x20
    default:
      HWC2_ALOGD_IF_ERR("%x is not supported, please fix.", format);
      return -1;
  }
}

int HwcGetRgaFormat(int format) {
  /* Because the format of librga is the value of driver format << 8 . */
  if (format & 0xFF) {
    format = HwcGetRgaFormatFromAndroid(format);
    if (format < 0) return -1;
  }
  if (format & 0xFF00 || format == 0)
    return format;
  else {
    format = HwcGetRgaCompatibleFormat(format);
    if (format & 0xFF00 || format == 0) return format;
  }

  HWC2_ALOGD_IF_ERR("%x is not supported, please fix.", format);
  return -1;
}

std::set<int> rk3588_rga3_support_formats({
    RK_FORMAT_RGBA_8888,        RK_FORMAT_BGRA_8888,
    RK_FORMAT_ARGB_8888,        RK_FORMAT_ABGR_8888,
    RK_FORMAT_RGBX_8888,        RK_FORMAT_BGRX_8888,
    RK_FORMAT_XRGB_8888,        RK_FORMAT_XBGR_8888,
    RK_FORMAT_RGB_888,          RK_FORMAT_BGR_888,
    RK_FORMAT_RGB_565,          RK_FORMAT_BGR_565,
    RK_FORMAT_YCbCr_420_SP,     RK_FORMAT_YCrCb_420_SP,
    RK_FORMAT_YCbCr_422_SP,     RK_FORMAT_YCrCb_422_SP,
    RK_FORMAT_YUYV_422,         RK_FORMAT_YVYU_422,
    RK_FORMAT_UYVY_422,         RK_FORMAT_VYUY_422,
    RK_FORMAT_YCbCr_420_SP_10B, RK_FORMAT_YCrCb_420_SP_10B,
    RK_FORMAT_YCbCr_422_SP_10B, RK_FORMAT_YCrCb_422_SP_10B
});

std::set<int> rk3588_rga2_support_formats({
    RK_FORMAT_RGBA_8888,        RK_FORMAT_BGRA_8888,
    RK_FORMAT_ARGB_8888,        RK_FORMAT_ABGR_8888,
    RK_FORMAT_RGBX_8888,        RK_FORMAT_BGRX_8888,
    RK_FORMAT_XRGB_8888,        RK_FORMAT_XBGR_8888,
    RK_FORMAT_RGBA_4444,        RK_FORMAT_BGRA_4444,
    RK_FORMAT_ARGB_4444,        RK_FORMAT_ABGR_4444,
    RK_FORMAT_RGBA_5551,        RK_FORMAT_BGRA_5551,
    RK_FORMAT_ARGB_5551,        RK_FORMAT_ABGR_5551,
    RK_FORMAT_RGB_888,          RK_FORMAT_BGR_888,
    RK_FORMAT_RGB_565,          RK_FORMAT_BGR_565,
    RK_FORMAT_YCbCr_420_SP,     RK_FORMAT_YCrCb_420_SP,
    RK_FORMAT_YCbCr_422_SP,     RK_FORMAT_YCrCb_422_SP,
    RK_FORMAT_YCbCr_420_P,      RK_FORMAT_YCrCb_420_P,
    RK_FORMAT_YCbCr_422_P,      RK_FORMAT_YCrCb_422_P,
    RK_FORMAT_YUYV_422,         RK_FORMAT_YVYU_422,
    RK_FORMAT_UYVY_422,         RK_FORMAT_VYUY_422,
    RK_FORMAT_YCbCr_400,        RK_FORMAT_YCbCr_420_SP_10B,
    RK_FORMAT_YCrCb_420_SP_10B, RK_FORMAT_YCbCr_422_SP_10B,
    RK_FORMAT_YCrCb_422_SP_10B
});

std::set<int> rk3576_rga2_support_formats({
    RK_FORMAT_RGBA_8888,        RK_FORMAT_BGRA_8888,
    RK_FORMAT_ARGB_8888,        RK_FORMAT_ABGR_8888,
    RK_FORMAT_RGBX_8888,        RK_FORMAT_BGRX_8888,
    RK_FORMAT_XRGB_8888,        RK_FORMAT_XBGR_8888,
    RK_FORMAT_RGBA_4444,        RK_FORMAT_BGRA_4444,
    RK_FORMAT_ARGB_4444,        RK_FORMAT_ABGR_4444,
    RK_FORMAT_RGBA_5551,        RK_FORMAT_BGRA_5551,
    RK_FORMAT_ARGB_5551,        RK_FORMAT_ABGR_5551,
    RK_FORMAT_RGB_888,          RK_FORMAT_BGR_888,
    RK_FORMAT_RGB_565,          RK_FORMAT_BGR_565,
    RK_FORMAT_YCbCr_420_SP,     RK_FORMAT_YCrCb_420_SP,
    RK_FORMAT_YCbCr_422_SP,     RK_FORMAT_YCrCb_422_SP,
    RK_FORMAT_YCbCr_444_SP,     RK_FORMAT_YCrCb_444_SP,
    RK_FORMAT_YCbCr_420_P,      RK_FORMAT_YCrCb_420_P,
    RK_FORMAT_YCbCr_422_P,      RK_FORMAT_YCrCb_422_P,
    RK_FORMAT_YUYV_422,         RK_FORMAT_YVYU_422,
    RK_FORMAT_UYVY_422,         RK_FORMAT_VYUY_422,
    RK_FORMAT_YCbCr_400,        RK_FORMAT_YCbCr_420_SP_10B,
    RK_FORMAT_YCrCb_420_SP_10B, RK_FORMAT_YCbCr_422_SP_10B,
    RK_FORMAT_YCrCb_422_SP_10B,
});

int UnifyAndroidFormatForRK3588(int format){
    auto input_format = format;

    if (input_format == HAL_PIXEL_FORMAT_YUV420_8BIT_I) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV420_10BIT_I) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12_10;
    } else if (input_format == HAL_PIXEL_FORMAT_YCBCR_420_888) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }
    return input_format;
}

int UnifyAndroidFormatForRK3576(int format){
    auto input_format = format;

    if (input_format == HAL_PIXEL_FORMAT_YUV420_8BIT_I) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV420_10BIT_I) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12_10;
    } else if (input_format == HAL_PIXEL_FORMAT_YCBCR_420_888) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV420_8BIT_RFBC) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV420_10BIT_RFBC) {
        input_format = HAL_PIXEL_FORMAT_YCrCb_NV12_10;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV422_8BIT_RFBC) {
        input_format = RK_FORMAT_YCbCr_422_SP;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV422_10BIT_RFBC) {
        input_format = RK_FORMAT_YCbCr_422_SP_10B;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV444_8BIT_RFBC) {
        input_format = RK_FORMAT_YCbCr_444_SP;
    } else if (input_format == HAL_PIXEL_FORMAT_YUV444_10BIT_RFBC) {
        HWC2_ALOGD_IF_ERR("librga do not support this Format: YUV444_10BIT_RFBC");
      // input_format = RK_FORMAT_YCbCr_444_SP_10B;
    }
    return input_format;
}

bool isRK3588RGA3SupportFormat(int format) {
  int input_format = UnifyAndroidFormatForRK3588(format);
  int rga_input_format = HwcGetRgaFormat(input_format);

  if (rk3588_rga3_support_formats.count(rga_input_format)) {
    return true;
  } else {
    return false;
  }
}

bool isRK3588RGA2SupportFormat(int format) {
  int input_format = UnifyAndroidFormatForRK3588(format);
  int rga_input_format = HwcGetRgaFormat(input_format);

  if (rk3588_rga2_support_formats.count(rga_input_format)) {
    return true;
  } else {
    return false;
  }
}

bool isRK3576RGA2SupportFormat(int format) {
  int input_format = UnifyAndroidFormatForRK3576(format);
  int rga_input_format = HwcGetRgaFormat(input_format);

  if (rk3576_rga2_support_formats.count(rga_input_format)) {
    return true;
  } else {
    return false;
  }
}

};
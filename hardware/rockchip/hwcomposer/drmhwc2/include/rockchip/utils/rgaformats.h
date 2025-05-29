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

//taken from hardware/rockchip/librga/include/rga.h
//内容从hardware/rockchip/librga/include/rga.h 中拷贝

namespace hwc_rga_utils{

enum HWC_Rga_SURF_FORMAT {
    RK_FORMAT_RGBA_8888    = 0x0 << 8,
    RK_FORMAT_RGBX_8888    = 0x1 << 8,
    RK_FORMAT_RGB_888      = 0x2 << 8,
    RK_FORMAT_BGRA_8888    = 0x3 << 8,
    RK_FORMAT_RGB_565      = 0x4 << 8,
    RK_FORMAT_RGBA_5551    = 0x5 << 8,
    RK_FORMAT_RGBA_4444    = 0x6 << 8,
    RK_FORMAT_BGR_888      = 0x7 << 8,

    RK_FORMAT_YCbCr_422_SP = 0x8 << 8,
    RK_FORMAT_YCbCr_422_P  = 0x9 << 8,
    RK_FORMAT_YCbCr_420_SP = 0xa << 8,
    RK_FORMAT_YCbCr_420_P  = 0xb << 8,

    RK_FORMAT_YCrCb_422_SP = 0xc << 8,
    RK_FORMAT_YCrCb_422_P  = 0xd << 8,
    RK_FORMAT_YCrCb_420_SP = 0xe << 8,
    RK_FORMAT_YCrCb_420_P  = 0xf << 8,

    RK_FORMAT_BPP1         = 0x10 << 8,
    RK_FORMAT_BPP2         = 0x11 << 8,
    RK_FORMAT_BPP4         = 0x12 << 8,
    RK_FORMAT_BPP8         = 0x13 << 8,

    RK_FORMAT_Y4           = 0x14 << 8,
    RK_FORMAT_YCbCr_400    = 0x15 << 8,

    RK_FORMAT_BGRX_8888    = 0x16 << 8,

    RK_FORMAT_YVYU_422     = 0x18 << 8,
    RK_FORMAT_YVYU_420     = 0x19 << 8,
    RK_FORMAT_VYUY_422     = 0x1a << 8,
    RK_FORMAT_VYUY_420     = 0x1b << 8,
    RK_FORMAT_YUYV_422     = 0x1c << 8,
    RK_FORMAT_YUYV_420     = 0x1d << 8,
    RK_FORMAT_UYVY_422     = 0x1e << 8,
    RK_FORMAT_UYVY_420     = 0x1f << 8,

    RK_FORMAT_YCbCr_420_SP_10B = 0x20 << 8,
    RK_FORMAT_YCrCb_420_SP_10B = 0x21 << 8,
    RK_FORMAT_YCbCr_422_SP_10B = 0x22 << 8,
    RK_FORMAT_YCrCb_422_SP_10B = 0x23 << 8,
    /* For compatibility with misspellings */
    RK_FORMAT_YCbCr_422_10b_SP = RK_FORMAT_YCbCr_422_SP_10B,
    RK_FORMAT_YCrCb_422_10b_SP = RK_FORMAT_YCrCb_422_SP_10B,

    RK_FORMAT_BGR_565      = 0x24 << 8,
    RK_FORMAT_BGRA_5551    = 0x25 << 8,
    RK_FORMAT_BGRA_4444    = 0x26 << 8,

    RK_FORMAT_ARGB_8888    = 0x28 << 8,
    RK_FORMAT_XRGB_8888    = 0x29 << 8,
    RK_FORMAT_ARGB_5551    = 0x2a << 8,
    RK_FORMAT_ARGB_4444    = 0x2b << 8,
    RK_FORMAT_ABGR_8888    = 0x2c << 8,
    RK_FORMAT_XBGR_8888    = 0x2d << 8,
    RK_FORMAT_ABGR_5551    = 0x2e << 8,
    RK_FORMAT_ABGR_4444    = 0x2f << 8,

    RK_FORMAT_RGBA2BPP     = 0x30 << 8,
    RK_FORMAT_A8           = 0x31 << 8, /* [0:7] Alpha */

    RK_FORMAT_YCbCr_444_SP = 0x32 << 8, /*  2 plane YCbCr little endian
                                         * plane 0: [0:7] Y
                                         * plane 1: non-subsampled [0:15] Cb:Cr 8:8  */
	RK_FORMAT_YCrCb_444_SP = 0x33 << 8, /*  2 plane YCrCb little endian
                                         * plane 0: [0:7] Y
                                         * plane 1: non-subsampled [0:15] Cr:Cb 8:8  */
    RK_FORMAT_Y8           = 0x34 << 8, /* [0:7] zero:Y 4:4 little endian */

    RK_FORMAT_UNKNOWN      = 0x100 << 8,
};

}
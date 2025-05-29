#
# Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
#
# Modification based on code covered by the Apache License, Version 2.0 (the "License").
# You may not use this software except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
# AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
# IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
# NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.
#
# IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

BOARD_USES_DRM_HWCOMPOSER2=false
BOARD_USES_DRM_HWCOMPOSER=false
# rk356x rk3588 rk3528 rk3562 use DrmHwc2
ifneq ($(filter rk356x rk3588 rk3528 rk3562 rk3576, $(strip $(TARGET_BOARD_PLATFORM))), )
ifeq ($(strip $(BUILD_WITH_RK_EBOOK)),true)
        BOARD_USES_DRM_HWCOMPOSER2=false
else  # BUILD_WITH_RK_EBOOK
        BOARD_USES_DRM_HWCOMPOSER2=true
endif # BUILD_WITH_RK_EBOOK
else
endif
#rk3399 rk3326 Android14 use DrmHwc2
ifneq ($(filter rk3399 rk3326, $(strip $(TARGET_BOARD_PLATFORM))), )
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 34)))
        BOARD_USES_DRM_HWCOMPOSER2=true
endif
endif

ifeq ($(strip $(BOARD_USES_DRM_HWCOMPOSER2)),true)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
  libcutils \
  libdrm \
  libhardware \
  liblog \
  libui \
  libutils \
  libsync_vendor \
  libtinyxml2 \
  libbaseparameter \
  librga \
  libbase

LOCAL_STATIC_LIBRARIES := \
  libdrmhwcutils

LOCAL_C_INCLUDES := \
  ${LOCAL_PATH}/include \
  external/libdrm \
  external/libdrm/include/drm \
  system/core \
  system/core/libsync/include \
  external/tinyxml2 \
  hardware/rockchip/libbaseparameter \
  hardware/rockchip/librga/include \
  hardware/rockchip/librga/im2d_api

LOCAL_SRC_FILES := \
  drmhwctwo.cpp \
  drm/drmconnector.cpp \
  drm/drmcrtc.cpp \
  drm/drmdevice.cpp \
  drm/drmencoder.cpp \
  drm/drmeventlistener.cpp \
  drm/drmmode.cpp \
  drm/drmplane.cpp \
  drm/drmproperty.cpp \
  drm/drmcompositorworker.cpp \
  resources/resourcemanager.cpp \
  resources/resourcescache.cpp \
  drm/vsyncworker.cpp \
  drm/invalidateworker.cpp \
  utils/autolock.cpp \
  rockchip/compositor/drmdisplaycomposition.cpp \
  rockchip/compositor/drmdisplaycompositor.cpp \
  rockchip/utils/drmdebug.cpp \
  rockchip/utils/rgautils.cpp \
  rockchip/common/drmfence.cpp \
  rockchip/common/drmlayer.cpp \
  rockchip/common/drmtype.cpp \
  rockchip/common/drmgralloc.cpp \
  rockchip/common/drmbaseparameter.cpp \
  rockchip/platform/common/platformdrmgeneric.cpp \
  rockchip/platform/common/platform.cpp \
  rockchip/platform/rk3326/drmvop3326.cpp \
  rockchip/platform/rk3399/drmvop3399.cpp \
  rockchip/platform/rk356x/drmvop356x.cpp \
  rockchip/platform/rk3588/drmvop3588.cpp \
  rockchip/platform/rk3576/drmvop3576.cpp \
  rockchip/platform/rk3528/drmvop3528.cpp \
  rockchip/platform/rk3562/drmvop3562.cpp \
  rockchip/platform/rk3326/drmhwc3326.cpp \
  rockchip/platform/rk3399/drmhwc3399.cpp \
  rockchip/platform/rk356x/drmhwc356x.cpp \
  rockchip/platform/rk3588/drmhwc3588.cpp \
  rockchip/platform/rk3576/drmhwc3576.cpp \
  rockchip/platform/rk3528/drmhwc3528.cpp \
  rockchip/platform/rk3562/drmhwc3562.cpp \
  rockchip/common/drmbufferqueue.cpp \
  rockchip/common/drmbuffer.cpp \
  rockchip/common/hdr/drmhdrparser.cpp \
  rockchip/producer/drmvideoproducer.cpp \
  rockchip/producer/vpcontext.cpp


LOCAL_CPPFLAGS += \
  -DHWC2_USE_CPP11 \
  -DHWC2_INCLUDE_STRINGIFICATION \
  -DRK_DRM_GRALLOC \
  -DUSE_HWC2 \
  -DMALI_AFBC_GRALLOC \
  -Wno-unreachable-code-loop-increment \
  -DUSE_NO_ASPECT_RATIO \
  -fPIC

ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 32)))
LOCAL_CPPFLAGS += -DANDROID_T
endif

ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 31)))
LOCAL_CFLAGS += -DANDROID_S
LOCAL_HEADER_LIBRARIES += \
  libhardware_rockchip_headers
endif

# API 30 -> Android 11.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 30)))
LOCAL_C_INCLUDES += \
    hardware/rockchip/hwcomposer/drmhwc2/include
LOCAL_CPPFLAGS += -DANDROID_R

# Gralloc config:
ifeq ($(TARGET_RK_GRALLOC_VERSION),4) # Gralloc 4.0
LOCAL_CPPFLAGS += -DUSE_GRALLOC_4=1
LOCAL_SHARED_LIBRARIES += \
    libhidlbase \
    libgralloctypes \
    android.hardware.graphics.mapper@4.0
LOCAL_SRC_FILES += \
    rockchip/common/drmgralloc4.cpp
LOCAL_HEADER_LIBRARIES += \
    libgralloc_headers
else
  LOCAL_CPPFLAGS += -DUSE_GRALLOC_0=1
endif # Gralloc 4.0

else  # Android 11
LOCAL_C_INCLUDES += \
  hardware/rockchip/hwcomposer/include
endif

# HWC3-AIDL config
ifeq ($(TARGET_USES_HWC3_AIDL), true)
LOCAL_CPPFLAGS += -DUSE_HWC3_AIDL=1
endif

# Mali config:
# API 29 -> Android 10.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 29)))
LOCAL_CPPFLAGS += -DANDROID_Q
ifneq (,$(filter mali-tDVx mali-G52, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
  hardware/rockchip/libgralloc/bifrost \
  hardware/rockchip/libgralloc/bifrost/src
endif

ifneq (,$(filter mali-t860 mali-t760, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
  hardware/rockchip/libgralloc/midgard
endif

ifneq (,$(filter mali400 mali450, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
  hardware/rockchip/libgralloc/utgard
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3368)
LOCAL_C_INCLUDES += \
  system/core/libion/original-kernel-headers
endif
endif

# RK3528 config:
ifneq ($(filter rk3528, $(strip $(TARGET_BOARD_PLATFORM))),)
LOCAL_CPPFLAGS += -DRK3528=1
BOARD_USES_VIVID_HDR = true
# API 28 -> Android 9.0
ifeq (0,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \> 29)))
LOCAL_CPPFLAGS += -DANDROID_P=1
LOCAL_C_INCLUDES += \
  hardware/rockchip/libgralloc/ \
  system/core/liblog/include/
endif
endif

ifneq ($(filter rk3576, $(strip $(TARGET_BOARD_PLATFORM))),)
LOCAL_CPPFLAGS += -DRK3576=1
BOARD_USES_VIVID_HDR = true
endif

# RK3588
ifneq ($(filter rk3588, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk3588
endif

# RK356x
ifneq ($(filter rk356x, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk356x
endif

# RK356x
ifneq ($(filter rk3576, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk3576
endif

ifneq ($(filter rk3399, $(strip $(TARGET_BOARD_PLATFORM))),)
LOCAL_CPPFLAGS += -DRK3399=1
endif
ifneq ($(filter rk3326, $(strip $(TARGET_BOARD_PLATFORM))),)
LOCAL_CPPFLAGS += -DRK3326=1
endif



ifeq ($(strip $(BOARD_USES_VIVID_HDR)),true)
USE_HDR_PARSER=true
# Android 启用 HDR 功能
LOCAL_SHARED_LIBRARIES += \
	libhdr_params_parser
LOCAL_CPPFLAGS += \
	-DUSE_HDR_PARSER=1 \
	-DUSE_VIVID_HDR=1
endif

# SR
# BOARD_USES_LIBSVEP := true
ifeq ($(strip $(BOARD_USES_LIBSVEP)),true)
BOARD_USES_LIBSVEP_SR := true
endif

ifeq ($(strip $(BOARD_USES_LIBSVEP_SR)),true)

CHECKED_DIRECTORY := hardware/rockchip/libsvep/libsvepsr
ifeq ($(wildcard $(CHECKED_DIRECTORY)),)
    $(error Directory $(CHECKED_DIRECTORY) does not exist, Please upgrade the libsvepsr to V2.1.0 version!)
else
    $(info Directory $(CHECKED_DIRECTORY) exists)
endif

LOCAL_C_INCLUDES += \
  hardware/rockchip/libsvep/libsvepsr/lib/Android/$(TARGET_SOC_PLATFORM)/include

LOCAL_SHARED_LIBRARIES += \
	libsvepsr \
	librknnrt-svep

LOCAL_CFLAGS += \
	-DUSE_LIBSR=1

LOCAL_REQUIRED_MODULES += \
	HwcSvepEnv.xml
endif

# MEMC
# BOARD_USES_LIBSVEP_MEMC=true
ifeq ($(strip $(BOARD_USES_LIBSVEP_MEMC)),true)

CHECKED_DIRECTORY := hardware/rockchip/libsvep/libsvepmemc
ifeq ($(wildcard $(CHECKED_DIRECTORY)),)
    $(error Directory $(CHECKED_DIRECTORY) does not exist, Please upgrade the libsvep version!)
else
    $(info Directory $(CHECKED_DIRECTORY) exists)
endif

LOCAL_C_INCLUDES += \
  hardware/rockchip/libsvep/libsvepmemc/lib/Android/$(TARGET_SOC_PLATFORM)/include

LOCAL_SHARED_LIBRARIES += \
	libsvepmemc \
	librknnrt-memc

LOCAL_CFLAGS += \
	-DUSE_LIBSVEP_MEMC=1

LOCAL_REQUIRED_MODULES += \
	HwcSvepMemcEnv.xml
endif

# BOARD_USES_LIBPQ_HWPQ=true
ifeq ($(strip $(BOARD_USES_LIBPQ_HWPQ)),true)
ifneq ($(strip $(TARGET_SOC_PLATFORM)),rk3576)
  $(error BOARD_USES_LIBPQ_HWPQ=true but TARGET_SOC_PLATFORM $(TARGET_SOC_PLATFORM) does not support HWPQ!)
endif
BOARD_USES_LIBPQ=true

LOCAL_CFLAGS += \
	-DUSE_LIBPQ_HWPQ=1
endif

# BOARD_USES_LIBPQ=true
ifeq ($(strip $(BOARD_USES_LIBPQ)),true)

CHECKED_DIRECTORY := hardware/rockchip/libpq
ifeq ($(wildcard $(CHECKED_DIRECTORY)),)
    $(error Directory $(CHECKED_DIRECTORY) does not exist, Please upgrade the libpq version!)
else
    $(info Directory $(CHECKED_DIRECTORY) exists)
endif

CHECKED_DIRECTORY := hardware/rockchip/libvisionpq
ifeq ($(wildcard $(CHECKED_DIRECTORY)),)
    $(error Directory $(CHECKED_DIRECTORY) does not exist, Please upgrade the libvisionpq version!)
else
    $(info Directory $(CHECKED_DIRECTORY) exists)
endif

LOCAL_C_INCLUDES += \
  hardware/rockchip/libpq/include \
  hardware/rockchip/libvisionpq/lib/Android/$(TARGET_SOC_PLATFORM)/include

LOCAL_SHARED_LIBRARIES += \
	libpq

LOCAL_CFLAGS += \
	-DUSE_LIBPQ=1
endif

# HwProxy aidl (hw_output)
# BOARD_USES_HWC_PROXY_SERVICE := false
ifeq ($(strip $(BOARD_USES_HWC_PROXY_SERVICE)),true)
LOCAL_SHARED_LIBRARIES += \
	rockchip.hwc.proxy.aidl-V1-ndk \
	librkhwcproxy \
	libbinder_ndk

LOCAL_C_INCLUDES += \
	hardware/rockchip/hwc_proxy_service/rockchip/hwc/proxy/drm_api

LOCAL_SRC_FILES += \
	rockchip/platform/common/RkHwcProxyClient.cpp

LOCAL_CFLAGS += \
	-DUSE_HWC_PROXY_SERVICE=1
endif


# EBOOK
# BOARD_USES_LIBEBOOK := true
ifeq ($(strip $(BOARD_USES_LIBEBOOK)),true)

CHECKED_DIRECTORY := hardware/rockchip/libebook
ifeq ($(wildcard $(CHECKED_DIRECTORY)),)
    $(error Directory $(CHECKED_DIRECTORY) does not exist, Please upgrade the libebook!)
else
    $(info Directory $(CHECKED_DIRECTORY) exists)
endif

LOCAL_C_INCLUDES += \
  hardware/rockchip/libebook/lib/Android/$(TARGET_SOC_PLATFORM)/include

LOCAL_SHARED_LIBRARIES += \
	libebook

LOCAL_CFLAGS += \
	-DUSE_LIBEBOOK=1
endif

# LOCAL_SANITIZE:=address

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_HARDWARE)
LOCAL_REQUIRED_MODULES += \
	HwComposerEnv.xml

# API 26 -> Android 8.0
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += \
  -Wno-unused-function \
  -Wno-unused-private-field \
  -Wno-unused-function \
  -Wno-unused-variable \
  -Wno-unused-parameter \
  -fPIC \
  -Wno-sign-compare \
  -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

## copy configs/*.xml from etc to /vendor/etc/init/hw
include $(CLEAR_VARS)
LOCAL_MODULE := HwComposerEnv.xml
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := configs/HwComposerEnv.xml
include $(BUILD_PREBUILT)

ifeq ($(strip $(BOARD_USES_LIBSVEP)),true)
## copy configs/*.xml from etc to /vendor/etc/init/hw
include $(CLEAR_VARS)
LOCAL_MODULE := HwcSvepEnv.xml
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := configs/HwcSvepEnv.xml
include $(BUILD_PREBUILT)
endif


ifeq ($(strip $(BOARD_USES_LIBSVEP_MEMC)),true)
## copy configs/*.xml from etc to /vendor/etc/init/hw
include $(CLEAR_VARS)
LOCAL_MODULE := HwcSvepMemcEnv.xml
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := configs/HwcSvepMemcEnv.xml
include $(BUILD_PREBUILT)
endif

ifeq ($(strip $(USE_HDR_PARSER)),true)
# libhdr_params_parser
TARGET_VIVID_HDR_PARSER_LIB_PATH := rockchip/common/hdr/vivid
include $(CLEAR_VARS)
LOCAL_MODULE := libhdr_params_parser
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
LOCAL_VENDOR_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := $(TARGET_VIVID_HDR_PARSER_LIB_PATH)/$(TARGET_ARCH)/libhdr_params_parser.so
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := $(TARGET_VIVID_HDR_PARSER_LIB_PATH)/$(TARGET_2ND_ARCH)/libhdr_params_parser.so
else
LOCAL_SRC_FILES_$(TARGET_ARCH) := $(TARGET_VIVID_HDR_PARSER_LIB_PATH)/$(TARGET_ARCH)/libhdr_params_parser.so
endif
include $(BUILD_PREBUILT)
endif # USE_HDR_PARSER

endif # HWC2

ifeq ($(strip $(BOARD_USES_DRM_HWCOMPOSER2)),true)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif

# Copyright (C) 2012 The Android Open Source Project
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

commonSrcFiles := \
   utils/audio_time.c \
   utils/channel.c \
   bitstream/audio_iec958.c \
   bitstream/audio_bitstream.c \
   bitstream/audio_bitstream_manager.c \
   audio_hw.c \
   alsa_route.c \
   alsa_mixer.c \
   voice_preprocess.c \
   audio_hw_hdmi.c \
   denoise/rkdenoise.c \
   voice/device.c \
   voice/stream.c \
   voice/session.c \
   voice/voice.c \
   voice/effect.c \
   voice/profile.c

ifeq ($(strip $(ENABLE_ADSP)), true)
commonSrcFiles += \
    adsp_config/platform_adsp.c
endif
ifeq ($(strip $(ENABLE_AUDIO_HAL_HFP_HF)), true)
   commonSrcFiles += audio_hw_hfp.c
endif
ifeq ($(strip $(BOARD_USE_AUDIO_EQDRC)),true)
commonSrcFiles += voice_eq_preprocess.c
endif
commonCIncludes := \
   $(call include-path-for, audio-utils) \
   $(call include-path-for, audio-route) \
   $(call include-path-for, speex)
ifeq ($(strip $(ENABLE_ADSP)), true)
commonCIncludes += \
    adsp_config/platform_adsp.h
endif
ifeq ($(strip $(ENABLE_ADSP_RK3308)), true)
commonCIncludes += \
    adsp_config/rk3308_config.h
endif

commonCFlags := -Wno-unused-parameter
commonCFlags += -DLIBTINYALSA_ENABLE_VNDK_EXT
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
    commonCFlags += -DBOX_HAL
endif
ifeq ($(strip $(ENABLE_AUDIO_HAL_HFP_HF)), true)
    commonCFlags += -DSUPPORT_AUDIO_HAL_HFP_HF
endif
ifeq ($(strip $(ENABLE_ADSP)), true)
    commonCFlags += -DSUPPORT_AUDIO_ADSP
endif
ifeq ($(strip $(BOARD_USE_DRM)),true)
    commonCFlags += -DUSE_DRM
endif
ifeq ($(strip $(BOARD_SUPPORT_MULTIAUDIO)), true)
    commonCFlags += -DSUPPORT_MULTIAUDIO
endif
ifeq ($(strip $(BOARD_USE_AUDIO_3A)),true)
    commonCFlags += -DAUDIO_3A
endif
ifeq ($(strip $(BOARD_USE_AUDIO_EQDRC)),true)
$(shell mkdir -p $(TARGET_OUT_VENDOR)/etc/)
$(shell mkdir -p $(TARGET_OUT_VENDOR)/lib/)
$(shell cp -rf $(LOCAL_PATH)/res/*.bin $(TARGET_OUT_VENDOR)/etc/)
$(shell cp -rf $(LOCAL_PATH)/res/libRK_AudioProcess.so $(TARGET_OUT_VENDOR)/lib/)
commonCFlags += -DAUDIO_EQDRC
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3368)
    commonCFlags += -DRK3368
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3228h)
    commonCFlags += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3328)
    commonCFlags += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3228)
    commonCFlags += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk322x)
    commonCFlags += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3128h)
    commonCFlags += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3399)
    commonCFlags += -DRK3399
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3399pro)
    commonCFlags += -DRK3399
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3128)
    commonCFlags += -DRK3128
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3588)
    commonCFlags += -DRK3588
endif
ifeq ($(AUD_VOICE_CONFIG),voice_support)
    commonCFlags += -DVOICE_SUPPORT
endif
#rk3528 no need padding
ifeq ($(filter rk3528, $(TARGET_BOARD_PLATFORM)), )
    commonCFlags += -DADD_PADDING
endif

commonCFlags += -Wno-error

commonSharedLibraries := liblog libcutils libaudioutils libaudioroute \
                         libhardware_legacy libspeexresampler libxml2
#API 31 -> Android 12.0, Android 12.0 link libtinyalsa_iec958
ifneq (1, $(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 31)))
    commonSharedLibraries += libtinyalsa_iec958
    commonCFlags += -DIEC958_FORAMT
    commonCFlags += -DSUPPORT_VX_ROCKCHIP
else
    commonSharedLibraries += libtinyalsa
endif

ifeq ($(strip $(ENABLE_TINYALSA_RK_PLUGIN)), true)
    commonSharedLibraries += libvirtualcard
    commonSharedLibraries += libsndcardparser
endif

commonStaticLibraries := libspeex

include $(CLEAR_VARS)
LOCAL_MODULE := libanr
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_SRC_FILES := denoise/skv/libanr.so
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := session.xml
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := config/voice/session.xml
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)
include $(BUILD_PREBUILT)

#primary hal
include $(CLEAR_VARS)
LOCAL_MODULE := audio.primary.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := $(commonSrcFiles)
LOCAL_C_INCLUDES += $(commonCIncludes)
LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_CFLAGS := $(commonCFlags)
LOCAL_CFLAGS += -DPRIMARY_HAL
#LOCAL_CFLAGS += -DSET_VOL_IN_HAL
LOCAL_SHARED_LIBRARIES := $(commonSharedLibraries)
LOCAL_STATIC_LIBRARIES := $(commonStaticLibraries)
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := session.xml
include $(BUILD_SHARED_LIBRARY)

#make extend hal so for multiaudio
ifeq ($(strip $(BOARD_SUPPORT_MULTIAUDIO)), true)
#1 extend hal
include $(CLEAR_VARS)
LOCAL_MODULE := audio.ext_1.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := $(commonSrcFiles)
LOCAL_C_INCLUDES += $(commonCIncludes)
LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_CFLAGS := $(commonCFlags)
LOCAL_CFLAGS += -DEXT_1_HAL
LOCAL_SHARED_LIBRARIES := $(commonSharedLibraries)
LOCAL_STATIC_LIBRARIES := $(commonStaticLibraries)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

#2 extend hal
include $(CLEAR_VARS)
LOCAL_MODULE := audio.ext_2.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := $(commonSrcFiles)
LOCAL_C_INCLUDES += $(commonCIncludes)
LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_CFLAGS := $(commonCFlags)
LOCAL_CFLAGS += -DEXT_2_HAL
LOCAL_SHARED_LIBRARIES := $(commonSharedLibraries)
LOCAL_STATIC_LIBRARIES := $(commonStaticLibraries)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

#3 extend hal
include $(CLEAR_VARS)
LOCAL_MODULE := audio.ext_3.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := $(commonSrcFiles)
LOCAL_C_INCLUDES += $(commonCIncludes)
LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_CFLAGS := $(commonCFlags)
LOCAL_CFLAGS += -DEXT_3_HAL
LOCAL_SHARED_LIBRARIES := $(commonSharedLibraries)
LOCAL_STATIC_LIBRARIES := $(commonStaticLibraries)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

#4 extend hal
include $(CLEAR_VARS)
LOCAL_MODULE := audio.ext_4.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := $(commonSrcFiles)
LOCAL_C_INCLUDES += $(commonCIncludes)
LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_CFLAGS := $(commonCFlags)
LOCAL_CFLAGS += -DEXT_4_HAL
LOCAL_SHARED_LIBRARIES := $(commonSharedLibraries)
LOCAL_STATIC_LIBRARIES := $(commonStaticLibraries)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
endif

include $(CLEAR_VARS)
LOCAL_CFLAGS += -Wno-error
LOCAL_SRC_FILES:= amix.c alsa_mixer.c
LOCAL_MODULE:= amix
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := liblog libc libcutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
$(info  "BUILD_BISTREAM_TEST")
LOCAL_CFLAGS += -Wno-error

#rk3528 no need padding
ifeq ($(filter rk3528, $(TARGET_BOARD_PLATFORM)), )
LOCAL_CFLAGS += -DADD_PADDING
endif

LOCAL_SRC_FILES := \
    alsa_mixer.c \
    bitstream/audio_bitstream.c  \
    bitstream/audio_bitstream_manager.c  \
    bitstream/audio_iec958.c  \
    bitstream_test.c

LOCAL_C_INCLUDES := \
    system/core/libutils/include \
    ${LOCAL_PATH}/bitstream

LOCAL_32_BIT_ONLY := true
LOCAL_MODULE:= bitstream_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := liblog libc libcutils

ifneq (1, $(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 31)))
LOCAL_SHARED_LIBRARIES += libtinyalsa_iec958
LOCAL_CFLAGS += -DIEC958_FORAMT
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif


include $(BUILD_EXECUTABLE)

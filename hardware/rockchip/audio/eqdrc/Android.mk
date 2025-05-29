LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := librkeqdrc
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := soundfx
LOCAL_SRC_FILES := \
    algo.c \
    eqdrc.c \
    profile.c
LOCAL_CFLAGS := -Wall -Werror -Wextra -Wno-unused-parameter
ifneq (,$(filter $(TARGET_BUILD_VARIANT),eng userdebug))
    LOCAL_SRC_FILES += tuner.c
    LOCAL_CFLAGS += -DEQDRC_TUNER_ENABLED
endif
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \< 14)))
LOCAL_SHARED_LIBRARIES := libaudioutils liblog libutils librkaudio_effect_eqdrc
else
LOCAL_SHARED_LIBRARIES := libaudioutils liblog libutils
LOCAL_STATIC_LIBRARIES := librkaudio_effect_eqdrc_static
endif
LOCAL_HEADER_LIBRARIES := librkeqdrc_headers libaudioeffects libaudioutils_headers \
                          libhardware_headers
LOCAL_REQUIRED_MODULES := rkaudio_effect_eqdrc_48000hz_2ch.bin \
                          rkaudio_effect_eqdrc_44100hz_2ch.bin
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := librkeqdrc_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libs/include
LOCAL_VENDOR_MODULE := true
include $(BUILD_HEADER_LIBRARY)

ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \< 14)))
include $(CLEAR_VARS)
LOCAL_MODULE := librkaudio_effect_eqdrc
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_SRC_FILES_arm := libs/arm32/librkaudio_effect_eqdrc.so
LOCAL_SRC_FILES_arm64 := libs/arm64/librkaudio_effect_eqdrc.so
LOCAL_MULTILIB := both
LOCAL_SHARED_LIBRARIES := libc libstdc++ liblog
include $(BUILD_PREBUILT)
else
include $(CLEAR_VARS)
LOCAL_MODULE := librkaudio_effect_eqdrc_static
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_SUFFIX := .a
LOCAL_SRC_FILES_arm := libs/arm32/librkaudio_effect_eqdrc.a
LOCAL_SRC_FILES_arm64 := libs/arm64/librkaudio_effect_eqdrc.a
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := rkaudio_effect_eqdrc_48000hz_2ch.bin
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)
LOCAL_SRC_FILES := configs/rkaudio_effect_eqdrc_48000hz_2ch.bin
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := rkaudio_effect_eqdrc_44100hz_2ch.bin
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)
LOCAL_SRC_FILES := configs/rkaudio_effect_eqdrc_44100hz_2ch.bin
include $(BUILD_PREBUILT)

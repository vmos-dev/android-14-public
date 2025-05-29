LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := librkpreprocess
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := soundfx
LOCAL_SRC_FILES := \
    algo.c \
    aloop.c \
    preprocess.c \
    profile.c
LOCAL_CFLAGS := -Wall -Werror -Wextra -Wno-unused-parameter
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \< 14)))
LOCAL_SHARED_LIBRARIES := libaudioutils liblog libutils libcutils libxml2 libtinyalsa_iec958 \
                          librkaudio_aec_bf
else
LOCAL_SHARED_LIBRARIES := libaudioutils liblog libutils libcutils libxml2 libtinyalsa_iec958
LOCAL_STATIC_LIBRARIES := librkaudio_aec_bf_static
endif
LOCAL_HEADER_LIBRARIES := libaudioeffects libaudioutils_headers libhardware_headers
LOCAL_REQUIRED_MODULES := rkaudio_effect_preprocess.xml
include $(BUILD_SHARED_LIBRARY)

ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \< 14)))
include $(CLEAR_VARS)
LOCAL_MODULE := librkaudio_aec_bf
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_SRC_FILES_arm := libs/arm32/librkaudio_aec_bf.so
LOCAL_SRC_FILES_arm64 := libs/arm64/librkaudio_aec_bf.so
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libs/include
LOCAL_MULTILIB := both
LOCAL_SHARED_LIBRARIES := libc libstdc++ liblog
include $(BUILD_PREBUILT)
else
include $(CLEAR_VARS)
LOCAL_MODULE := librkaudio_aec_bf_static
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_SUFFIX := .a
LOCAL_SRC_FILES_arm := libs/arm32/librkaudio_aec_bf.a
LOCAL_SRC_FILES_arm64 := libs/arm64/librkaudio_aec_bf.a
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libs/include
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := rkaudio_effect_preprocess.xml
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)
LOCAL_SRC_FILES := rkaudio_effect_preprocess.xml
include $(BUILD_PREBUILT)

LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_HAVE_BLUETOOTH_BES),)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

BDROID_DIR := $(TOP_DIR)system/bt

LOCAL_CFLAGS += \
				-Wall \
				-Werror \
				-Wno-switch \
				-Wno-unused-function \
				-Wno-unused-parameter \
				-Wno-unused-variable

LOCAL_SRC_FILES := \
		         bes_mmap_write.cpp \
				 bes_mmap_read.cpp \
				 bes_mmap_utils.cpp

LOCAL_C_INCLUDES += \
				$(LOCAL_PATH)/../include

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libbinder \
        libbase \
        liblog

LOCAL_MODULE := libbt-bes-mmap
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

include $(BUILD_STATIC_LIBRARY)
endif # BOARD_HAVE_BLUETOOTH_BES

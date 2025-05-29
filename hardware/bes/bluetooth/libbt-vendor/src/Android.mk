LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_HAVE_BLUETOOTH_BES),)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

BDROID_DIR := $(TOP_DIR)system/bt
TINYALSA_DIR := $(TOP_DIR)external/tinyalsa

LOCAL_CFLAGS += \
				-Wall \
				-Werror \
				-Wno-switch \
				-Wno-unused-function \
				-Wno-unused-parameter \
				-Wno-unused-variable

LOCAL_SRC_FILES := \
        bt_vendor_bes.c \
        hardware.c \
        userial_vendor.c \
        upio.c \
        bes_list.c \
        bes_utils.c \
        bes_h5.c \
        bes_snd_io.c \
        bes_cqueue.c \
        tinyalsa/mixer.c \
        tinyalsa/pcm.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../include \
        $(BDROID_DIR)/hci/include \
        $(BDROID_DIR)/include \
        $(BDROID_DIR)/stack/include \
        $(BDROID_DIR)/device/include \
        $(LOCAL_PATH)/../codec/sbc \
        $(LOCAL_PATH)/../../libbt-bes-mmap/include \
        $(BDROID_DIR) \
        $(TOP_DIR)libhwbinder/include \
        $(TOP_DIR)frameworks/native/libs/binder/include


LOCAL_C_INCLUDES += $(bdroid_C_INCLUDES)
LOCAL_CFLAGS += $(bdroid_CFLAGS)

LOCAL_HEADER_LIBRARIES := libutils_headers
LOCAL_WHOLE_STATIC_LIBRARIES += libbt-codec

LOCAL_STATIC_LIBRARIES := libbt-bes-mmap
LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libbinder \
        liblog

LOCAL_MODULE := libbt-vendor-bes2600
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
endif # BOARD_HAVE_BLUETOOTH_BES

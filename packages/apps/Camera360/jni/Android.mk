LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    Semaphore.cpp \
    RkAroundCamera_JNI.cpp \

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \

LOCAL_SYSTEM_SHARED_LIBRARIES := \
    libc \
    libm

LOCAL_SHARED_LIBRARIES := \
    libcutils\
    libgui \
    libui \
    libhardware \
    liblog \
    libandroid \
    libandroidfw \
    libbinder

LOCAL_LDFLAGS_$(TARGET_ARCH) := \
    $(LOCAL_PATH)/lib64/libutils.so \
    $(LOCAL_PATH)/lib64/lib_render_3d.so

LOCAL_CFLAGS := -O2 -g -W -Wall -Wno-pointer-arith -Wno-format -Wno-unused-function -Wno-unused-parameter
LOCAL_LDLIBS := -ljnigraphics
LOCAL_MULTILIB:= 64
LOCAL_MODULE:= librkaroundcamera
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

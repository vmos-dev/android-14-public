LOCAL_PATH := $(call my-dir)

# libvman-audio.so
# =========================================================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := AudioModule.c

LOCAL_C_INCLUDES := \
	hardware/libhardware/include \
	$(LOCAL_PATH)/

LOCAL_HEADER_LIBRARIES += \
	libhardware_headers

LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_MODULE := vman_audio_hal.default
LOCAL_MULTILIB := both
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_SHARED_LIBRARIES := liblog
include $(BUILD_SHARED_LIBRARY)

# libvman-pq.so
# =========================================================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := PqModule.cpp

LOCAL_C_INCLUDES := \
        hardware/libhardware/include \
        $(LOCAL_PATH)/

LOCAL_HEADER_LIBRARIES += \
        libhardware_headers

LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_MODULE := vman_pq_hal.default
LOCAL_MULTILIB := both
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_SHARED_LIBRARIES := liblog \
                          libbinder \
                          libbinder_ndk \
                          libutils \
                          libcutils \
                          libhidlbase \
                          rockchip.hardware.outputmanager@1.0
include $(BUILD_SHARED_LIBRARY)

# libvman-display.so
# =========================================================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := DisplayModule.cpp

LOCAL_C_INCLUDES := \
        hardware/libhardware/include \
        $(LOCAL_PATH)/

LOCAL_HEADER_LIBRARIES += \
        libhardware_headers

LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_MODULE := vman_display_hal.default
LOCAL_MULTILIB := both
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_SHARED_LIBRARIES := liblog \
                          libbinder \
                          libbinder_ndk \
                          libutils \
                          libcutils \
                          libhidlbase \
                          rockchip.hardware.outputmanager@1.0
include $(BUILD_SHARED_LIBRARY)

# libvman-power.so
# =========================================================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := PowerModule.c

LOCAL_C_INCLUDES := \
        hardware/libhardware/include \
        $(LOCAL_PATH)/

LOCAL_HEADER_LIBRARIES += \
        libhardware_headers

LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_MODULE := vman_power_hal.default
LOCAL_MULTILIB := both
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_SHARED_LIBRARIES := liblog
include $(BUILD_SHARED_LIBRARY)

# libvman-sys.so
# =========================================================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := SysModule.cpp

LOCAL_C_INCLUDES := \
        hardware/libhardware/include \
        $(LOCAL_PATH)/

LOCAL_HEADER_LIBRARIES += \
        libhardware_headers

LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_MODULE := vman_sys_hal.default
LOCAL_MULTILIB := both
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_SHARED_LIBRARIES := liblog \
                          libbinder \
                          libbinder_ndk \
                          libutils \
                          libcutils \
                          libhidlbase \
                          rockchip.hardware.outputmanager@1.0
include $(BUILD_SHARED_LIBRARY)

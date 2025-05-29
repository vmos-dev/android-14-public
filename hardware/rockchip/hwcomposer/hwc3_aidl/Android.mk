# Copyright (C) 2021 The Android Open Source Project
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
include $(CLEAR_VARS)

soc_ver := $(TARGET_BOARD_PLATFORM)
LOCAL_MODULE := android.hardware.graphics.composer3-service.rockchip

LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/NOTICE

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += \
	-DSOC_VERSION=$(soc_ver) \
	-DLOG_TAG=\"hwc3\"

# hwc3 re-uses hwc2.2 ComposerResource and libexynosdisplay
LOCAL_SHARED_LIBRARIES := android.hardware.graphics.composer3-V2-ndk \
	android.hardware.graphics.composer@2.1-resources \
    android.hardware.graphics.composer@2.2-resources \
	android.hardware.graphics.composer@2.4 \
	libbase \
	libbinder \
	libbinder_ndk \
	libcutils \
	libfmq \
	libhardware \
	libhardware_legacy \
	liblog \
	libsync \
	libutils

LOCAL_STATIC_LIBRARIES := libaidlcommonsupport

LOCAL_HEADER_LIBRARIES := \
	android.hardware.graphics.composer3-command-buffer \
	google_hal_headers \
	libgralloc_headers

LOCAL_C_INCLUDES := \
	$(TOP)/hardware/rockchip/hwcomposer/drmhwc2/include \
	$(TOP)/hardware/rockchip/hwcomposer/hwc3/include

LOCAL_SRC_FILES := \
	Composer.cpp \
	ComposerClient.cpp \
	ComposerCommandEngine.cpp \
	impl/HalImpl.cpp \
	impl/ResourceManager.cpp \
	service.cpp

ifeq ($(BOARD_USES_HWC_SERVICES),true)
LOCAL_CFLAGS += -DUSES_HWC_SERVICES
LOCAL_HEADER_LIBRARIES += libbinder_headers
endif

ifeq ($(strip $(BOARD_USES_HWC_PROXY_SERVICE)),true)
LOCAL_CFLAGS += \
	-DUSE_HWC_PROXY_SERVICE=1
endif

LOCAL_VINTF_FRAGMENTS = hwc3-default.xml
LOCAL_INIT_RC := hwc3-rk.rc

include $(BUILD_EXECUTABLE)

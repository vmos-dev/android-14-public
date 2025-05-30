# Copyright (C) 2011 The Android Open Source Project
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

LOCAL_PATH := hardware/broadcom/wlan/bcmdhd/wifi_hal

# Make the HAL library
# ============================================================
include $(CLEAR_VARS)

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-format \
    -Wno-reorder \
    -Wno-unused-function \
    -Wno-unused-parameter \
    -Wno-unused-private-field \
    -Wno-unused-variable \
    -include unistd.h

LOCAL_C_INCLUDES += \
	external/libnl/include \
	$(call include-path-for, libhardware_legacy)/hardware_legacy \
	external/wpa_supplicant_8/src/drivers

LOCAL_C_INCLUDES += \
	external/boringssl/include \
	external/boringssl/src/crypto/digest \
	external/boringssl/src/crypto/evp/

LOCAL_HEADER_LIBRARIES := libutils_headers \
			  liblog_headers \
			  libnetlink_iface_headers \
			  libhardware_legacy_headers

LOCAL_SRC_FILES := \
	wifi_hal.cpp \
	rtt.cpp \
	common.cpp \
	cpp_bindings.cpp \
	gscan.cpp \
	nan.cpp \
	link_layer_stats.cpp \
	wifi_logger.cpp \
	wifi_offload.cpp \
	twt.cpp \
	scan.cpp

LOCAL_SHARED_LIBRARIES := \
	librkwifi-ctrl \
	libcrypto \
	libnl \
	libutils \
	libcutils \
	liblog

LOCAL_MODULE := libwifi-hal-bcm
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)


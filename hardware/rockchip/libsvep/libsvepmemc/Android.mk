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
ifeq ($(strip $(BOARD_USES_LIBSVEP)),true)
	BOARD_USES_LIBSVEP_MEMC := true
endif

ifeq ($(strip $(BOARD_USES_LIBSVEP_MEMC)),true)

TARGET_SOC_PLATFORM := rk3588

# RK3588
ifneq ($(filter rk3588, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk3588
endif

# RK356x
ifneq ($(filter rk356x, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk356x
endif

# SVEP lib
TARGET_SVEP_LIB_PATH := lib/Android/$(TARGET_SOC_PLATFORM)/

include $(CLEAR_VARS)
LOCAL_MODULE := libsvepmemc
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_SHARED_LIBRARIES := \
  libcutils \
  liblog \
  libui \
  libutils \
  libsync_vendor \
  librga \
  libOpenCL \
  librknnrt \
  libhidlbase \
  libz \
  libhardware \
  libEGL \
  libGLESv2 \
  libgralloctypes \
  android.hardware.graphics.mapper@4.0

LOCAL_PROPRIETARY_MODULE := true

LOCAL_REQUIRED_MODULES := \
	MemcOsd.ttf

ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := $(TARGET_SVEP_LIB_PATH)/$(TARGET_CPU_ABI)/libsvepmemc.so
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := $(TARGET_SVEP_LIB_PATH)/$(TARGET_2ND_CPU_ABI)/libsvepmemc.so
else
LOCAL_SRC_FILES_$(TARGET_ARCH) := $(TARGET_SVEP_LIB_PATH)/$(TARGET_CPU_ABI)/libsvepmemc.so
endif
LOCAL_CHECK_ELF_FILES := false
LOCAL_MODULE_SUFFIX := .so
include $(BUILD_PREBUILT)

## copy MemcOsd.ttf /vendor/etc/
include $(CLEAR_VARS)
LOCAL_MODULE := MemcOsd.ttf
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := resources/osd/MemcOsd.ttf
include $(BUILD_PREBUILT)

subdir_makefiles=$(call first-makefiles-under,$(LOCAL_PATH))
$(foreach mk,$(subdir_makefiles),$(info including $(mk) ...)$(eval include $(mk)))

endif

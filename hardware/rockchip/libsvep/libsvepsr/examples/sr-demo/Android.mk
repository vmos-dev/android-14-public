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

include $(CLEAR_VARS)
# RK3588
ifneq ($(filter rk3588, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk3588
endif

# RK356x
ifneq ($(filter rk356x, $(strip $(TARGET_BOARD_PLATFORM))), )
TARGET_SOC_PLATFORM := rk356x
endif

# SVEP lib
TARGET_SVEP_HEAD_PATH := ${LOCAL_PATH}/../../lib/Android/$(TARGET_SOC_PLATFORM)/include


LOCAL_MODULE := sr-demo
LOCAL_SHARED_LIBRARIES := \
  libcutils \
  liblog \
  libui \
  libutils \
  libsync_vendor \
  librga \
  libz \
  libOpenCL \
  librknnrt-svep \
  libsvepsr

LOCAL_C_INCLUDES := \
	${LOCAL_PATH}/include \
	${TARGET_SVEP_HEAD_PATH}

LOCAL_SRC_FILES :=  \
	src/AsyncWorker.cpp \
	src/main.cpp \
	src/worker.cpp

LOCAL_VENDOR_MODULE := true
LOCAL_CHECK_ELF_FILES := false
include $(BUILD_EXECUTABLE)
endif

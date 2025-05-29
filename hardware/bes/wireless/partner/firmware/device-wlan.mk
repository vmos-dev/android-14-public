#
# Copyright (C) 2008 The Android Open Source Project
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
#


-include hardware/bes/wireless/wlan/bes2600/config/config-bes.mk

FW_BASE_PATH := hardware/bes/wireless/partner/firmware/$(BOARD_USR_WIFI)
#firmware
PRODUCT_COPY_FILES += $(call find-copy-subdir-files,"*.bin",$(FW_BASE_PATH),$(TARGET_COPY_OUT_VENDOR)/etc/firmware)

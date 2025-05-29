#
# Copyright (C) 2008 The Android Open Source Project
# Copyright (C) 2023 Rockchip Corporation
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

########################

AICS_SUBDIRS := $(wildcard hardware/aic/firmware/aicsdio/*/)

AICS_DEST_DIRS := $(TARGET_COPY_OUT_VENDOR)/etc/firmware

PRODUCT_COPY_FILES += \
	$(foreach DIR, $(AICS_SUBDIRS), \
		$(foreach file, $(notdir $(shell ls $(DIR)/*.bin $(DIR)/*.txt)), \
			$(DIR)/$(file):$(AICS_DEST_DIRS)/$(file) \
		) \
	)

########################

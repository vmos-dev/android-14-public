/*
 * Copyright (C) 2022 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android-base/logging.h>

#include <cutils/properties.h>

#include "log.h"
#include "allocator.h"

using aidl::android::hardware::graphics::allocator::impl::arm::allocator;

static void set_properties_for_align_behavior_of_alloc_buffer()
{
	property_set("vendor.gralloc.is_rk_rule_not_supported", "1");
	property_set("vendor.gralloc.could_specify_byte_stride_as_rk_rule", "0");
	property_set("vendor.gralloc.could_specify_byte_stride_align", "1");
	property_set("vendor.gralloc.could_specify_alloc_height_align", "1");
}

int main()
{
	ABinderProcess_setThreadPoolMaxThreadCount(0);
	auto instance = ndk::SharedRefBase::make<allocator>();
	const std::string name = std::string() + allocator::descriptor + "/default";
	auto status = AServiceManager_addService(instance->asBinder().get(), name.c_str());
	CHECK_EQ(status, STATUS_OK);

	ALOGI("%s: %d: to set properties_for_align_behavior_of_alloc_buffer", __FUNCTION__, __LINE__);
	set_properties_for_align_behavior_of_alloc_buffer();

	ABinderProcess_joinThreadPool();
	return EXIT_FAILURE;
}

/*
 * Copyright (C) 2020, 2022. Arm Limited. All rights reserved.
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

#pragma once

#include "allocator_hidl_header.h"
#include "core/buffer_descriptor.h"

namespace arm
{
namespace allocator
{
using android::hardware::hidl_handle;
using android::hardware::Return;
using android::hardware::graphics::mapper::V4_0::BufferDescriptor;

class GrallocAllocator : public IAllocator
{
public:
	/**
	 * IAllocator constructor. All the state information required for the Gralloc
	 * private module is populated in its default constructor. Gralloc 4.0 specific
	 * state information can be populated here.
	 */
	GrallocAllocator() = default;

	/**
	 * IAllocator destructor. All the resources acquired for Gralloc private module
	 * are released
	 */
	virtual ~GrallocAllocator();

	/* Override IAllocator 4.0 interface */
	Return<void> allocate(const BufferDescriptor &descriptor, uint32_t count, allocate_cb hidl_cb) override;
};

} // namespace allocator
} // namespace arm

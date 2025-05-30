/*
 * Copyright (C) 2016, 2018-2022 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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

#include "core/buffer_descriptor.h"

/*
 * Creates a new private_handle_t with graphics memory to back it.
 *
 * @param descriptor     [in]    Request descriptor
 *
 * @return nullptr upon failure
 */
unique_private_handle allocator_allocate(const buffer_descriptor_t *descriptor);

/*
 * Signal start/end of CPU access to a allocated graphics memory.
 *
 * @param handle [in]   Buffer handle
 * @param read   [in]   Flag indicating CPU read access to memory
 * @param write  [in]   Flag indicating CPU write access to memory
 *
 * @return              0, on success; -errno, otherwise.
 */
int allocator_sync_start(const imported_handle *handle, bool read, bool write);
int allocator_sync_end(const imported_handle *handle, bool read, bool write);

int allocator_sync_start(const private_handle_t *handle, bool read, bool write);
int allocator_sync_end(const private_handle_t *handle, bool read, bool write);

int allocator_map(imported_handle *handle);
void allocator_unmap(imported_handle *handle);

void allocator_close();

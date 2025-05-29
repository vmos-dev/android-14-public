/*
 * Copyright (C) 2016 The Android Open Source Project
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
#include "utils/autofd.h"

#include <stdint.h>
#include <string>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

class SrAllocator
{
public:
    static SrAllocator* GetInstance();

    /* Not copyable or movable */
    SrAllocator(const SrAllocator&)            = delete;
    SrAllocator& operator=(const SrAllocator&) = delete;

    int Alloc(uint32_t stride, uint32_t h_stride, uint32_t fourcc_format,
              uint64_t usage, std::string requestorName, uint32_t* byte_stride,
              int* size, uint64_t* buffer_id);
    int Lock(int fd, int size, void** vaddr);
    int UnLock(void* vaddr, int size);
    int Free(uint64_t buffer_id);

private:
    SrAllocator();
    ~SrAllocator();
    int Init();
    int AllocDmaBufferHeap(int fd, size_t len, unsigned int flags,
                           int* dmabuf_fd);
    int GetDmaBufferHeapFd(uint64_t usage);
    int GetBufferLen(uint32_t stride, uint32_t height, uint32_t fourcc_format,
                     uint32_t* byte_stride);
    bool bValid_;
    /* Stores all open dmabuf_heap handles. */
    std::unordered_map<std::string, int> dmabuf_heap_fds_;
    /* Stores all alloc dma_buffer_fd. */
    std::unordered_map<uint64_t, int> alloc_fds_;
    int ion_fd_;
    /* Protects dma_buf_heap_fd_ from concurrent access */
    mutable std::mutex mtx_;
};
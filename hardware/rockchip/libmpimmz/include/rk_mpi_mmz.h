/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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
 *
 */

#ifndef __RK_MPI_MMZ_H__
#define __RK_MPI_MMZ_H__

#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef void*               MB_BLK;
typedef int                 RK_S32;
typedef uint32_t            RK_U32;
typedef uint64_t            RK_U64;
typedef void                RK_VOID;

typedef struct _rkMB_EXT_CONFIG_S {
    RK_VOID            *vaddr;
    RK_U64              paddr;
    RK_S32              fd;
    RK_U64              len;
    RK_VOID            *reserve1;
    RK_VOID            *reserve2;
} MB_EXT_CONFIG_S;

// 分配物理不连续的内存
#define RK_MMZ_ALLOC_TYPE_IOMMU     0x00000000
// 分配物理连续的内存
#define RK_MMZ_ALLOC_TYPE_CMA       0x00000001

// 分配的内存支持cache
#define RK_MMZ_ALLOC_CACHEABLE      0x00000000
// 分配的内存不支持cache
#define RK_MMZ_ALLOC_UNCACHEABLE    0x00000010
// 分配的内存地址需要在4G以内
#define RK_MMZ_ALLOC_DMA32          0x00000020

#define RK_MMZ_SYNC_READONLY        0x00000000
#define RK_MMZ_SYNC_WRITEONLY       0x00000001
#define RK_MMZ_SYNC_RW              0x00000002

/*
    申请buffer
    pBlk     返回分配的buffer信息
    u32Len   申请buffer的大小
    u32Flags 申请buffer类型
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_Alloc(MB_BLK *pBlk, RK_U32 u32Len, RK_U32 u32Flags);

/*
    释放buffer
 */
RK_S32 RK_MPI_MMZ_Free(MB_BLK mb);

/*
    获取物理地址
    对于物理连续内存，返回其物理地址
    对于非物理连续内存，返回-1
 */
RK_U64   RK_MPI_MMZ_Handle2PhysAddr(MB_BLK mb);

/*
    获取用户空间虚拟地址
    失败返回NULL
 */
RK_VOID *RK_MPI_MMZ_Handle2VirAddr(MB_BLK mb);

/*
    获取buffer的fd
    失败返回-1
 */
RK_S32   RK_MPI_MMZ_Handle2Fd(MB_BLK mb);

/*
    获取buffer大小
    失败返回 (RK_U64)-1
 */
RK_U64   RK_MPI_MMZ_GetSize(MB_BLK mb);

/*
    通过fd查找到对应的buffer
    成功 返回mb
    失败 返回NULL
 */
MB_BLK   RK_MPI_MMZ_Fd2Handle(RK_S32 fd);

/*
    通过vaddr查找到对应的buffer
    成功 返回mb
    失败 返回NULL
 */
MB_BLK   RK_MPI_MMZ_VirAddr2Handle(RK_VOID *pstVirAddr);

/*
    通过paddr查找到对应的buffer
    成功 返回mb
    失败 返回NULL
 */
MB_BLK   RK_MPI_MMZ_PhyAddr2Handle(RK_U64 paddr);

/*
    导入fd，生成新的MB_BLK
      fd是由其它进程通过binder传递过来
      MB_BLK使用完之后，需要调用RK_MPI_MMZ_Free释放内存
    成功 返回mb
    失败 返回NULL
 */
MB_BLK   RK_MPI_MMZ_ImportFD(RK_S32 fd, RK_U32 len);

/*
    查询buffer是否cacheable
    是  返回1
    否  返回0
    不确定  返回-1
 */
RK_S32 RK_MPI_MMZ_IsCacheable(MB_BLK mb);

/*
    flush cache, 在cpu访问前调用
      当offset等于0，且length等于0或者length是整块buffer大小的时候，执行full sync
      其它情况则执行partial sync
      partial sync要求offset和length是64bytes对齐
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_FlushCacheStart(MB_BLK mb, RK_U32 offset, RK_U32 length, RK_U32 flags);

/*
    flush cache, 在cpu访问前调用
      当offset等于0，且length等于0或者length是整块buffer大小的时候，执行full sync
      其它情况则执行partial sync
      partial sync要求offset和length是64bytes对齐
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_FlushCacheEnd(MB_BLK mb, RK_U32 offset, RK_U32 length, RK_U32 flags);

/*
    flush cache, 在cpu访问前调用，指定待刷新内存的虚拟地址及其长度
      当vaddr是buffer的起始地址，且length是整块buffer的大小，执行full sync
      其它情况则执行partial sync
      partial sync要求offset和length是64bytes对齐
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_FlushCacheVaddrStart(RK_VOID* vaddr, RK_U32 length, RK_U32 flags);

/*
    flush cache, 在cpu访问结束后调用，指定待刷新内存的虚拟地址及其长度
      当vaddr是buffer的起始地址，且length是整块buffer的大小，执行full sync
      其它情况则执行partial sync
      partial sync要求地址偏移和length是64bytes对齐
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_FlushCacheVaddrEnd(RK_VOID* vaddr, RK_U32 length, RK_U32 flags);

/*
    flush cache, 在cpu访问前调用，指定待刷新内存的物理地址及其长度
      当paddr是buffer的起始地址，且length是整块buffer的大小，执行full sync
      其它情况则执行partial sync
      partial sync要求地址偏移和length是64bytes对齐
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_FlushCachePaddrStart(RK_U64 paddr, RK_U32 length, RK_U32 flags);

/*
    flush cache, 在cpu访问前调用，指定待刷新内存的物理地址及其长度
      当paddr是buffer的起始地址，且length是整块buffer的大小，执行full sync
      其它情况则执行partial sync
      partial sync要求地址偏移和length是64bytes对齐
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MMZ_FlushCachePaddrEnd(RK_U64 paddr, RK_U32 length, RK_U32 flags);

/*
    把用户的内存加入到MMZ进行管理
    当这块内存不再使用时候，需调用RK_MPI_MMZ_Free释放内存
    pBlk             返回MB_BLK，其中存放着用户内存的信息
    pstMbExtConfig   由用户提供的内存信息
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_SYS_CreateMB(MB_BLK *pBlk, MB_EXT_CONFIG_S *pstMbExtConfig);

/*
    设置/获取buffer的宽高信息，给TDE使用
    成功  返回0
    失败  返回负值
 */
RK_S32 RK_MPI_MB_SetBufferStride(MB_BLK mb, RK_U32 u32HorStride, RK_U32 u32VerStride);
RK_S32 RK_MPI_MB_GetBufferStride(MB_BLK mb, RK_U32 *pu32HorStride, RK_U32 *pu32VerStride);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* __RK_MPI_MMZ_H__ */

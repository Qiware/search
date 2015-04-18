#if !defined(__SHM_SLAB__)
#define __SHM_SLAB__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>

#include "log.h"
#include "spinlock.h"

/* 内存分配方式 */
typedef enum
{
    SHM_SLAB_ALLOC_UNKNOWN,     /* 未知方式 */
    SHM_SLAB_ALLOC_SMALL,       /* 小于128字节 */
    SHM_SLAB_ALLOC_EXACT,       /* 等于128字节 */
    SHM_SLAB_ALLOC_LARGE,       /* 大于128字节，小于2048字节 */
    SHM_SLAB_ALLOC_PAGES,       /* 按整页分配 */

    SHM_SLAB_ALLOC_TOTAL
} SHM_SLAB_ALLOC_TYPE;

/* SLOT管理 */
typedef struct
{
    uint32_t index;             /* slot索引 */
    int page_idx;               /* slot链表的页索引 */
} shm_slab_slot_t;

/* PAGE管理 */
typedef struct
{
    uint32_t index;             /* 页索引 */
    uint32_t pages;             /* 记录页数组的页数 */
    uint32_t shift:16;          /* 页分配的大小对应的位移 */
    uint32_t type:16;           /* 页分配的类型: 取值范围SHM_SLAB_ALLOC_TYPE */
    uint32_t bitmap;            /* 页的使用位图 */
    uint32_t rbitmap;           /* 初始位图的取反值 */
    int next_idx;               /* 下一页的索引 */
    int prev_idx;               /* 上一页的索引 */
} shm_slab_page_t;

typedef struct
{
    spinlock_t lock;            /* 锁 */

    size_t pool_size;           /* 内存空间总大小 */
    
    size_t min_size;            /* 最小分配单元 */
    int min_shift;              /* 最小分配单元的位移 */

    size_t base_offset;         /* 基础偏移量 */
    size_t data_offset;         /* 可分配空间的起始偏移量 */
    size_t end_offset;          /* 可分配空间的结束偏移量 */

    size_t slot_offset;         /* SLOT数组的起始偏移量 */
    size_t page_offset;         /* PAGE数组的起始偏移量 */
	
    shm_slab_page_t free;       /* 空闲页链表 */
} shm_slab_pool_t;

int32_t shm_slab_init(shm_slab_pool_t *pool);
void *shm_slab_alloc(shm_slab_pool_t *pool, size_t size);
void shm_slab_dealloc(shm_slab_pool_t *pool, void *p);

size_t shm_slab_head_size(size_t size);
#endif /*__SHM_SLAB__*/

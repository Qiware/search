/******************************************************************************
 ** 内存池算法的空间布局:
 **     ---------- -------- -----------------------------------------------
 **    |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **    |\基础信息\|X页对象X|///////可///////用////////空////////间/////////|
 **    |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **     ---------- -------- -----------------------------------------------
 **    ^          ^        ^                                               ^
 **    |          |        |                                               |
 **   addr       page     data                                            end
 *******************************************************************************/
#if !defined(__SHM_POOL_H__)
#define __SHM_POOL_H__

#include <stdint.h>
#include <spinlock.h>

#define SHM_POOL_PAGE_SLOT_NUM  (256) /* 单页的内存块数目(注意:为32的整数倍) */
#define SHM_POOL_BITMAP_MAX    SHM_POOL_PAGE_SLOT_NUM >> 5

/* 页对象 */
typedef struct
{
    spinlock_t lock;                /* SPIN锁对象 */
    uint32_t bitmap_num;            /* 实际使用的BITMAP数 */
    uint32_t bitmap[SHM_POOL_BITMAP_MAX];  /* 分配位图(0:未使用 1:使用) */
    size_t data_off;               /* 相对于data的偏移 */
} shm_pool_page_t;

/* 数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    int page_num;                   /* 总页数 */
    size_t unit_size;               /* 各内存块SIZE */
    size_t page_size;               /* 各页SIZE */

    size_t page_off;                /* 页对象 */
    size_t data_off;                /* 可支配空间 */
} shm_pool_info_t;

/* 全局对象 */
typedef struct
{
    shm_pool_info_t *info;          /* 基础信息 */
    shm_pool_page_t *page;          /* 页对象地址 */
    void *addr;                     /* 起始地址 */
    void **page_data;               /* 各页可分配地址 */
} shm_pool_t;

/* 共享队列总空间 */
#define shm_pool_total(max, unit_size) \
    sizeof(shm_pool_t) \
    + div_ceiling(max, SHM_POOL_PAGE_SLOT_NUM) * sizeof(shm_pool_page_t) \
    + max * size;

shm_pool_t *shm_pool_init(void *start, int max, size_t unit_size);
shm_pool_t *shm_pool_get(void *addr);
void *shm_pool_alloc(shm_pool_t *pool);
void shm_pool_dealloc(shm_pool_t *pool, void *p);
void shm_pool_destroy(shm_pool_t *pool);

#endif /*__SHM_POOL_H__*/

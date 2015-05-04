#if !defined(__CHUNK_H__)
#define __CHUNK_H__

#include <stdint.h>
#include "spinlock.h"

#define CHUNK_PAGE_SLOT_NUM  (256) /* 单页的内存块数目 */

/* 页对象 */
typedef struct
{
    spinlock_t lock;                /* SPIN锁对象 */

    uint32_t bitmaps;               /* 位图长度 */
    uint32_t *bitmap;               /* 分配位图(0:未使用 1:使用) */
    void *addr;                     /* 各页内存起始地址(指向chunk_t->addr中的内存空间) */
} chunk_page_t;

/* 内存数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    uint32_t pages;                 /* 总页数 */
    size_t size;                    /* 各内存块SIZE(size of slot) */

    void *addr;                     /* 总内存起始地址 */

    chunk_page_t *page;           /* 内存页 */

    /* 计算而来 */
    size_t page_size;               /* 页尺寸: MEM_BLK_PAGE_SLOT_NUM * size */
} chunk_t;

chunk_t *chunk_creat(int num, size_t size);
void *chunk_alloc(chunk_t *blk);
void chunk_dealloc(chunk_t *blk, void *p);
void chunk_destroy(chunk_t *blk);

#endif /*__CHUNK_H__*/

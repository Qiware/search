#if !defined(__MEMBLK_H__)
#define __MEMBLK_H__

#include <stdint.h>
#include "ticket_lock.h"

#define MEMBLK_PAGE_SLOT_NUM  (256) /* 单页的内存块数目 */

/* 页对象 */
typedef struct
{
    ticketlock_t lock;              /* SPIN锁对象 */

    uint32_t bitmaps;               /* 位图长度 */
    uint32_t *bitmap;               /* 分配位图(0:未使用 1:使用) */
    void *addr;                     /* 各页内存起始地址(指向memblk_t->addr中的内存空间) */
} memblk_page_t;

/* 内存数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    uint32_t pages;                 /* 总页数 */
    size_t size;                    /* 各内存块SIZE */

    void *addr;                     /* 总内存起始地址 */

    memblk_page_t *page;         /* 内存页 */
} memblk_t;

memblk_t *memblk_creat(int num, size_t size);
void *memblk_alloc(memblk_t *blk);
void memblk_dealloc(memblk_t *blk, void *p);
void memblk_destroy(memblk_t *blk);

#endif /*__MEMBLK_H__*/

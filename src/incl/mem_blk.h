#if !defined(__MEM_BLK_H__)
#define __MEM_BLK_H__

#include <stdint.h>
#include "ticket_lock.h"

#define MEM_BLK_PAGE_SLOT_NUM  (256) /* 单页的内存块数目 */

/* 页对象 */
typedef struct
{
    ticketlock_t lock;              /* SPIN锁对象 */

    uint32_t bitmaps;               /* 位图长度 */
    uint32_t *bitmap;               /* 分配位图(0:未使用 1:使用) */
    void *addr;                     /* 各页内存起始地址(指向mem_blk_t->addr中的内存空间) */
} mem_blk_page_t;

/* 内存数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    uint32_t pages;                 /* 总页数 */
    size_t size;                    /* 各内存块SIZE */

    void *addr;                     /* 总内存起始地址 */

    mem_blk_page_t *page;         /* 内存页 */
} mem_blk_t;

mem_blk_t *mem_blk_creat(int num, size_t size);
void *mem_blk_alloc(mem_blk_t *blk);
void mem_blk_dealloc(mem_blk_t *blk, void *p);
void mem_blk_destroy(mem_blk_t *blk);

#endif /*__MEM_BLK_H__*/

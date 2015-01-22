#if !defined(__SHM_BLOCK_H__)
#define __SHM_BLOCK_H__

#include <stdint.h>
#include <spinlock.h>

#define SHM_BLOCK_PAGE_SLOT_NUM  (256) /* 单页的内存块数目(注意:为32的整数倍) */

/* 页对象 */
typedef struct
{
    spinlock_t lock;                /* SPIN锁对象 */

    int bitmaps;                    /* 位图长度 */
    uint32_t bitmap[SHM_BLOCK_PAGE_SLOT_NUM/32];  /* 分配位图(0:未使用 1:使用) */
    void *addr;                     /* 各页内存起始地址(指向shm_block_t->addr中的内存空间) */
} shm_block_page_t;

/* 内存数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    int pages;                      /* 总页数 */
    size_t size;                    /* 各内存块SIZE */

    void *addr;                     /* 总内存起始地址 */

    shm_block_page_t *page;         /* 内存页 */
} shm_block_t;

shm_block_t *shm_block_creat(int num, size_t size);
void *shm_block_alloc(shm_block_t *blk);
void shm_block_dealloc(shm_block_t *blk, void *p);
void shm_block_destroy(shm_block_t *blk);

#endif /*__SHM_BLOCK_H__*/

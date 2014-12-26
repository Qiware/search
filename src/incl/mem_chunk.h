#if !defined(__MEM_CHUNK_H__)
#define __MEM_CHUNK_H__

#include <stdint.h>
#include <spinlock.h>

#define MEM_CHUNK_PAGE_SLOT_NUM  (256) /* 单页的内存块数目 */

/* 页对象 */
typedef struct
{
    spinlock_t lock;                /* SPIN锁对象 */

    int bitmaps;                    /* 位图长度 */
    uint32_t *bitmap;               /* 分配位图(0:未使用 1:使用) */
    void *addr;                     /* 各页内存起始地址(指向mem_chunk_t->addr中的内存空间) */
} mem_chunk_page_t;

/* 内存数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    int pages;                      /* 总页数 */
    size_t size;                    /* 各内存块SIZE */

    void *addr;                     /* 总内存起始地址 */

    mem_chunk_page_t *page;         /* 内存页 */
} mem_chunk_t;

mem_chunk_t *mem_chunk_creat(int num, size_t size);
void *mem_chunk_alloc(mem_chunk_t *chunk);
void mem_chunk_dealloc(mem_chunk_t *chunk, void *p);
void mem_chunk_destroy(mem_chunk_t *chunk);

#endif /*__MEM_CHUNK_H__*/

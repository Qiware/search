#if !defined(__MEM_CHUNK_H__)
#define __MEM_CHUNK_H__

#include <stdint.h>
#include <spinlock.h>

/* 内存数据块对象 */
typedef struct
{
    int num;                        /* Chunk总数 */
    size_t size;                    /* 各Chunk大小 */

    int bitmap_num;                 /* 位图长度 */
    uint32_t *bitmap;               /* 分配位图(0:未使用 1:使用) */
    void *addr;                     /* 内存起始地址 */
    spinlock_t lock;                /* SPIN锁对象 */
} mem_chunk_t;

mem_chunk_t *mem_chunk_init(int num, size_t size);
void *mem_chunk_alloc(mem_chunk_t *chunk);
void mem_chunk_dealloc(mem_chunk_t *chunk, void *p);
void mem_chunk_destroy(mem_chunk_t *chunk);

#endif /*__MEM_CHUNK_H__*/

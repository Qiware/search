/******************************************************************************
 ** 内存池算法的空间布局:
 **     ------ -------- -----------------------------------------------
 **    |\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **    |\对象\|X页对象X|///////可///////用////////空////////间/////////|
 **    |\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **     ------ -------- -----------------------------------------------
 **    ^      ^        ^                                               ^
 **    |      |        |                                               |
 **   addr   page     data                                            end
 *******************************************************************************/
#if !defined(__SHM_BLOCK_H__)
#define __SHM_BLOCK_H__

#include <stdint.h>
#include <spinlock.h>

#define SHM_BLOCK_PAGE_SLOT_NUM  (256) /* 单页的内存块数目(注意:为32的整数倍) */

/* 页对象 */
typedef struct
{
    spinlock_t lock;                /* SPIN锁对象 */
    uint32_t bitmap_num;            /* 实际使用的BITMAP数 */
    uint32_t bitmap[SHM_BLOCK_PAGE_SLOT_NUM/32];  /* 分配位图(0:未使用 1:使用) */
    size_t data_off;               /* 相对于data的偏移 */
} shm_block_page_t;

/* 数据块对象 */
typedef struct
{
    int num;                        /* 内存块总数 */
    int page_num;                   /* 总页数 */
    size_t unit_size;               /* 各内存块SIZE */

    size_t page_off;                /* 页对象 */
    size_t data_off;                /* 可支配空间 */
} shm_block_t;

shm_block_t *shm_block_creat(const char *path, int num, size_t unit_size);
void *shm_block_alloc(shm_block_t *blk);
void shm_block_dealloc(shm_block_t *blk, void *p);
void shm_block_destroy(shm_block_t *blk);

#endif /*__SHM_BLOCK_H__*/

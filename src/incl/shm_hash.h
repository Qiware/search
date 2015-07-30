#if !defined(__SHM_HASH_H__)
#define __SHM_HASH_H__

#include "spinlock.h"
#include "shm_list.h"
#include "shm_ring.h"

/*
 *    ------------------------------------------------------------------------
 *   |          |            |                |                               |
 *   | 头部信息 | 哈  希  槽 | 结  点  单  元 |   数      据      单      元  |
 *   |          |            |                |                               |
 *    ------------------------------------------------------------------------
 *   ^          ^            ^                ^
 *   |          |            |                |
 *  addr       slot         node             data
 */

/* 哈希槽 */
typedef struct
{
    spinlock_t lock;                /* 链表锁 */
    shm_list_t list;                /* 链表 */
} shm_hash_slot_t;

/* 头部信息 */
typedef struct
{
    int len;                        /* 哈希数组长度 */
    int max;                        /* 结点最大个数 */
    size_t size;                    /* 数据结点单元大小 */
    off_t slot_off;                 /* 哈希槽起始偏移 */
    off_t node_off;                 /* 结点单元起始偏移 */
    off_t data_off;                 /* 数据单元起始偏移 */
} shm_hash_head_t;

/* 哈希表 */
typedef struct
{
    void *addr;                     /* 首地址 */
    shm_hash_head_t *head;          /* 头部信息 */
    shm_hash_slot_t *slot;          /* 哈希槽数组(其长度为head->len) */
    shm_ring_t *node_pool;          /* 链表结点内存池(用于链表结点空间的申请和回收)
                                       未被使用的结点在此队列中 */
    shm_ring_t *data_pool;          /* 数据结点内存池(用于数据结点空间的申请和回收)
                                       未被使用的结点在此队列中 */
} shm_hash_t;

shm_hash_t *shm_hash_creat(const char *path, int len, int max, size_t size);
void *shm_hash_alloc(shm_hash_t *sh);
void shm_hash_dealloc(shm_hash_t *sh, void *addr);

int shm_hash_push(shm_hash_t *sh, void *key, int len, void *data);
void *shm_hash_pop(shm_hash_t *sh, void *key, int len, cmp_cb_t cmp_cb);

#endif /*__SHM_HASH_H__*/

#if !defined(__HASH_MAP_H__)
#define __HASH_MAP_H__

#include "comm.h"
#include "lock.h"

/* 选线 */
typedef struct
{
    void *pool;                                     /* 内存池 */
    mem_alloc_cb_t alloc;                           /* 申请内存 */
    mem_dealloc_cb_t dealloc;                       /* 释放内存 */
} hash_map_opt_t;

/* 哈希数组 */
typedef struct
{
    int len;                                        /* 数组长 */
    uint64_t total;                                 /* 数据总数 */

    void **tree;                                    /* 树(长度: len) */
    pthread_rwlock_t *lock;                         /* 树锁(长度: len) */

    key_cb_t key_cb;                                /* 生成KEY的回调 */
    cmp_cb_t cmp_cb;                                /* 比较回调 */

    /* 内存池 */
    struct {
        void *pool;                                 /* 内存池 */
        mem_alloc_cb_t alloc;                       /* 申请内存 */
        mem_dealloc_cb_t dealloc;                   /* 释放内存 */
    };
} hash_map_t;

hash_map_t *hash_map_creat(int len, key_cb_t key_cb, cmp_cb_t cmp_cb, hash_map_opt_t *opt);
int hash_map_insert(hash_map_t *htab, void *data, lock_e lock);
void *hash_map_query(hash_map_t *hmap, void *key, lock_e lock);
void hash_map_unlock(hash_map_t *map, void *key, lock_e lock);
void *hash_map_delete(hash_map_t *htab, void *key, lock_e lock);
int hash_map_trav(hash_map_t *htab, trav_cb_t proc, void *args, lock_e lock);
int hash_map_destroy(hash_map_t *htab, mem_dealloc_cb_t dealloc, void *args);

#define hash_map_total(htab) ((htab)->total)

#endif /*__HASH_MAP_H__*/

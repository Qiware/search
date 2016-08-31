#if !defined(__HASH_MAP_H__)
#define __HASH_MAP_H__

#include "comm.h"

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

    void **tree;                                    /* 树(数组成员: mod个) */
    pthread_rwlock_t *lock;                         /* 树锁 */

    key_cb_t key_cb;                                /* 生成KEY的回调 */
    cmp_cb_t cmp_cb;                                /* 比较回调 */

    uint64_t total;                                 /* 数据总数 */

    /* 内存池 */
    void *pool;                                     /* 内存池 */
    mem_alloc_cb_t alloc;                           /* 申请内存 */
    mem_dealloc_cb_t dealloc;                       /* 释放内存 */

    /* 操作接口 */
    tree_insert_cb_t insert;
    tree_delete_cb_t delete;
    tree_query_cb_t query;
    tree_trav_cb_t trav;
    tree_destroy_cb_t destroy;
} hash_map_t;

hash_map_t *hash_map_creat(int len, key_cb_t key_cb, cmp_cb_t cmp_cb, hash_map_opt_t *opt);
int hash_map_insert(hash_map_t *htab, void *data);
int hash_map_query(hash_map_t *htab, void *key, copy_cb_t copy, void *data);
void *hash_map_delete(hash_map_t *htab, void *key);
int hash_map_trav(hash_map_t *htab, trav_cb_t proc, void *args);
int hash_map_destroy(hash_map_t *htab, mem_dealloc_cb_t dealloc, void *args);

#define hash_map_total(htab) ((htab)->total)

#endif /*__HASH_MAP_H__*/

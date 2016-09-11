#if !defined(__HASH_TAB_H__)
#define __HASH_TAB_H__

#include "comm.h"
#include "lock.h"

/* 选线 */
typedef struct
{
    void *pool;                                     /* 内存池 */
    mem_alloc_cb_t alloc;                           /* 申请内存 */
    mem_dealloc_cb_t dealloc;                       /* 释放内存 */
} hash_tab_opt_t;

/* 哈希数组 */
typedef struct
{
    int len;                                        /* 数组长 */
    uint64_t total;                                 /* 数据总数 */

    void **tree;                                    /* 树(长度: len) */
    pthread_rwlock_t *lock;                         /* 树锁(长度: len) */

    cmp_cb_t cmp;                                   /* 比较回调 */
    hash_cb_t hash;                                 /* 生成哈系值的回调 */

    /* 内存池 */
    struct {
        void *pool;                                 /* 内存池 */
        mem_alloc_cb_t alloc;                       /* 申请内存 */
        mem_dealloc_cb_t dealloc;                   /* 释放内存 */
    };
} hash_tab_t;

hash_tab_t *hash_tab_creat(int len, hash_cb_t hash, cmp_cb_t cmp, hash_tab_opt_t *opt);
int hash_tab_insert(hash_tab_t *htab, void *data, lock_e lock);
void *hash_tab_query(hash_tab_t *htab, void *key, lock_e lock);
void hash_tab_unlock(hash_tab_t *htab, void *key, lock_e lock);
void *hash_tab_delete(hash_tab_t *htab, void *key, lock_e lock);
int hash_tab_trav(hash_tab_t *htab, trav_cb_t proc, void *args, lock_e lock);
int hash_tab_destroy(hash_tab_t *htab, mem_dealloc_cb_t dealloc, void *args);

#define hash_tab_total(htab) ((htab)->total)

#endif /*__HASH_TAB_H__*/

#if !defined(__HASH_TAB_H__)
#define __HASH_TAB_H__

#include "comm.h"

typedef int (*hash_tab_query_cb_t)(void *data, void *out);

/* 选线 */
typedef struct
{
#define HASH_TAB_AVL    (0)                         /* 平衡二叉树 */
#define HASH_TAB_RBT    (1)                         /* 红黑树 */
    int tree_type;                                  /* 树类型 */

    void *pool;                                     /* 内存池 */
    mem_alloc_cb_t alloc;                           /* 申请内存 */
    mem_dealloc_cb_t dealloc;                       /* 释放内存 */
} hash_tab_opt_t;

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
    tree_insert_cb_t tree_insert;
    tree_delete_cb_t tree_delete;
    tree_query_cb_t tree_query;
    tree_trav_cb_t tree_trav;
    tree_destroy_cb_t tree_destroy;
} hash_tab_t;

hash_tab_t *hash_tab_creat(int mod, key_cb_t key_cb, cmp_cb_t cmp_cb, hash_tab_opt_t *opt);
int hash_tab_insert(hash_tab_t *htab, void *pkey, int pkey_len, void *addr);
int hash_tab_query(hash_tab_t *htab, void *pkey, int pkey_len, hash_tab_query_cb_t query_cb, void *data);
void *hash_tab_remove(hash_tab_t *htab, void *pkey, int pkey_len);
int hash_tab_destroy(hash_tab_t *htab, mem_dealloc_cb_t dealloc, void *args);
int hash_tab_trav(hash_tab_t *htab, trav_cb_t proc, void *args);

#define hash_tab_total(htab) ((htab)->total)

#endif /*__HASH_TAB_H__*/

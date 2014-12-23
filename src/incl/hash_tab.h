#if !defined(__HASH_TAB_H__)
#define __HASH_TAB_H__

#include <pthread.h>

#include "avl_tree.h"

/* 哈希数组 */
typedef struct
{
    int num;                    /* 结点数 */
    avl_tree_t **tree;          /* 平衡二叉树(数组成员: num个) */
    pthread_rwlock_t *lock;     /* 平衡二叉树锁 */

    key_cb_t key_cb;            /* 生成KEY的回调 */
    avl_cmp_cb_t cmp_cb;        /* 比较回调 */
} hash_tab_t;

hash_tab_t *hash_tab_creat(slab_pool_t *slab, int mod, key_cb_t key_cb, avl_cmp_cb_t cmp_cb);
int hash_tab_insert(hash_tab_t *hash, void *pkey, int pkey_len, void *addr);
int hash_tab_query(hash_tab_t *hash, void *pkey, int pkey_len, void *data, int data_len);
void *hash_tab_remove(hash_tab_t *hash, void *pkey, int pkey_len);
int hash_tab_destroy(hash_tab_t *hash);
int hash_tab_trav(hash_tab_t *hash, avl_trav_cb_t cb, void *args);

#endif /*__HASH_TAB_H__*/

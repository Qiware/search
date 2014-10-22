#if !defined(__HASH_TAB_H__)
#define __HASH_TAB_H__

#include "avl_tree.h"

/* 哈希数组 */
typedef struct
{
    int num;                    /* 结点数 */
    avl_tree_t **tree;          /* 平衡二叉树(数组成员: num个) */

    avl_key_cb_t key;
    avl_cmp_cb_t cmp;
} hash_tab_t;

hash_tab_t *hash_tab_init(int num, avl_key_cb_t key, avl_cmp_cb_t cmp);
int hash_tab_insert(hash_tab_t *hash, avl_unique_t *unique, void *addr);
void *hash_tab_search(hash_tab_t *hash, avl_unique_t *unique);
void *hash_tab_delete(hash_tab_t *hash, avl_unique_t *unique);
int hash_tab_destroy(hash_tab_t *hash);

#endif /*__HASH_TAB_H__*/

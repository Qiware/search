#if !defined(__HASH_ARRAY_H__)
#define __HASH_ARRAY_H__

#include "comm.h"
#include "slab.h"
#include "rb_tree.h"

/* 哈希结点 */
typedef struct
{
    int key;                    /* KEY值 */
    void *data;                 /* 数据地址 */
    rbt_tree_t *tree;           /* 红黑树 */
} hash_node_t;

/* 哈希数组 */
typedef struct
{
    int num;                    /* 结点数 */
    hash_node_t *node;          /* 结点数组 */
    slab_pool_t *slab;          /* Slab内存机制 */

    key_cb_t key_cb;            /* 计算KEY值的函数 */
} hash_array_t;

hash_array_t *hash_array_init(int num, size_t slab_size, key_cb_t key_cb);
int hash_array_insert(hash_array_t *hash, int key, void *addr);
void *hash_array_search(hash_array_t *hash, int key);
void *hash_array_remove(hash_array_t *hash, int key);
int hash_array_destroy(hash_array_t *hash);

#endif /*__HASH_ARRAY_H__*/

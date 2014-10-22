#if !defined(__HASH_ARRAY_H__)
#define __HASH_ARRAY_H__

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
    eslab_pool_t pool;          /* Slab内存机制 */

    uint32_t (*key)(const char *str, size_t len);  /* 计算KEY值的函数 */
} hash_array_t;

hash_array_t *hash_array_init(int num, uint32_t (*key)(const char *str, size_t len));
int hash_array_insert(hash_array_t *hash, int key, void *addr);
void *hash_array_search(hash_array_t *hash, int key);
void *hash_array_delete(hash_array_t *hash, int key);
int hash_array_destroy(hash_array_t *hash);

#endif /*__HASH_ARRAY_H__*/

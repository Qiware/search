#if !defined(__TRIE_H__)
#define __TRIE_H__

#include "comm.h"

/* 选项 */
typedef struct
{
    void *pool;                         /* 内存池 */
    mem_alloc_cb_t alloc;               /* 申请空间 */
    mem_dealloc_cb_t dealloc;           /* 释放空间 */
} trie_opt_t;

/* 键树的结点 */
typedef struct _kwt_node_t
{
    u_char key;                         /* 键值 */
    void *data;                         /* 结点数据 */
    struct _kwt_node_t *child;          /* 后续节点 */
} trie_node_t;

/* 键树 */
typedef struct
{
    int max;                            /* 结点个数(必须为2的次方) */
    trie_node_t *root;                  /* 键树根结点 */

    /* 选项 */
    void *pool;                         /* 内存池 */
    mem_alloc_cb_t alloc;               /* 申请空间 */
    mem_dealloc_cb_t dealloc;           /* 释放空间 */
} trie_tree_t;

trie_tree_t *trie_creat(trie_opt_t *opt);
int trie_insert(trie_tree_t *tree, const u_char *str, int len, void *data);
int trie_query(trie_tree_t *tree, const u_char *str, int len, void **data);
void trie_print(trie_tree_t *kwt);
void trie_destroy(trie_tree_t *tree, void *mempool, mem_dealloc_cb_t dealloc);

#endif /*__TRIE_H__*/

#if !defined(__B_TREE_H__)
#define __B_TREE_H__

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

/* B树结点 */
typedef struct _btree_node_t
{
    int num;                                        /* 关键字数 */
    int *key;                                       /* 关键字指针 */
    struct _btree_node_t **child;                   /* 孩子结点 */
    struct _btree_node_t *parent;                   /* 父亲结点 */
}btree_node_t;

/* B树结构 */
typedef struct
{
    int max;                                        /* 关键字最大个数 */
    int min;                                        /* 最小关键字个数 */
    int sep_idx;                                    /* 结点分化的分割索引 */
    btree_node_t *root;                             /* 根结点 */
}btree_t;

extern btree_t *btree_creat(int m);
extern int btree_insert(btree_t *btree, int key);
extern int btree_remove(btree_t *btree, int key);
extern int btree_destroy(btree_t **btree);
void _btree_print(const btree_node_t *node, int deep);
#define btree_print(btree) /* 打印B树 */\
{ \
    if (NULL != btree->root) \
    { \
        _btree_print(btree->root, 0); \
    } \
}

#endif /*__B_TREE_H__*/

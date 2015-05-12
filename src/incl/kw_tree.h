#if !defined(__KW_TREE_H__)
#define __KW_TREE_H__

#include "comm.h"

/* KW树的结点 */
typedef struct _kwt_node_t
{
    unsigned char key;                  /* 键值 */
    void *data;                         /* 结点数据 */
    struct _kwt_node_t *child;          /* 后续节点 */
} kwt_node_t;

/* KW树 */
typedef struct
{
    int max;                            /* 结点个数(必须为2的次方) */
    kwt_node_t *root;                   /* KW树根结点 */
} kwt_tree_t;

kwt_tree_t *kwt_creat(void);
int kwt_insert(kwt_tree_t *tree, const unsigned char *str, int len, void *data);
int kwt_search(kwt_tree_t *tree, const unsigned char *str, int len, void **data);
void kwt_destroy(kwt_tree_t *tree, void *mempool, mem_dealloc_cb_t dealloc);

#endif /*__KW_TREE_H__*/

#if !defined(__KW_TREE_H__)
#define __KW_TREE_H__

#include "comm.h"

/* KW树的结点 */
typedef struct _kwt_node_t
{
    void *data;                         /* 结点数据 */
    struct _kwt_node_t *next;     /* 后一个结点 */
} kwt_node_t;

/* KW树 */
typedef struct
{
    int count;                          /* Unit中结点个数 */
    kwt_node_t *root;             /* KW树根结点 */
} kwt_tree_t;

kwt_tree_t *kwt_creat(int count);
int kwt_insert(kwt_tree_t *tree, const char *str, int len);
const kwt_node_t *kwt_search(kwt_tree_t *tree, const char *str, int len);
void kwt_destroy(kwt_tree_t **tree);

#endif /*__KW_TREE_H__*/

#if !defined(__AVL_TREE_H__)
#define __AVL_TREE_H__

#include "comm.h"
#include "stack.h"

#define AVL_RH    (-1)          /* 右子树增高 */
#define AVL_EH    (0)           /* 高度未发生变化 */
#define AVL_LH    (1)           /* 左子树增高 */

#define AVL_MAX_DEPTH   (256)

typedef enum
{
    AVL_OK                      /* 成功 */

    , AVL_ERR = ~0x7FFFFFFF     /* 失败 */
    , AVL_NODE_EXIST            /* 节点已存在 */
    , AVL_ERR_STACK             /* 栈异常 */
    , AVL_ERR_NOT_FOUND         /* 未找到 */
} avl_err_e;

/* 选项 */
typedef struct
{
    void *pool;                 /* 内存池 */
    mem_alloc_cb_t alloc;       /* 申请内存 */
    mem_dealloc_cb_t dealloc;   /* 释放内存 */
} avl_opt_t;

/* 节点结构 */
typedef struct _node_t
{
    int bf;                     /* 平衡因子 */

    struct _node_t *parent;     /* 父节点 */
    struct _node_t *lchild;     /* 左孩子 */
    struct _node_t *rchild;     /* 右孩子 */

    void *data;                 /* 附加数据(注: 必须包含主键) */
} avl_node_t;

/* 树结构 */
typedef struct
{
    uint64_t num;               /* 结点数 */
    avl_node_t *root;           /* 根节点 */

    cmp_cb_t cmp_cb;            /* 数值比较回调 */

    void *pool;                 /* 内存池 */
    mem_alloc_cb_t alloc;       /* 申请内存 */
    mem_dealloc_cb_t dealloc;   /* 释放内存 */
} avl_tree_t;

avl_tree_t *avl_creat(avl_opt_t *opt, cmp_cb_t cmp_cb);
int avl_insert(avl_tree_t *tree, void *data);
void *avl_query(avl_tree_t *tree, void *key);
int avl_delete(avl_tree_t *tree, void *key, void **data);
int avl_print(avl_tree_t *tree);
int avl_trav(avl_tree_t *tree, trav_cb_t proc, void *args);
void *avl_find(avl_tree_t *tree, find_cb_t find, void *args);
void avl_destroy(avl_tree_t *tree, mem_dealloc_cb_t dealloc_cb, void *args);
#define avl_isempty(tree) (NULL == (tree)->root)
#define avl_num(tree) ((tree)->num)

/* 测试使用 */
void avl_assert(const avl_node_t *node);

#endif /*__AVL_TREE_H__*/

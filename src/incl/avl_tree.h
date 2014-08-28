#if !defined(__AVL_TREE_H__)
#define __AVL_TREE_H__

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <memory.h>

#include "stack.h"
#include "xdt_comm.h"

#define AVL_RH    (-1)              /* 右子树增高 */
#define AVL_EH    (0)               /* 高度未发生变化 */
#define AVL_LH    (1)               /* 左子树增高 */

#define AVL_MAX_DEPTH   (256)

typedef enum
{
    AVL_SUCCESS                 /* 成功 */

    , AVL_FAILED = ~0x7FFFFFFF  /* 失败 */
    , AVL_NODE_EXIST            /* 节点存在 */
    , AVL_ERR_STACK             /* 栈异常 */
    , AVL_ERR_NOT_FOUND         /* 未找到 */
}avl_err_e;


/* 节点结构 */
typedef struct _node_t
{
    struct _node_t *parent;     /* 父节点 */
    struct _node_t *lchild;     /* 左孩子 */
    struct _node_t *rchild;     /* 右孩子 */
    int32_t key;                    /* 节点值: 可根据实际情况设置数据类型 */
    int32_t bf;                     /* 平衡因子 */

    void *data;                 /* 附加数据 */
}avl_node_t;

/* 树结构 */
typedef struct
{
    avl_node_t *root;           /* 根节点 */
}avl_tree_t;

/* 设置node的左孩子节点 */
#define avl_set_lchild(node, lc) \
{ \
    (node)->lchild = (lc); \
    if(NULL != (lc)) \
    { \
        (lc)->parent = (node); \
    } \
}

/* 设置node的右孩子节点 */
#define avl_set_rchild(node, rc) \
{ \
    (node)->rchild = (rc); \
    if(NULL != (rc)) \
    { \
        (rc)->parent = (node); \
    } \
}

/* 替换父节点的孩子节点 */
#define avl_replace_child(tree, _parent, old, _new) \
{ \
    if(NULL == _parent) \
    { \
        (tree)->root = (_new); \
        if(NULL != (_new)) \
        { \
            (_new)->parent = NULL; \
        } \
    } \
    else if(_parent->lchild == old) \
    { \
        avl_set_lchild(_parent, _new); \
    } \
    else if(_parent->rchild == old) \
    { \
        avl_set_rchild(_parent, _new); \
    } \
}

extern int32_t avl_creat(avl_tree_t **tree);
extern int32_t avl_insert(avl_tree_t *tree, int32_t key, void *data);
extern const avl_node_t *avl_search(const avl_tree_t *tree, int32_t key);
extern int32_t avl_delete(avl_tree_t *tree, int32_t key, void **data);
extern int32_t avl_print(avl_tree_t *tree);
extern void avl_destroy(avl_tree_t **tree);

/* 测试使用 */
void avl_assert(const avl_node_t *node);

#endif /*__AVL_TREE_H__*/

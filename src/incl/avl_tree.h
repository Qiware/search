#if !defined(__AVL_TREE_H__)
#define __AVL_TREE_H__

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <memory.h>
#include <stdint.h>

#include "stack.h"
#include "common.h"

#define AVL_RH    (-1)          /* 右子树增高 */
#define AVL_EH    (0)           /* 高度未发生变化 */
#define AVL_LH    (1)           /* 左子树增高 */

#define AVL_MAX_DEPTH   (256)

typedef enum
{
    AVL_OK                      /* 成功 */

    , AVL_ERR = ~0x7FFFFFFF     /* 失败 */
    , AVL_NODE_EXIST            /* 节点存在 */
    , AVL_ERR_STACK             /* 栈异常 */
    , AVL_ERR_NOT_FOUND         /* 未找到 */
} avl_err_e;

typedef struct
{
    void *unique;
    int len;
} avl_unique_t;

typedef uint32_t (*avl_key_cb_t)(void *unique, size_t len);
typedef int (*avl_cmp_cb_t)(void *data1, void *data2);

/* 节点结构 */
typedef struct _node_t
{
    int key;                    /* 节点值: 可根据实际情况设置数据类型 */
                                /* Key might not unique */
    avl_unique_t unique;        /* Unique */

    int bf;                     /* 平衡因子 */
    void *data;                 /* 附加数据 */

    struct _node_t *parent;     /* 父节点 */
    struct _node_t *lchild;     /* 左孩子 */
    struct _node_t *rchild;     /* 右孩子 */
} avl_node_t;

/* 树结构 */
typedef struct
{
    avl_node_t *root;           /* 根节点 */

    avl_key_cb_t key;           /* Create key */
    avl_cmp_cb_t cmp;           /* Compare data */
} avl_tree_t;

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

extern int avl_creat(avl_tree_t **tree, avl_key_cb_t key, avl_cmp_cb_t cmp);
extern int avl_insert(avl_tree_t *tree, avl_unique_t *unique, void *data);
extern const avl_node_t *avl_search(const avl_tree_t *tree, avl_unique_t *unique);
extern int avl_delete(avl_tree_t *tree, avl_unique_t *unique, void **data);
extern int avl_print(avl_tree_t *tree);
extern void avl_destroy(avl_tree_t **tree);

/* 测试使用 */
void avl_assert(const avl_node_t *node);

#endif /*__AVL_TREE_H__*/

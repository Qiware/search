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

/* 唯一键 */
typedef struct
{
    void *data;
    size_t len;
} avl_ukey_t;

/******************************************************************************
 **函数名称: avl_key_cb_t
 **功    能: 为唯一键产生KEY值
 **输入参数: 
 **     ukey: 唯一键(任意数据类型, 但该值在二叉树中必须是唯一的)
 **     len: 唯一键长度
 **输出参数: NONE
 **返    回: KEY值
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.09 #
 ******************************************************************************/
typedef uint32_t (*avl_key_cb_t)(const void *ukey, size_t len);

/******************************************************************************
 **函数名称: avl_cmp_cb_t
 **功    能: 唯一键与值的比较函数
 **输入参数: 
 **     ukey: 唯一键
 **     data: 与唯一键进行比较的数值
 **输出参数: NONE
 **返    回: 
 **     1. 0:相等
 **     2. < 0: 小于(ukey < data)
 **     3. > 0: 大于(ukey > data)
 **实现描述: 
 **注意事项: 
 **     数值中必须存有与之相关联ukey的值
 **作    者: # Qifeng.zou # 2014.11.09 #
 ******************************************************************************/
typedef int (*avl_cmp_cb_t)(const void *ukey, const void *data);

/* 节点结构 */
typedef struct _node_t
{
    int key;                    /* 节点值: 该值可能不唯一 */

    int bf;                     /* 平衡因子 */

    struct _node_t *parent;     /* 父节点 */
    struct _node_t *lchild;     /* 左孩子 */
    struct _node_t *rchild;     /* 右孩子 */

    void *data;                 /* 附加数据 */
} avl_node_t;

/* 树结构 */
typedef struct
{
    avl_node_t *root;           /* 根节点 */

    avl_key_cb_t key_cb;        /* 生成KEY的回调 */
    avl_cmp_cb_t cmp_cb;        /* 数值比较回调 */
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

int avl_creat(avl_tree_t **tree, avl_key_cb_t key_cb, avl_cmp_cb_t cmp_cb);
int avl_insert(avl_tree_t *tree, void *uk, int uk_len, void *data);
avl_node_t *avl_search(avl_tree_t *tree, void *uk, int uk_len);
int avl_delete(avl_tree_t *tree, void *uk, int uk_len, void **data);
int avl_print(avl_tree_t *tree);
void avl_destroy(avl_tree_t **tree);

/* 测试使用 */
void avl_assert(const avl_node_t *node);

#endif /*__AVL_TREE_H__*/

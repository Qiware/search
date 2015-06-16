#if !defined(__AVL_TREE_H__)
#define __AVL_TREE_H__

#include "comm.h"
#include "slab.h"
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

/* 主键 */
typedef struct
{
    void *v;                    /* 主键值 */
    size_t len;                 /* 主键长度 */
} avl_key_t;

/******************************************************************************
 **函数名称: avl_cmp_cb_t
 **功    能: 主键与值的比较函数
 **输入参数: 
 **     key: 主键
 **     data: 与唯一键进行比较的数值
 **输出参数: NONE
 **返    回: 
 **     1. 0:相等
 **     2. < 0: 小于(key < data)
 **     3. > 0: 大于(key > data)
 **实现描述: 
 **注意事项: 
 **     数值中必须存有与之相关联key的值
 **作    者: # Qifeng.zou # 2014.11.09 #
 ******************************************************************************/
typedef int (*avl_cmp_cb_t)(const void *key, const void *data);

/* 节点结构 */
typedef struct _node_t
{
    int64_t idx;                /* 索引值: 该值有key_cb生成 可能不唯一 */

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

    key_cb_t key_cb;            /* 生成KEY的回调 */
    avl_cmp_cb_t cmp_cb;        /* 数值比较回调 */

    void *pool;                 /* 内存池 */
    mem_alloc_cb_t alloc;       /* 申请内存 */
    mem_dealloc_cb_t dealloc;   /* 释放内存 */
} avl_tree_t;

typedef int (*avl_trav_cb_t)(void *data, void *args);

avl_tree_t *avl_creat(avl_opt_t *opt, key_cb_t key_cb, avl_cmp_cb_t cmp_cb);
int avl_insert(avl_tree_t *tree, void *key, int key_len, void *data);
avl_node_t *avl_query(avl_tree_t *tree, void *key, int key_len);
int avl_delete(avl_tree_t *tree, void *key, int key_len, void **data);
int avl_print(avl_tree_t *tree);
int avl_trav(avl_tree_t *tree, avl_trav_cb_t proc, void *args);
void avl_destroy(avl_tree_t *tree, mem_dealloc_cb_t dealloc_cb, void *args);
#define avl_isempty(tree) (NULL == (tree)->root)

/* 通用回调 */
int avl_key_cb_int32(const int *key, size_t len);
int avl_cmp_cb_int32(const int *key, const void *data);

int64_t avl_key_cb_int64(const int64_t *key, size_t len);
int avl_cmp_cb_int64(const int64_t *key, const void *data);

/* 测试使用 */
void avl_assert(const avl_node_t *node);

#endif /*__AVL_TREE_H__*/

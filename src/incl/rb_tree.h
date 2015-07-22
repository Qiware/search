#if !defined(__RB_TREE_H__)
#define __RB_TREE_H__

#include "comm.h"
#include "stack.h"

#define RBT_COLOR_BLACK  'b'        /* 黑色 */
#define RBT_COLOR_RED    'r'        /* 红色 */

#define RBT_LCHILD       (0)        /* 左孩子 */
#define RBT_RCHILD       (1)        /* 右孩子 */

#define RBT_MAX_DEPTH    (512)

/* 遍历回调类型 */
typedef int (*rbt_trav_cb_t)(void *data, void *args);

/******************************************************************************
 **函数名称: rbt_cmp_cb_t
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
 **注意事项: 数值中必须存有与之相关联key的值
 **作    者: # Qifeng.zou # 2015.07.21 #
 ******************************************************************************/
typedef int (*rbt_cmp_cb_t)(const void *key, const void *data);

/* 错误码定义 */
typedef enum
{
    RBT_OK                          /* 成功 */

    , RBT_ERR = ~0x7fffffff         /* 失败 */
    , RBT_NODE_EXIST                /* 结点存在 */
} rbt_ret_e;

/* 主键KV */
typedef struct
{
    void *v;                        /* 主键值 */
    size_t len;                     /* 主键长度 */
} rbt_key_t;

/* 选项 */
typedef struct
{
    void *pool;                     /* 内存池 */
    mem_alloc_cb_t alloc;           /* 申请内存 */
    mem_dealloc_cb_t dealloc;       /* 释放内存 */
} rbt_opt_t;

/* 设置默认选项 */
#define rbt_setup_opt(opt)   \
{ \
    (opt)->pool = NULL; \
    (opt)->alloc = mem_alloc; \
    (opt)->dealloc = mem_dealloc; \
}

/* 结点结构 */
typedef struct _rbt_node_t
{
    int64_t idx;                    /* 索引值(非唯一值) */
    int32_t color;                  /* 结点颜色: RBT_COLOR_BLACK(黑) 或 RBT_COLOR_RED(红) */
    struct _rbt_node_t *parent;     /* 父节点 */
    struct _rbt_node_t *lchild;     /* 左孩子节点 */
    struct _rbt_node_t *rchild;     /* 右孩子节点 */

    void *data;                     /* 数据地址 */
} rbt_node_t;

/* 红黑树结构 */
typedef struct
{
    rbt_node_t *root;               /* 根节点 */
    rbt_node_t *sentinel;           /* 哨兵节点 */

    key_cb_t key_cb;                /* KEY值生成函数 */
    rbt_cmp_cb_t cmp_cb;            /* 主键比较函数 */

    struct
    {
        void *pool;                 /* 内存池 */
        mem_alloc_cb_t alloc;       /* 申请内存 */
        mem_dealloc_cb_t dealloc;   /* 释放内存 */
    };
} rbt_tree_t;

#define rbt_copy_color(node, src) ((node)->color = (src)->color);
#define rbt_set_color(node, c)  ((node)->color = (c))
#define rbt_set_red(node)   rbt_set_color(node, RBT_COLOR_RED)
#define rbt_set_black(node) rbt_set_color(node, RBT_COLOR_BLACK)
#define rbt_is_red(node)    (RBT_COLOR_RED == (node)->color)
#define rbt_is_black(node)   (RBT_COLOR_BLACK == (node)->color)

/* 设置左孩子 */
#define rbt_set_lchild(tree, node, left) \
{ \
    (node)->lchild = (left); \
    if(tree->sentinel != left) \
    { \
        (left)->parent = (node); \
    } \
} 

/* 设置右孩子 */
#define rbt_set_rchild(tree, node, right) \
{ \
    (node)->rchild = (right); \
    if(tree->sentinel != right) \
    { \
        (right)->parent = (node); \
    } \
} 

/* 设置孩子节点 */
#define rbt_set_child(tree, node, type, child) \
{ \
    if(RBT_LCHILD == type) \
    { \
        rbt_set_lchild(tree, node, child); \
    } \
    else \
    { \
        rbt_set_rchild(tree, node, child); \
    } \
} 

rbt_tree_t *rbt_creat(rbt_opt_t *opt, key_cb_t key_cb, rbt_cmp_cb_t cmp_cb);
int rbt_insert(rbt_tree_t *tree, void *key, int key_len, void *data);
int rbt_delete(rbt_tree_t *tree, void *key, int key_len, void **data);
rbt_node_t *rbt_search(rbt_tree_t *tree, void *key, int key_len);
int rbt_print(rbt_tree_t *tree);
int rbt_trav(rbt_tree_t *tree, rbt_trav_cb_t proc, void *args);
int rbt_destroy(rbt_tree_t *tree);

int rbt_key_cb_int32(const int *key, size_t len);
int rbt_cmp_cb_int32(const int *key, const void *data);

int64_t rbt_key_cb_int64(const int64_t *key, size_t len);
int rbt_cmp_cb_int64(const int64_t *key, const void *data);

#endif /*__RB_TREE_H__*/

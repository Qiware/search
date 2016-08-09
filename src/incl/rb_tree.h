#if !defined(__RB_TREE_H__)
#define __RB_TREE_H__

#include "comm.h"
#include "stack.h"

#define RBT_COLOR_BLACK  'b'        /* 黑色 */
#define RBT_COLOR_RED    'r'        /* 红色 */

#define RBT_LCHILD       (0)        /* 左孩子 */
#define RBT_RCHILD       (1)        /* 右孩子 */

#define RBT_MAX_DEPTH    (512)

/* 错误码定义 */
typedef enum
{
    RBT_OK                          /* 成功 */

    , RBT_ERR = ~0x7fffffff         /* 失败 */
    , RBT_NODE_EXIST                /* 结点存在 */
} rbt_ret_e;

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
    int32_t color;                  /* 结点颜色: RBT_COLOR_BLACK(黑) 或 RBT_COLOR_RED(红) */
    struct _rbt_node_t *parent;     /* 父结点 */
    struct _rbt_node_t *lchild;     /* 左孩子结点 */
    struct _rbt_node_t *rchild;     /* 右孩子结点 */

    void *data;                     /* 数据地址(注: 包含主键) */
} rbt_node_t;

/* 红黑树结构 */
typedef struct
{
    uint64_t num;                   /* 结点数 */
    rbt_node_t *root;               /* 根结点 */
    rbt_node_t *sentinel;           /* 哨兵结点 */

    cmp_cb_t cmp_cb;                /* 主键比较函数 */

    struct {
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
    if(tree->sentinel != left) { \
        (left)->parent = (node); \
    } \
} 

/* 设置右孩子 */
#define rbt_set_rchild(tree, node, right) \
{ \
    (node)->rchild = (right); \
    if(tree->sentinel != right) { \
        (right)->parent = (node); \
    } \
} 

/* 设置孩子结点 */
#define rbt_set_child(tree, node, type, child) \
{ \
    if(RBT_LCHILD == type) { \
        rbt_set_lchild(tree, node, child); \
    } \
    else { \
        rbt_set_rchild(tree, node, child); \
    } \
} 

rbt_tree_t *rbt_creat(rbt_opt_t *opt, cmp_cb_t cmp_cb);
int rbt_insert(rbt_tree_t *tree, void *data);
int rbt_delete(rbt_tree_t *tree, void *key, void **data);
void *rbt_query(rbt_tree_t *tree, void *key);
int rbt_print(rbt_tree_t *tree, print_cb_t print);
int rbt_trav(rbt_tree_t *tree, trav_cb_t proc, void *args);
void *rbt_find(rbt_tree_t *tree, find_cb_t find, void *args);
#define rbt_num(tree) ((tree)->num)
int rbt_destroy(rbt_tree_t *tree, mem_dealloc_cb_t dealloc, void *args);

#endif /*__RB_TREE_H__*/

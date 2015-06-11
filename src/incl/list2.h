#if !defined(__LIST2_H__)
#define __LIST2_H__

#include "comm.h"

/* 链表选项 */
typedef struct
{
    void *pool;                     /* 内存池 */
    mem_alloc_cb_t alloc;           /* 申请空间 */
    mem_dealloc_cb_t dealloc;       /* 释放空间 */
} list2_opt_t;

/* 链表结点 */
typedef struct _list2_node_t
{
    void *data;                     /* 数据指针 */
    struct _list2_node_t *prev;     /* 前节点 */
    struct _list2_node_t *next;     /* 后节点 */
} list2_node_t;

/* 双向链表 */
typedef struct
{
    int num;                        /* 成员个数 */
    list2_node_t *head;             /* 链表头 */

    struct
    {
        void *pool;                 /* 内存池 */
        mem_alloc_cb_t alloc;       /* 申请空间 */
        mem_dealloc_cb_t dealloc;   /* 释放空间 */
    };
} list2_t;

list2_t *list2_creat(list2_opt_t *opt);
int list2_lpush(list2_t *list, void *data);
int list2_rpush(list2_t *list, void *data);
void *list2_lpop(list2_t *list);
void *list2_rpop(list2_t *list);
void *list2_delete(list2_t *list, list2_node_t *node);

typedef int (*list2_trav_cb_t)(void *data, void *args);
int list2_trav(list2_t *list, list2_trav_cb_t cb, void *args);

#endif /*__LIST2_H__*/

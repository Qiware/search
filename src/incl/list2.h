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

    struct {
        void *pool;                 /* 内存池 */
        mem_alloc_cb_t alloc;       /* 申请空间 */
        mem_dealloc_cb_t dealloc;   /* 释放空间 */
    };
} list2_t;

#define list2_len(list) ((list)->num)
list2_t *list2_creat(list2_opt_t *opt);
int list2_lpush(list2_t *list, void *data);
int list2_rpush(list2_t *list, void *data);
void *list2_lpop(list2_t *list);
void *list2_rpop(list2_t *list);
void *list2_delete(list2_t *list, list2_node_t *node);
void *list2_roll(list2_t *list);
void list2_destroy(list2_t *list, mem_dealloc_cb_t dealloc, void *pool);

int list2_trav(list2_t *list, trav_cb_t cb, void *args);
void *list2_find(list2_t *list, find_cb_t cb, void *args);
void *list2_find_and_del(list2_t *list, find_cb_t cb, void *args);

#endif /*__LIST2_H__*/

#if !defined(__LIST_H__)
#define __LIST_H__

#include "comm.h"

/* 选项 */
typedef struct
{
    void *pool;                     /* 内存池 */
    mem_alloc_cb_t alloc;           /* 申请空间 */
    mem_dealloc_cb_t dealloc;       /* 释放空间 */
} list_opt_t;

/* 单向链表结点 */
typedef struct _list_node_t
{
    void *data;                     /* 数据块 */
    struct _list_node_t *next;      /* 下一结点 */
} list_node_t;

/* 单向链表对象 */
typedef struct
{
    int num;                        /* 结点数 */
    list_node_t *head;              /* 链表头 */
    list_node_t *tail;              /* 链表尾 */

    struct
    {
        void *pool;                 /* 内存池 */
        mem_alloc_cb_t alloc;       /* 申请空间 */
        mem_dealloc_cb_t dealloc;   /* 释放空间 */
    };
} list_t;

void list_assert(list_t *list);
#define list_isempty(list) (NULL == (list)->head)

list_t *list_creat(list_opt_t *opt);
void list_destroy(list_t *list, void *pool, mem_dealloc_cb_t dealloc);
int list_insert(list_t *list, list_node_t *prev, void *data);
int list_remove(list_t *list, void *data);

int list_lpush(list_t *list, void *data);
int list_rpush(list_t *list, void *data);

void *list_lpop(list_t *list);
void *list_rpop(list_t *list);

#endif /*__LIST_H__*/

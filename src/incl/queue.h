#if !defined(__QUEUE_H__)
#define __QUEUE_H__

#include <pthread.h>

#include "slab.h"

/* 队列配置 */
typedef struct
{
    size_t count;           /* 单元数 */
} queue_conf_t;

/* 循环队列结点 */
typedef struct _Qnode_t
{
    struct _Qnode_t *next;
    void *data;
} Qnode_t;

/* 循环队列 */
typedef struct
{
    int max;                /* 队列容量 */
    int num;                /* 队列成员个数 */

    Qnode_t *base;          /* 队列基址 */
    Qnode_t *head;          /* 队列头 */
    Qnode_t *tail;          /* 队列尾 */
} Queue_t;

int queue_init(Queue_t *q, int max);
int queue_push(Queue_t *q, void *addr);
void* queue_pop(Queue_t *q);
void queue_destroy(Queue_t *q);

/* 获取队列剩余空间 */
#define queue_space(q) ((q)->max - (q)->num)

/* 加锁队列 */
typedef struct
{
    pthread_rwlock_t lock;                  /* 队列锁(可用SPIN锁替换) */
    Queue_t queue;                          /* 队列 */

    pthread_rwlock_t slab_lock;             /* 内存池锁(可用SPIN锁替换) */
    slab_pool_t *slab;                      /* 内存池 */
} lqueue_t;

lqueue_t *lqueue_init(int max, size_t pool);
void *lqueue_mem_alloc(lqueue_t *lq, size_t size);
void lqueue_mem_dealloc(lqueue_t *lq, void *p);
int lqueue_push(lqueue_t *lq, void *addr);
void *lqueue_pop(lqueue_t *lq);
void *lqueue_trypop(lqueue_t *lq);
void lqueue_destroy(lqueue_t *lq);

#endif /*__QUEUE_H__*/

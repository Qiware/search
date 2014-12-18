#if !defined(__QUEUE_H__)
#define __QUEUE_H__

#include <pthread.h>

#include "spinlock.h"
#include "mem_chunk.h"

/* 队列配置 */
typedef struct
{
    size_t count;           /* 单元数 */
} queue_conf_t;

/* 循环队列结点 */
typedef struct __qnode_t
{
    struct __qnode_t *next;
    void *data;
} _qnode_t;

/* 循环队列 */
typedef struct
{
    int max;                                /* 队列容量 */
    int num;                                /* 队列成员个数 */

    _qnode_t *base;                         /* 队列基址 */
    _qnode_t *head;                         /* 队列头 */
    _qnode_t *tail;                         /* 队列尾 */

    spinlock_t lock;                        /* 队列锁 */
} _queue_t;

/* 获取队列剩余空间 */
#define queue_space(q) ((q)->max - (q)->num)

int _queue_init(_queue_t *q, int max);
int queue_push_lock(_queue_t *q, void *addr);
void *queue_pop_lock(_queue_t *q);
void _queue_destroy(_queue_t *q);

/* 加锁队列 */
typedef struct
{
    _queue_t queue;                         /* 队列 */
    mem_chunk_t *chunk;                     /* 内存池 */
} queue_t;

queue_t *queue_init(int max, size_t size);
#define queue_malloc(q) mem_chunk_alloc((q)->chunk)
#define queue_dealloc(q, p) mem_chunk_dealloc((q)->chunk, p)
#define queue_push(q, addr) queue_push_lock(&((q)->queue), addr)
#define queue_pop(q) queue_pop_lock(&((q)->queue))
void queue_destroy(queue_t *q);

#endif /*__QUEUE_H__*/

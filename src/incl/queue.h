#if !defined(__QUEUE_H__)
#define __QUEUE_H__

#include "chunk.h"
#include "spinlock.h"

/* 队列配置 */
typedef struct
{
    int max;                                /* 单元总数 */
    int size;                               /* 单元大小 */
} queue_conf_t;

/* 循环队列(无锁队列) */
typedef struct
{
    unsigned int max;                       /* 队列容量(注: 必须为2的次方) */
    unsigned int mask;                      /* 掩码值Mask = (max - 1) */
    volatile unsigned int num;              /* 队列成员个数 */

    void **data;                            /* 指针数组(对其构造循环队列) */

    /* 生产者 */
    struct
    {
        volatile unsigned int head;         /* 生产者: 头索引 */
        volatile unsigned int tail;         /* 生产者: 尾索引 */
    } prod;
    /* 消费者 */
    struct
    {
        volatile unsigned int head;         /* 消费者: 头索引 */
        volatile unsigned int tail;         /* 消费者: 尾索引 */
    } cons;
} ring_queue_t;

ring_queue_t *ring_queue_creat(int max);
int ring_queue_push(ring_queue_t *q, void *addr);
void *ring_queue_pop(ring_queue_t *q);
void ring_queue_destroy(ring_queue_t *q);

/* 加锁队列 */
typedef struct
{
    ring_queue_t *ring;                     /* 队列 */
    chunk_t *chunk;                         /* 内存池 */
} queue_t;

queue_t *queue_creat(int max, int size);
#define queue_malloc(q) chunk_alloc((q)->chunk)
#define queue_dealloc(q, p) chunk_dealloc((q)->chunk, p)
#define queue_push(q, addr) ring_queue_push((q)->ring, addr)
#define queue_pop(q) ring_queue_pop((q)->ring)
void queue_destroy(queue_t *q);

/* 获取队列剩余空间 */
#define queue_space(q) ((q)->ring->max - (q)->ring->num)
#define queue_used(q) ((q)->ring->num)
#define queue_size(q) ((q)->chunk->size)

#endif /*__QUEUE_H__*/

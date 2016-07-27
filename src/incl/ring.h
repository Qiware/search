#if !defined(__RING_H__)
#define __RING_H__

#include "log.h"

/* 环形无锁队列 */
typedef struct
{
    unsigned int max;                       /* 队列容量(注: 必须为2的次方) */
    unsigned int mask;                      /* 掩码值Mask = (max - 1) */
    volatile unsigned int num;              /* 队列成员个数 */

    void **data;                            /* 指针数组(对其构造循环队列) */

    /* 生产者 */
    struct {
        volatile unsigned int head;         /* 生产者: 头索引(注: 其值一直往上递增) */
        volatile unsigned int tail;         /* 生产者: 尾索引(注: 其值一直往上递增) */
    } prod;

    /* 消费者 */
    struct {
        volatile unsigned int head;         /* 消费者: 头索引(注: 其值一直往上递增) */
        volatile unsigned int tail;         /* 消费者: 尾索引(注: 其值一直往上递增) */
    } cons;
} ring_t;

ring_t *ring_creat(int max);
int ring_push(ring_t *rq, void *addr);
int ring_mpush(ring_t *rq, void **addr, unsigned int num);
void *ring_pop(ring_t *rq);
int ring_mpop(ring_t *rq, void **addr, unsigned int num);
void ring_print(ring_t *rq);
void ring_destroy(ring_t *rq);
#define ring_max(rq) ((rq)->max)
#define ring_used(rq) ((rq)->num)

#endif /*__RING_H__*/

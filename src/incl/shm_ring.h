#if !defined(__SHM_RING_H__)
#define __SHM_RING_H__

#include "log.h"

/* 环形队列 */
typedef struct
{
    unsigned int max;                       /* 队列容量(注: 必须为2的次方) */
    unsigned int mask;                      /* 掩码值Mask = (max - 1) */
    volatile unsigned int num;              /* 队列成员个数 */

    /* 生产者 */
    struct
    {
        volatile unsigned int head;         /* 生产者: 头索引(注: 其值一直往上递增) */
        volatile unsigned int tail;         /* 生产者: 尾索引(注: 其值一直往上递增) */
    } prod;
    /* 消费者 */
    struct
    {
        volatile unsigned int head;         /* 消费者: 头索引(注: 其值一直往上递增) */
        volatile unsigned int tail;         /* 消费者: 尾索引(注: 其值一直往上递增) */
    } cons;

    off_t off[0];                           /* 环形数组(对其构造环形队列: 其长度为max)) */
} shm_ring_t;

size_t shm_ring_total(int max);
shm_ring_t *shm_ring_init(void *addr, int max);
int shm_ring_push(shm_ring_t *rq, off_t off);
int shm_ring_mpush(shm_ring_t *rq, off_t *off, unsigned int num);
off_t shm_ring_pop(shm_ring_t *rq);
int shm_ring_mpop(shm_ring_t *rq, off_t *off, unsigned int num);
void shm_ring_print(shm_ring_t *rq);
#define shm_ring_isempty(ring) (0 == (ring)->num)
#define shm_ring_used(ring) ((ring)->num)

#endif /*__SHM_RING_H__*/

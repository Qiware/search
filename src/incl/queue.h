#if !defined(__QUEUE_H__)
#define __QUEUE_H__

#include "slot.h"
#include "ring.h"

/* 队列配置 */
typedef struct
{
    int max;                                /* 单元总数 */
    size_t size;                            /* 单元大小 */
} queue_conf_t;

/* 队列 */
typedef struct
{
    slot_t *slot;                           /* 内存池 */
    ring_t *ring;                           /* 队列 */
} queue_t;

queue_t *queue_creat(int max, int size);
#define queue_malloc(q, size) slot_alloc((q)->slot, size)
static inline void queue_dealloc(queue_t *q, void *p) { slot_dealloc((q)->slot, p); }
#define queue_push(q, addr) ring_push((q)->ring, addr)
#define queue_mpush(q, addr, num) ring_mpush((q)->ring, addr, num)
#define queue_pop(q) ring_pop((q)->ring)
#define queue_mpop(q, addr, num) ring_mpop((q)->ring, addr, num)
#define queue_print(q) ring_print((q)->ring)
void queue_destroy(queue_t *q);

/* 获取队列剩余空间 */
#define queue_space(q) (ring_max((q)->ring) - ring_used((q)->ring))
#define queue_used(q) ring_used((q)->ring)
#define queue_empty(q) !ring_used((q)->ring)
#define queue_max(q) ring_max((q)->ring)
#define queue_size(q) slot_size((q)->slot)

#endif /*__QUEUE_H__*/

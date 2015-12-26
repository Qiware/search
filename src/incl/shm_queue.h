#if !defined(__SHM_QUEUE_H__)
#define __SHM_QUEUE_H__

#include "shm_ring.h"
#include "shm_slot.h"

#define SHMQ_MPUSH_MAX_NUM  (512)   /* 一次最大压入条数 */
#define SHMQ_MPOP_MAX_NUM   (512)   /* 一次最大弹出条数 */

/* 队列对象 */
typedef struct
{
    shm_ring_t *ring;               /* 环形队列 */
    shm_slot_t *slot;               /* 内存池对象 */
} shm_queue_t;

shm_queue_t *shm_queue_creat(const char *path, int max, int size);
shm_queue_t *shm_queue_attach(const char *path);
static inline void *shm_queue_malloc(shm_queue_t *shmq, size_t size)
{
    return shm_slot_alloc((shmq)->slot, size);
}
static inline void shm_queue_dealloc(shm_queue_t *shmq, void *p)
{
    return shm_slot_dealloc(shmq->slot, p);
}
int shm_queue_push(shm_queue_t *shmq, void *p);
int shm_queue_mpush(shm_queue_t *shmq, void **p, int num);
void *shm_queue_pop(shm_queue_t *shmq);
int shm_queue_mpop(shm_queue_t *shmq, void **p, int _num);

#define shm_queue_print(shmq) shm_ring_print((shmq)->ring)
#define shm_queue_isempty(shmq) shm_ring_isempty((shmq)->ring)
#define shm_queue_size(shmq) shm_slot_get_size((shmq)->slot)
#define shm_queue_used(shmq) shm_ring_used((shmq)->ring)

#endif /*__SHM_QUEUE_H__*/

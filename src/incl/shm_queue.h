#if !defined(__SHM_QUEUE_H__)
#define __SHM_QUEUE_H__

#include "shm_ring.h"
#include "shm_slot.h"

/* 队列对象 */
typedef struct
{
    shm_ring_t *ring;
    shm_slot_t *slot;
} shm_queue_t;

shm_queue_t *shm_queue_creat(int key, int max, int size);
shm_queue_t *shm_queue_attach(int key);
#define shm_queue_malloc(shmq) shm_slot_alloc((shmq)->slot)
#define shm_queue_dealloc(shmq, p) shm_slot_dealloc((shmq)->slot, p)
int shm_queue_push(shm_queue_t *shmq, void *p);
void *shm_queue_pop(shm_queue_t *shmq);

#define shm_queue_print(shmq) shm_ring_print((shmq)->ring)

#endif /*__SHM_QUEUE_H__*/

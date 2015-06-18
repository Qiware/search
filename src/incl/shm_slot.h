#if !defined(__SHM_SLOT_H__)
#define __SHM_SLOT_H__

#include "comm.h"

/* SLOT头 */
typedef struct
{
    int max;                    /* 内存块数 */
    int size;                   /* 内存块大小 */
} shm_slot_t;

size_t shm_slot_total(int num, size_t size);
shm_slot_t *shm_slot_init(void *addr, int num, size_t size);
void *shm_slot_alloc(shm_slot_t *slot, int size);
void shm_slot_dealloc(shm_slot_t *slot, void *p);
void shm_slot_print(shm_slot_t *slot);

#define shm_slot_get_max(slot) ((slot)->max)
#define shm_slot_get_size(slot) ((slot)->size)

#endif /*__SHM_SLOT_H__*/

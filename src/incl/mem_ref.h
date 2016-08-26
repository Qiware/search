#if !defined(__MEM_REF_H__)
#define __MEM_REF_H__

#include "comm.h"

int mem_ref_init(void);

void *mem_ref_alloc(size_t size, void *pool, mem_alloc_cb_t alloc, mem_dealloc_cb_t dealloc);
void mem_ref_dealloc(void *pool, void *addr);

int mem_ref_check(void *addr);

int mem_ref_incr(void *addr);
int mem_ref_decr(void *addr);

#endif /*__MEM_REF_H__*/

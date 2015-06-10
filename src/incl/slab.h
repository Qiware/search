#if !defined(__SLAB_H__)
#define __SLAB_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdint.h>

#include "log.h"
#include "spinlock.h"

typedef struct _slab_page_t
{
    uintptr_t slab;
    struct _slab_page_t  *next;
    uintptr_t prev;
} slab_page_t;

typedef struct
{
    log_cycle_t *log;               /* 日志对象 */

    size_t min_size;
    size_t min_shift;

    slab_page_t *pages;
    slab_page_t free;

    u_char *start;
    u_char *end;

    spinlock_t lock;                  /* 内存锁 */
} slab_pool_t;

slab_pool_t *slab_init(void *addr, size_t size, log_cycle_t *log);
void *slab_alloc(slab_pool_t *pool, size_t size);
void slab_dealloc(slab_pool_t *pool, void *p);
#define slab_destroy(pool) { free(pool); pool = NULL; }

slab_pool_t *slab_creat_by_calloc(size_t size, log_cycle_t *log);
#endif /*__SLAB_H__*/

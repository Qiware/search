#if !defined(__MEM_POOL_H__)
#define __MEM_POOL_H__

#include <stdint.h>

#include "common.h"
#include "log.h"


/*
 * MEM_POOL_MAX_ALLOC_FROM_POOL should be (ngx_pagesize - 1), i.e. 4095 on x86.
 * On Windows NT it decreases a number of locked pages in a kernel.
 */
#define MEM_POOL_MAX_ALLOC_FROM_POOL  (4 * KB - 1)

#define NGX_DEFAULT_POOL_SIZE    (16 * KB)

#define MEM_POOL_ALIGNMENT       16
#define NGX_MIN_POOL_SIZE  \
    mem_align((sizeof(mem_pool_t) + 2 * sizeof(mem_pool_large_t)), MEM_POOL_ALIGNMENT)


typedef void (*mem_pool_cleanup_cb_t)(void *data);

typedef struct _mem_pool_cleanup_t
{
    mem_pool_cleanup_cb_t   handler;
    void                 *data;
    struct _mem_pool_cleanup_t   *next;
}mem_pool_cleanup_t;

/* 大块内存 */
typedef struct _mem_pool_large_t
{
    struct _mem_pool_large_t     *next;
    void                 *alloc;
}mem_pool_large_t;

typedef struct _mem_pool_t __mem_pool_t;
typedef struct
{
    u_char               *last;
    u_char               *end;
    __mem_pool_t         *next;
    uint64_t            failed;
} mem_pool_data_t;

/* 内存池对象 */
typedef struct _mem_pool_t
{
    mem_pool_data_t      d;
    size_t               max;
    struct _mem_pool_t   *current;
    mem_pool_large_t     *large;
    mem_pool_cleanup_t   *cleanup;
    log_cycle_t          *log;
}mem_pool_t;

typedef struct
{
    int              fd;
    u_char               *name;
    log_cycle_t            *log;
} mem_pool_cleanup_file_t;

mem_pool_t *mem_pool_creat(size_t size, log_cycle_t *log);
void mem_pool_destroy(mem_pool_t *pool);
void mem_pool_reset(mem_pool_t *pool);

void *mem_pool_alloc(mem_pool_t *pool, size_t size);
void *mem_pool_nalloc(mem_pool_t *pool, size_t size);
void *mem_pool_calloc(mem_pool_t *pool, size_t size);
void *mem_pool_mem_align(mem_pool_t *pool, size_t size, size_t alignment);
int mem_pool_free(mem_pool_t *pool, void *p);

mem_pool_cleanup_t *mem_pool_cleanup_add(mem_pool_t *p, size_t size);
void mem_pool_run_cleanup_file(mem_pool_t *p, int fd);
void mem_pool_cleanup_file(void *data);
void mem_pool_delete_file(void *data);
#endif /* _NGX_PALLOC_H_INCLUDED_ */

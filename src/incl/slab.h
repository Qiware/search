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
    size_t min_size;
    size_t min_shift;

    slab_page_t *pages;
    slab_page_t free;

    u_char *start;
    u_char *end;

    void *data;
    void *addr;

    spinlock_t lock;                        /* 内存锁 */
} slab_pool_t;

/* 可扩展SLAB节点 */
typedef struct _eslab_node_t
{
    slab_pool_t *pool;
    struct _eslab_node_t *next;
} eslab_node_t;

/* 可扩展SLAB */
typedef struct
{
    int count;                  /* SLAB结点总数 */
    size_t incr;                /* 扩展SLAB递增空间 */

    eslab_node_t *node;         /* SLAB内存池链表 */
    eslab_node_t *curr;         /* 最近一次正在使用eSLAB结点 */
} eslab_pool_t;

slab_pool_t *slab_init(void *addr, size_t size);
void *slab_alloc(slab_pool_t *pool, size_t size);
void slab_dealloc(slab_pool_t *pool, void *p);
#define slab_destroy(pool) { free(pool); pool = NULL; }

int eslab_init(eslab_pool_t *spl, size_t size);
int eslab_destroy(eslab_pool_t *spl);
void *eslab_alloc(eslab_pool_t *spl, size_t size);
void eslab_dealloc(eslab_pool_t *spl, void *p);
#endif /*__SLAB_H__*/

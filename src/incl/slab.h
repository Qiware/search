#if !defined(__SLAB_H__)
#define __SLAB_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdint.h>

#include "log.h"

typedef struct _slab_page_t
{
    uintptr_t slab;
    struct _slab_page_t  *next;
    uintptr_t prev;
}slab_page_t;

typedef struct
{
    size_t min_size;
    size_t min_shift;

    slab_page_t  *pages;
    slab_page_t   free;

    u_char *start;
    u_char *end;

    void *data;
    void *addr;
} slab_pool_t;

void slab_init(slab_pool_t *pool);
void *slab_alloc(slab_pool_t *pool, size_t size);
void slab_free(slab_pool_t *pool, void *p);

/* 可扩展SLAB节点 */
typedef struct _eslab_node_t
{
    slab_pool_t *sp;
    struct _eslab_node_t *next;
}eslab_node_t;

/* 可扩展SLAB */
typedef struct
{
    int count;
    size_t inc_size;
    eslab_node_t *node;
}eslab_pool_t;

int eslab_init(eslab_pool_t *spl, size_t size);
int eslab_destroy(eslab_pool_t *spl);
void *eslab_alloc(eslab_pool_t *spl, size_t size);
int eslab_free(eslab_pool_t *spl, void *p);
#endif /*__SLAB_H__*/

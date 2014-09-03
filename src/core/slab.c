/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: slab.c
 ** 版本号: 1.0
 ** 描  述: SLAB算法机制
 **         该算法主要用于内存的分配、管理和回收的处理。
 ** 作  者: # Nginx # 2013.07.12 # 来自Nginx #
 ******************************************************************************/
#include "slab.h"
#include "log.h"

#define SLAB_PAGE_MASK   3
#define SLAB_PAGE        0
#define SLAB_BIG         1
#define SLAB_EXACT       2
#define SLAB_SMALL       3

#define SLAB_PAGE_SIZE  4096
#define SLAB_PAGE_SHIFT 12
#define SLAB_EXTRA_SIZE (SLAB_PAGE_SIZE)

#if defined(__x86_64__)

#define SLAB_PAGE_FREE   0
#define SLAB_PAGE_BUSY   0xffffffffffffffff
#define SLAB_PAGE_START  0x8000000000000000

#define SLAB_SHIFT_MASK  0x000000000000000f
#define SLAB_MAP_MASK    0xffffffff00000000
#define SLAB_MAP_SHIFT   32

#define SLAB_BUSY        0xffffffffffffffff

#else   /*!__x86_64__*/

#define SLAB_PAGE_FREE   0
#define SLAB_PAGE_BUSY   0xffffffff
#define SLAB_PAGE_START  0x80000000

#define SLAB_SHIFT_MASK  0x0000000f
#define SLAB_MAP_MASK    0xffff0000
#define SLAB_MAP_SHIFT   16

#define SLAB_BUSY        0xffffffff
#endif  /*!__x86_64__*/

#define slab_junk(p, size) memset(p, 0, size)

static slab_page_t *slab_alloc_pages(slab_pool_t *pool, uint32_t pages);
static void slab_free_pages(slab_pool_t *pool, slab_page_t *page, uint32_t pages);


static size_t slab_max_size = 0;
static size_t slab_exact_size = 0;
static uint32_t slab_exact_shift = 0;
static size_t slab_page_size = SLAB_PAGE_SIZE;
static uint32_t slab_page_shift = SLAB_PAGE_SHIFT;

#define slab_get_max_size() (slab_max_size)
#define slab_set_max_size(size)  (slab_max_size = (size))
#define slab_get_exact_size() (slab_exact_size)
#define slab_set_exact_size(size) (slab_exact_size = (size))
#define slab_get_exact_shift() (slab_exact_shift)
#define slab_set_exact_shift(shift) (slab_exact_shift = (shift))
#define slab_get_page_size() (slab_page_size)
#define slab_set_page_size(size) (slab_page_size = (size))
#define slab_get_page_shift() (slab_page_shift)
#define slab_set_page_shift(shift) (slab_page_shift = (shift))

#define slab_align_ptr(p, a) \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

/******************************************************************************
 **函数名称: slab_init
 **功    能: Slab初始化
 **输入参数:
 **     pool: Slab对象
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
void slab_init(slab_pool_t *pool, log_cycle_t *log)
{
    u_char *p = NULL;
    size_t size = 0;
    int m = 0;
    uint32_t i = 0, n = 0, pages = 0, shift = 0;
    slab_page_t *slots = NULL;

    pool->log = log;

    /* STUB */
    if (0 == slab_get_max_size())
    {
        slab_set_max_size(slab_get_page_size() / 2);
        slab_set_exact_size(slab_get_page_size() / (8 * sizeof(uintptr_t)));
        for (n = slab_get_exact_size(); n >>= 1; shift++)
        {
            /* void */
        }
        slab_set_exact_shift(shift);
    }
    /**/

    pool->min_shift = 3;
    pool->min_size = 1 << pool->min_shift;

    p = (u_char *) pool + sizeof(slab_pool_t);
    size = pool->end - p;

    slab_junk(p, size);

    slots = (slab_page_t *) p;
    n = slab_get_page_shift() - pool->min_shift;

    for (i = 0; i < n; i++)
    {
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(slab_page_t);

    pages = (unsigned int) (size / (slab_get_page_size() + sizeof(slab_page_t)));

    memset(p, 0, pages * sizeof(slab_page_t));

    pool->pages = (slab_page_t *) p;

    pool->free.prev = 0;
    pool->free.next = (slab_page_t *) p;

    pool->pages->slab = pages;
    pool->pages->next = &pool->free;
    pool->pages->prev = (uintptr_t) &pool->free;

    pool->start = (u_char *)slab_align_ptr(
                    (uintptr_t) p + pages * sizeof(slab_page_t), slab_get_page_size());

    m = pages - (pool->end - pool->start) / slab_get_page_size();
    if (m > 0)
    {
        pages -= m;
        pool->pages->slab = pages;
    }

    log_info(pool->log, "start:%p end:%p size:%d pages:%d\n",
        pool->start, pool->end, pool->end - pool->start, pages);
}

/******************************************************************************
 **函数名称: slab_alloc
 **功    能: 从Slab中申请内存空间
 **输入参数:
 **     pool: Slab对象
 **     size: 申请的空间大小
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
void *slab_alloc(slab_pool_t *pool, size_t size)
{
    size_t s = 0;
    uintptr_t p = 0, n = 0,
        m = 0, mask = 0, *bitmap = NULL;
    uint32_t i = 0, slot = 0, shift = 0, map = 0;
    slab_page_t *page = NULL, *prev = NULL, *slots = NULL;

    if (size >= slab_get_max_size())
    {
        page = slab_alloc_pages(pool,
                (size >> slab_get_page_shift())
                    + ((size % slab_get_page_size()) ? 1 : 0));
        if (page)
        {
            p = (page - pool->pages) << slab_get_page_shift();
            p += (uintptr_t) pool->start;
        }
        else
        {
            p = 0;
        }

        goto done;
    }

    if (size > pool->min_size)
    {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++)
        {
            /* void */
        }
        slot = shift - pool->min_shift;
    }
    else
    {
        size = pool->min_size;
        shift = pool->min_shift;
        slot = 0;
    }

    slots = (slab_page_t *) ((u_char *) pool + sizeof(slab_pool_t));
    page = slots[slot].next;

    if (page->next != page)
    {
        if (shift < slab_get_exact_shift())
        {
            do
            {
                p = (page - pool->pages) << slab_get_page_shift();
                bitmap = (uintptr_t *) (pool->start + p);

                map = (1 << (slab_get_page_shift() - shift)) / (sizeof(uintptr_t) * 8);

                for (n = 0; n < map; n++)
                {
                    if (SLAB_BUSY != bitmap[n])
                    {
                        for (m = 1, i = 0; m; m <<= 1, i++)
                        {
                            if ((bitmap[n] & m))
                            {
                                continue;
                            }

                            bitmap[n] |= m;

                            i = ((n * sizeof(uintptr_t) * 8) << shift) + (i << shift);

                            if (SLAB_BUSY == bitmap[n])
                            {
                                for (n = n + 1; n < map; n++)
                                {
                                    if (bitmap[n] != SLAB_BUSY)
                                    {
                                        p = (uintptr_t) bitmap + i;
                                        goto done;
                                    }
                                }

                                prev = (slab_page_t *)(page->prev & ~SLAB_PAGE_MASK);
                                prev->next = page->next;
                                page->next->prev = page->prev;

                                page->next = NULL;
                                page->prev = SLAB_SMALL;
                            }

                            p = (uintptr_t) bitmap + i;

                            goto done;
                        }
                    }
                }

                page = page->next;

            } while (page);

        }
        else if (shift == slab_get_exact_shift())
        {
            do
            {
                if (SLAB_BUSY != page->slab)
                {
                    for (m = 1, i = 0; m; m <<= 1, i++)
                    {
                        if ((page->slab & m))
                        {
                            continue;
                        }

                        page->slab |= m;

                        if (SLAB_BUSY == page->slab)
                        {
                            prev = (slab_page_t *)
                                            (page->prev & ~SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = SLAB_EXACT;
                        }

                        p = (page - pool->pages) << slab_get_page_shift();
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);

        }
        else     /* shift > slab_exact_shift */
        {
            n = slab_get_page_shift() - (page->slab & SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t) 1 << n) - 1;
            mask = n << SLAB_MAP_SHIFT;

            do
            {
                if ((page->slab & SLAB_MAP_MASK) != mask)
                {
                    for (m = (uintptr_t) 1 << SLAB_MAP_SHIFT, i = 0;
                         m & mask;
                         m <<= 1, i++)
                    {
                        if ((page->slab & m))
                        {
                            continue;
                        }

                        page->slab |= m;

                        if ((page->slab & SLAB_MAP_MASK) == mask)
                        {
                            prev = (slab_page_t *)
                                            (page->prev & ~SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = SLAB_BIG;
                        }

                        p = (page - pool->pages) << slab_get_page_shift();
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);
        }
    }

    page = slab_alloc_pages(pool, 1);

    if (page)
    {
        if (shift < slab_get_exact_shift())
        {
            p = (page - pool->pages) << slab_get_page_shift();
            bitmap = (uintptr_t *) (pool->start + p);

            s = 1 << shift;
            n = (1 << (slab_get_page_shift() - shift)) / 8 / s;

            if (n == 0)
            {
                n = 1;
            }

            bitmap[0] = (2 << n) - 1;

            map = (1 << (slab_get_page_shift() - shift)) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++)
            {
                bitmap[i] = 0;
            }

            page->slab = shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | SLAB_SMALL;

            slots[slot].next = page;

            p = ((page - pool->pages) << slab_get_page_shift()) + s * n;
            p += (uintptr_t) pool->start;

            goto done;

        }
        else if (shift == slab_get_exact_shift())
        {
            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | SLAB_EXACT;

            slots[slot].next = page;

            p = (page - pool->pages) << slab_get_page_shift();
            p += (uintptr_t) pool->start;

            goto done;

        }
        else     /* shift > slab_exact_shift */
        {
            page->slab = ((uintptr_t) 1 << SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | SLAB_BIG;

            slots[slot].next = page;

            p = (page - pool->pages) << slab_get_page_shift();
            p += (uintptr_t) pool->start;

            goto done;
        }
    }

    p = 0;

done:

    if (0 != p)
    {
        memset((void *)p, 0, size);
    }

    return (void *) p;
}

/******************************************************************************
 **函数名称: slab_free
 **功    能: 释放从Slab申请的内存空间
 **输入参数:
 **     pool: Slab对象
 **     p: 内存起始地址
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
void slab_free(slab_pool_t *pool, void *p)
{
    size_t size = 0;
    uintptr_t slab = 0, m = 0, *bitmap = NULL;
    uint32_t n = 0, type = 0, slot = 0, shift = 0, map = 0;
    slab_page_t *slots = NULL, *page = NULL;

    if ((u_char *) p < pool->start || (u_char *) p > pool->end)
    {
        log_error(pool->log, "Outside of pool. [%p]", p);
        goto fail;
    }

    n = ((u_char *) p - pool->start) >> slab_get_page_shift();
    page = &pool->pages[n];
    slab = page->slab;
    type = page->prev & SLAB_PAGE_MASK;

    switch (type)
    {
        case SLAB_SMALL:
        {
            shift = slab & SLAB_SHIFT_MASK;
            size = 1 << shift;

            if ((uintptr_t) p & (size - 1)) {
                goto wrong_chunk;
            }

            n = ((uintptr_t) p & (slab_get_page_size() - 1)) >> shift;
            m = (uintptr_t) 1 << (n & (sizeof(uintptr_t) * 8 - 1));
            n /= (sizeof(uintptr_t) * 8);
            bitmap = (uintptr_t *) ((uintptr_t) p & ~(slab_get_page_size() - 1));

            if (bitmap[n] & m)
            {
                if (NULL == page->next)
                {
                    slots = (slab_page_t *)
                                       ((u_char *) pool + sizeof(slab_pool_t));
                    slot = shift - pool->min_shift;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | SLAB_SMALL;
                    page->next->prev = (uintptr_t) page | SLAB_SMALL;
                }

                bitmap[n] &= ~m;

                n = (1 << (slab_get_page_shift() - shift)) / 8 / (1 << shift);

                if (n == 0)
                {
                    n = 1;
                }

                if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1))
                {
                    goto done;
                }

                map = (1 << (slab_get_page_shift() - shift)) / (sizeof(uintptr_t) * 8);

                for (n = 1; n < map; n++)
                {
                    if (bitmap[n])
                    {
                        goto done;
                    }
                }

                slab_free_pages(pool, page, 1);

                goto done;
            }

            goto chunk_already_free;

        }
        case SLAB_EXACT:
        {
            m = (uintptr_t) 1 <<
                    (((uintptr_t) p & (slab_get_page_size() - 1)) >> slab_get_exact_shift());
            size = slab_get_exact_size();

            if ((uintptr_t) p & (size - 1))
            {
                goto wrong_chunk;
            }

            if (slab & m)
            {
                if (SLAB_BUSY == slab)
                {
                    slots = (slab_page_t *)
                                       ((u_char *) pool + sizeof(slab_pool_t));
                    slot = slab_get_exact_shift() - pool->min_shift;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | SLAB_EXACT;
                    page->next->prev = (uintptr_t) page | SLAB_EXACT;
                }

                page->slab &= ~m;

                if (page->slab)
                {
                    goto done;
                }

                slab_free_pages(pool, page, 1);

                goto done;
            }

            goto chunk_already_free;

        }
        case SLAB_BIG:
        {
            shift = slab & SLAB_SHIFT_MASK;
            size = 1 << shift;

            if ((uintptr_t) p & (size - 1))
            {
                goto wrong_chunk;
            }

            m = (uintptr_t) 1 << ((((uintptr_t) p & (slab_get_page_size() - 1)) >> shift)
                                  + SLAB_MAP_SHIFT);

            if (slab & m)
            {
                if (NULL == page->next)
                {
                    slots = (slab_page_t *)
                                       ((u_char *) pool + sizeof(slab_pool_t));
                    slot = shift - pool->min_shift;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | SLAB_BIG;
                    page->next->prev = (uintptr_t) page | SLAB_BIG;
                }

                page->slab &= ~m;

                if (page->slab & SLAB_MAP_MASK)
                {
                    goto done;
                }

                slab_free_pages(pool, page, 1);

                goto done;
            }

            goto chunk_already_free;

        }
        case SLAB_PAGE:
        {
            if ((uintptr_t) p & (slab_get_page_size() - 1))
            {
                goto wrong_chunk;
            }

            if (SLAB_PAGE_FREE == slab)
            {
                log_error(pool->log, "Page is already free");
                goto fail;
            }

            if (SLAB_PAGE_BUSY == slab)
            {
                log_error(pool->log, "Pointer to wrong page");
                goto fail;
            }

            n = ((u_char *) p - pool->start) >> slab_get_page_shift();
            size = slab & ~SLAB_PAGE_START;

            slab_free_pages(pool, &pool->pages[n], size);

            slab_junk(p, size << slab_get_page_shift());

            return;
        }
    }

    /* not reached */

    return;

done:

    slab_junk(p, size);

    return;

wrong_chunk:

    log_error(pool->log, "Pointer to wrong chunk");

    goto fail;

chunk_already_free:

    log_error(pool->log, "Chunk is already free");

fail:

    return;
}

/******************************************************************************
 **函数名称: slab_alloc_pages
 **功    能: 从Slab中申请一页内存
 **输入参数:
 **     pool: Slab对象
 **     pages: 申请的页数
 **输出参数:
 **返    回: 页对象地址
 **实现描述: 
 **注意事项: 
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
static slab_page_t *slab_alloc_pages(slab_pool_t *pool, uint32_t pages)
{
    slab_page_t *page = NULL, *p = NULL;


    for (page = pool->free.next; page != &pool->free; page = page->next)
    {
        if (page->slab >= pages)
        {
            if (page->slab > pages)
            {
                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            }
            else
            {
                p = (slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | SLAB_PAGE_START;
            page->next = NULL;
            page->prev = SLAB_PAGE;

            if (0 == --pages)
            {
                return page;
            }

            for (p = page + 1; pages; pages--)
            {
                p->slab = SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    log_error(pool->log, "Not enough memory!");

    return NULL;
}

/******************************************************************************
 **函数名称: slab_fee_pages
 **功    能: 释放从Slab中申请的页
 **输入参数:
 **     pool: Slab对象
 **     page: 页对象
 **     pages: 页数
 **输出参数:
 **返    回: 页地址
 **实现描述: 
 **注意事项: 
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
static void slab_free_pages(slab_pool_t *pool, slab_page_t *page, uint32_t pages)
{
    slab_page_t *prev = NULL;

    page->slab = pages--;

    if (pages)
    {
        memset(&page[1], 0, pages * sizeof(slab_page_t));
    }

    if (page->next)
    {
        prev = (slab_page_t *) (page->prev & ~SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}

/*******************************************************************************
 ** Copyright (C) 2013-2014 Xundao technology Cot,. Ltd
 **
 ** 模块: 扩展SLAB模块
 ** 作用: 使SLAB机制管理的空间可以动态增加
 ** 说明: 
 **      通过链表的方式将多个SLAB管理空间串联起来，从而实现SLAB空间的扩展.
 ** 注意: 
 ** 作者: # Qifeng.zou # 2013.08.15 #
 ******************************************************************************/
static slab_pool_t *eslab_add(eslab_pool_t *eslab, size_t size);

/******************************************************************************
 ** Name : eslab_init
 ** Func : Initialize expand slab pool.
 ** Input: 
 **     eslab: Expand slab pool
 **     size: Size(bytes) of new slab pool.
 **Output: NONE
 **Return: 0:success !0:failed
 ** Proc :
 **     1. Destory expand slab pool.
 **     2. Add node into link.
 ** Note : 
 **Author: # Qifeng.zou # 2013.08.15 #
 ******************************************************************************/
int eslab_init(eslab_pool_t *eslab, size_t size, log_cycle_t *log)
{
    slab_pool_t *slab = NULL;
    
    eslab_destroy(eslab);

    eslab->inc_size = size;
    eslab->log = log;
    
    slab = eslab_add(eslab, size);
    if (NULL == slab)
    {
        log_error(eslab->log, "Add slab node failed!");
        return -1;
    }
    return 0;
}

/******************************************************************************
 ** Name : eslab_destroy
 ** Func : Destory expand slab pool.
 ** Input: 
 **     eslab: Expand slab pool
 **Output: NONE
 **Return: 0:success !0:failed
 ** Note : 
 **Author: # Qifeng.zou # 2013.08.15 #
 ******************************************************************************/
int eslab_destroy(eslab_pool_t *eslab)
{
    eslab_node_t *node = NULL, *next = NULL;

    node = eslab->node;
    while (NULL != node)
    {
        next = node->next;
        if (node->sp)
        {
            free(node->sp);
        }
        free(node);
        node = next;
    }
    return 0;
}

/******************************************************************************
 ** Name : eslab_add
 ** Func : Add into expand slab pool.
 ** Input: 
 **     eslab: Expand slab pool
 **     size: Size of new slab pool.
 **Output: NONE
 **Return: Address of new slab.
 ** Note : 
 **Author: # Qifeng.zou # 2013.08.15 #
 ******************************************************************************/
static slab_pool_t *eslab_add(eslab_pool_t *eslab, size_t size)
{
    void *addr = NULL;
    eslab_node_t *node = NULL, *next = NULL;

    /* 1. Alloc memory for node. */
    node = calloc(1, sizeof(eslab_node_t));
    if (NULL == node)
    {
        log_error(eslab->log, "Alloc memory failed!");
        return NULL;
    }

    /* 2. Alloc memory for slab pool */
    addr = calloc(1, size);
    if (NULL == addr)
    {
        free(node), node=NULL;
        log_error(eslab->log, "Alloc memory failed!");
        return NULL;
    }

    node->sp = (slab_pool_t *)addr;
    node->sp->end = addr + size;
    slab_init(node->sp, eslab->log);

    /* 3. Add node into link */
    next = eslab->node;
    eslab->node = node;
    node->next = next;

    eslab->count++;

    return (slab_pool_t *)addr;
}

/******************************************************************************
 ** Name : eslab_alloc
 ** Func : Alloc memory from slab pool.
 ** Input: 
 **     eslab: Expand slab pool
 **     size: Size of new slab pool.
 **Output: NONE
 **Return: Memory address
 ** Note : 
 **Author: # Qifeng.zou # 2013.08.15 #
 ******************************************************************************/
void *eslab_alloc(eslab_pool_t *eslab, size_t size)
{
    void *p = NULL;
    int block = 0, count = 0;
    size_t alloc_size = 0;
    slab_pool_t *slab = NULL;
    eslab_node_t *node = eslab->node, *next;

    /* 1. Search and alloc */
    while (NULL != node)
    {
        next = node->next;
        
        p = slab_alloc(node->sp, size);
        if (NULL != p)
        {
            return p;
        }

        node = next;
        count++;
    }

    log_info(eslab->log, "Not enough memory! [%d]", count);

    /* 2. Add new slab node into link */
    block = size / eslab->inc_size;
    if (size % eslab->inc_size)
    {
        block++;
    }

    alloc_size = (block * eslab->inc_size + SLAB_EXTRA_SIZE);
    
    slab = eslab_add(eslab, alloc_size);
    if (NULL == slab)
    {
        log_error(eslab->log, "Add slab node failed!");
        return NULL;
    }

    slab->log = eslab->log;

    /* 3. Alloc memory from new slab pool */
    return slab_alloc(slab, size);
}

/******************************************************************************
 ** Name : eslab_free
 ** Func : Free special memory from expand slab pool.
 ** Input: 
 **     eslab: Expadnd slab pool
 **     p: Needed to be free.
 **Output: NONE
 **Return: 0:success !0:failed
 ** Note : 
 **Author: # Qifeng.zou # 2013.08.15 #
 ******************************************************************************/
int eslab_free(eslab_pool_t *eslab, void *p)
{
    eslab_node_t *node = eslab->node, *next = NULL;

    while (NULL != node)
    {
        next = node->next;
        
        if ((p >= (void *)node->sp->start) && (p < (void *)node->sp->end))
        {
            slab_free(node->sp, p);
            return 0;
        }
        node = next;
    }
    return -1;
}

/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: slab.c
 ** 版本号: 1.0
 ** 描  述: SLAB算法机制
 **         该算法主要用于内存的分配、管理和回收的处理。
 ** 注  意：此内存机制只适合"小内存块"的空间分配, 小内存指的是小于4K的内存块
 **         如果反复进行大量大小内存分配的混合空间申请和释放, "可能"出现分配空间失败的情况.
 ** 作  者: # Nginx # 2013.07.12 # 来自Nginx #
 ******************************************************************************/
#include "log.h"
#include "slab.h"

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

#if !defined(__MEM_LEAK_CHECK__)
static slab_page_t *slab_alloc_pages(slab_pool_t *pool, uint32_t pages);
static void slab_dealloc_pages(slab_pool_t *pool, slab_page_t *page, uint32_t pages);


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
 **     addr: 起始地址
 **     size: 内存SIZE
 **输出参数:
 **返    回: Slab对象
 **实现描述:
 **注意事项:
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
slab_pool_t *slab_init(void *addr, size_t size, log_cycle_t *log)
{
    int m;
    u_char *p;
    size_t left_size;
    slab_pool_t *pool;
    slab_page_t *slots;
    uint32_t i, n, pages, shift = 0;

    if (size < (sizeof(slab_pool_t) + slab_get_page_size())) {
        return NULL;
    }

    pool = (slab_pool_t *)addr;
    pool->log = log;
    pool->end = addr + size;

    /* STUB */
    if (0 == slab_get_max_size()) {
        slab_set_max_size(slab_get_page_size() / 2);
        slab_set_exact_size(slab_get_page_size() / (8 * sizeof(uintptr_t)));
        for (n = slab_get_exact_size(); n >>= 1; shift++) {
            /* void */
        }
        slab_set_exact_shift(shift);
    }
    /**/

    pool->min_shift = 3;
    pool->min_size = 1 << pool->min_shift;

    p = (u_char *) pool + sizeof(slab_pool_t);
    left_size = pool->end - p;

    slab_junk(p, left_size);

    slots = (slab_page_t *) p;
    n = slab_get_page_shift() - pool->min_shift;

    for (i = 0; i < n; i++) {
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(slab_page_t);

    pages = (unsigned int) (left_size / (slab_get_page_size() + sizeof(slab_page_t)));

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
    if (m > 0) {
        pages -= m;
        pool->pages->slab = pages;
    }

    log_info(pool->log, "start:%p end:%p left_size:%d pages:%d\n",
        pool->start, pool->end, pool->end - pool->start, pages);

    spin_lock_init(&pool->lock);

    return pool;
}

/******************************************************************************
 **函数名称: slab_alloc
 **功    能: 从Slab中申请内存空间
 **输入参数:
 **     pool: Slab对象
 **     size: 申请的空间大小
 **输出参数:
 **返    回: 内存地址
 **实现描述:
 **注意事项: 此内存机制只适合"小内存块"的空间分配, 小内存指的是小于4K的内存块.
 **          如果反复进行大量大小内存分配的混合空间申请和释放, "可能"出现分配空间
 **          失败的情况.
 **作    者: # Nginx # YYYY.MM.DD #
 ******************************************************************************/
void *slab_alloc(slab_pool_t *pool, size_t size)
{
    size_t s;
    uintptr_t p, n, m, mask, *bitmap;
    uint32_t i, slot, shift, map;
    slab_page_t *page, *prev, *slots;

    spin_lock(&pool->lock);    /* 加锁 */

    if (size >= slab_get_max_size()) {
        page = slab_alloc_pages(pool,
                (size >> slab_get_page_shift()) + ((size % slab_get_page_size()) ? 1 : 0));
        if (page) {
            p = (page - pool->pages) << slab_get_page_shift();
            p += (uintptr_t) pool->start;
        }
        else {
            p = 0;
        }

        goto done;
    }

    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) {
            /* void */
        }
        slot = shift - pool->min_shift;
    }
    else {
        size = pool->min_size;
        shift = pool->min_shift;
        slot = 0;
    }

    slots = (slab_page_t *) ((u_char *) pool + sizeof(slab_pool_t));
    page = slots[slot].next;

    if (page->next != page) {
        if (shift < slab_get_exact_shift()) {
            do {
                p = (page - pool->pages) << slab_get_page_shift();
                bitmap = (uintptr_t *) (pool->start + p);

                map = (1 << (slab_get_page_shift() - shift)) / (sizeof(uintptr_t) * 8);

                for (n = 0; n < map; n++) {
                    if (SLAB_BUSY != bitmap[n]) {
                        for (m = 1, i = 0; m; m <<= 1, i++) {
                            if ((bitmap[n] & m)) {
                                continue;
                            }

                            bitmap[n] |= m;

                            i = ((n * sizeof(uintptr_t) * 8) << shift) + (i << shift);

                            if (SLAB_BUSY == bitmap[n]) {
                                for (n = n + 1; n < map; n++) {
                                    if (bitmap[n] != SLAB_BUSY) {
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
        else if (shift == slab_get_exact_shift()) {
            do {
                if (SLAB_BUSY != page->slab) {
                    for (m = 1, i = 0; m; m <<= 1, i++) {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if (SLAB_BUSY == page->slab) {
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
        else {   /* shift > slab_exact_shift */
            n = slab_get_page_shift() - (page->slab & SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t) 1 << n) - 1;
            mask = n << SLAB_MAP_SHIFT;

            do {
                if ((page->slab & SLAB_MAP_MASK) != mask) {
                    for (m = (uintptr_t) 1 << SLAB_MAP_SHIFT, i = 0;
                         m & mask;
                         m <<= 1, i++)
                    {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if ((page->slab & SLAB_MAP_MASK) == mask) {
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

    if (page) {
        if (shift < slab_get_exact_shift()) {
            p = (page - pool->pages) << slab_get_page_shift();
            bitmap = (uintptr_t *) (pool->start + p);

            s = 1 << shift;
            n = (1 << (slab_get_page_shift() - shift)) / 8 / s;

            if (n == 0) {
                n = 1;
            }

            bitmap[0] = (2 << n) - 1;

            map = (1 << (slab_get_page_shift() - shift)) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
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
        else if (shift == slab_get_exact_shift()) {
            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | SLAB_EXACT;

            slots[slot].next = page;

            p = (page - pool->pages) << slab_get_page_shift();
            p += (uintptr_t) pool->start;
            goto done;
        }
        else {   /* shift > slab_exact_shift */
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

    if (0 != p) {
        memset((void *)p, 0, size);
    }

    spin_unlock(&pool->lock);    /* 解锁 */

    return (void *) p;
}

/******************************************************************************
 **函数名称: slab_dealloc
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
void slab_dealloc(slab_pool_t *pool, void *p)
{
    size_t size;
    uintptr_t slab, m, *bitmap;
    uint32_t n, type, slot, shift, map;
    slab_page_t *slots, *page;

    spin_lock(&pool->lock);    /* 加锁 */

    n = ((u_char *) p - pool->start) >> slab_get_page_shift();
    page = &pool->pages[n];
    slab = page->slab;
    type = page->prev & SLAB_PAGE_MASK;

    switch (type) {
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

            if (bitmap[n] & m) {
                if (NULL == page->next) {
                    slots = (slab_page_t *)((u_char *) pool + sizeof(slab_pool_t));
                    slot = shift - pool->min_shift;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | SLAB_SMALL;
                    page->next->prev = (uintptr_t) page | SLAB_SMALL;
                }

                bitmap[n] &= ~m;

                n = (1 << (slab_get_page_shift() - shift)) / 8 / (1 << shift);

                if (n == 0) {
                    n = 1;
                }

                if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                    goto done;
                }

                map = (1 << (slab_get_page_shift() - shift)) / (sizeof(uintptr_t) * 8);

                for (n = 1; n < map; n++) {
                    if (bitmap[n]) {
                        goto done;
                    }
                }

                slab_dealloc_pages(pool, page, 1);

                goto done;
            }
            goto chunk_already_free;
        }
        case SLAB_EXACT:
        {
            m = (uintptr_t) 1 <<
                    (((uintptr_t) p & (slab_get_page_size() - 1)) >> slab_get_exact_shift());
            size = slab_get_exact_size();

            if ((uintptr_t) p & (size - 1)) {
                goto wrong_chunk;
            }

            if (slab & m) {
                if (SLAB_BUSY == slab) {
                    slots = (slab_page_t *)((u_char *) pool + sizeof(slab_pool_t));
                    slot = slab_get_exact_shift() - pool->min_shift;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | SLAB_EXACT;
                    page->next->prev = (uintptr_t) page | SLAB_EXACT;
                }

                page->slab &= ~m;

                if (page->slab) {
                    goto done;
                }

                slab_dealloc_pages(pool, page, 1);
                goto done;
            }
            goto chunk_already_free;
        }
        case SLAB_BIG:
        {
            shift = slab & SLAB_SHIFT_MASK;
            size = 1 << shift;

            if ((uintptr_t) p & (size - 1)) {
                goto wrong_chunk;
            }

            m = (uintptr_t) 1 << ((((uintptr_t) p & (slab_get_page_size() - 1)) >> shift)
                                  + SLAB_MAP_SHIFT);

            if (slab & m) {
                if (NULL == page->next) {
                    slots = (slab_page_t *)((u_char *) pool + sizeof(slab_pool_t));
                    slot = shift - pool->min_shift;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | SLAB_BIG;
                    page->next->prev = (uintptr_t) page | SLAB_BIG;
                }

                page->slab &= ~m;

                if (page->slab & SLAB_MAP_MASK) {
                    goto done;
                }

                slab_dealloc_pages(pool, page, 1);

                goto done;
            }
            goto chunk_already_free;
        }
        case SLAB_PAGE:
        {
            if ((uintptr_t) p & (slab_get_page_size() - 1)) {
                goto wrong_chunk;
            }

            if (SLAB_PAGE_FREE == slab) {
                log_error(pool->log, "Page is already free");
                goto fail;
            }

            if (SLAB_PAGE_BUSY == slab) {
                log_error(pool->log, "Pointer to wrong page");
                goto fail;
            }

            n = ((u_char *) p - pool->start) >> slab_get_page_shift();
            size = slab & ~SLAB_PAGE_START;

            slab_dealloc_pages(pool, &pool->pages[n], size);

            slab_junk(p, size << slab_get_page_shift());

            spin_unlock(&pool->lock); /* 解锁 */
            return;
        }
    }

    /* not reached */
    spin_unlock(&pool->lock); /* 解锁 */
    return;
done:
    slab_junk(p, size);

    spin_unlock(&pool->lock);
    return;
wrong_chunk:
    log_error(pool->log, "Pointer to wrong chunk");
    goto fail;
chunk_already_free:
    log_error(pool->log, "Chunk is already free");
fail:
    spin_unlock(&pool->lock); /* 解锁 */
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
    slab_page_t *page, *p;


    for (page = pool->free.next; page != &pool->free; page = page->next) {
        if (page->slab >= pages) {
            if (page->slab > pages) {
                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            }
            else {
                p = (slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | SLAB_PAGE_START;
            page->next = NULL;
            page->prev = SLAB_PAGE;

            if (0 == --pages) {
                return page;
            }

            for (p = page + 1; pages; pages--) {
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
 **函数名称: slab_dealloc_pages
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
static void slab_dealloc_pages(slab_pool_t *pool, slab_page_t *page, uint32_t pages)
{
    slab_page_t *prev;

    page->slab = pages--;

    if (pages) {
        memset(&page[1], 0, pages * sizeof(slab_page_t));
    }

    if (page->next) {
        prev = (slab_page_t *) (page->prev & ~SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}

/******************************************************************************
 **函数名称: slab_creat_by_calloc
 **功    能: 通过calloc()申请SLAB空间
 **输入参数:
 **     size: 总空间
 **输出参数:
 **返    回: Slab对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.12 #
 ******************************************************************************/
slab_pool_t *slab_creat_by_calloc(size_t size, log_cycle_t *log)
{
    void *addr;
    slab_pool_t *pool;

    addr = (void *)calloc(1, size);
    if (NULL == addr) {
        return NULL;
    }

    pool = slab_init(addr, size, log);
    if (NULL == pool) {
        free(addr);
        return NULL;
    }

    return pool;
}
#else /*__MEM_LEAK_CHECK__*/
slab_pool_t *slab_init(void *addr, size_t size, log_cycle_t *log) { return calloc(1, sizeof(slab_pool_t)); }
void *slab_alloc(slab_pool_t *pool, size_t size) { return calloc(1, size); }
void slab_dealloc(slab_pool_t *pool, void *p) { free(p); }
slab_pool_t *slab_creat_by_calloc(size_t size, log_cycle_t *log) { return calloc(1, size); }
#endif /*__MEM_LEAK_CHECK__*/

/******************************************************************************
 **函数名称: slab_alloc_ex
 **功    能: 从Slab中申请内存空间
 **输入参数:
 **     pool: Slab对象
 **     size: 申请的空间大小
 **输出参数:
 **返    回: 内存地址
 **实现描述: 当申请的内存空间超过4KB时, 直接从操作系统中申请.
 **注意事项: 此内存机制只适合"小内存块"的空间分配, 小内存指的是小于4K的内存块.
 **          如果反复进行大量大小内存分配的混合空间申请和释放, "可能"出现分配空间
 **          失败的情况.
 **作    者: # Qifeng.zou # 2015.08.22 23:59:41 #
 ******************************************************************************/
void *slab_alloc_ex(slab_pool_t *pool, size_t size)
{
    if (size > SLAB_PAGE_SIZE) {
        return malloc(size);
    }

    return slab_alloc(pool, size);
}

/******************************************************************************
 **函数名称: slab_dealloc_ex
 **功    能: 释放从Slab申请的内存空间
 **输入参数:
 **     pool: Slab对象
 **     p: 内存起始地址
 **输出参数:
 **返    回: VOID
 **实现描述: 当指针p不在slab范围时, 直接让操作系统回收内存.
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.23 00:03:26 #
 ******************************************************************************/
void slab_dealloc_ex(slab_pool_t *pool, void *p)
{
    if ((u_char *)p < pool->start || (u_char *)p > pool->end) {
        free(p); /* 不在内存池的空间 */
        return;
    }

    slab_dealloc(pool, p);
}

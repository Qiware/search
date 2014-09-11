/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mem_pool.c
 ** 版本号: 1.0
 ** 描  述: 内存池的实现
 **         设计思路与NGINX的一致
 ** 作  者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "mem_pool.h"
#include "xdo_unistd.h"

static void *mem_pool_alloc_block(mem_pool_t *pool, size_t size);
static void *mem_pool_alloc_large(mem_pool_t *pool, size_t size);

/******************************************************************************
 **函数名称: mem_pool_creat
 **功    能: 创建内存池
 **输入参数: 
 **     size: 内存池大小
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
mem_pool_t *mem_pool_creat(size_t size)
{
    mem_pool_t  *p;

    p = xdo_mem_align(MEM_POOL_ALIGNMENT, size);
    if (NULL == p)
    {
        return NULL;
    }

    p->d.last = (u_char *) p + sizeof(mem_pool_t);
    p->d.end = (u_char *) p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(mem_pool_t);
    p->max = (size < MEM_POOL_MAX_ALLOC_FROM_POOL) ? size : MEM_POOL_MAX_ALLOC_FROM_POOL;

    p->current = (mem_pool_t *)p;
    p->large = NULL;
    p->cleanup = NULL;

    return p;
}

/******************************************************************************
 **函数名称: mem_pool_destroy
 **功    能: 销毁内存池
 **输入参数: 
 **     pool: 内存池
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void mem_pool_destroy(mem_pool_t *pool)
{
    mem_pool_t          *p, *n;
    mem_pool_large_t    *l;
    mem_pool_cleanup_t  *c;

    for (c = pool->cleanup; c; c = c->next)
    {
        if (c->handler)
        {
            //ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
            //               "run cleanup: %p", c);
            c->handler(c->data);
        }
    }

    for (l = pool->large; l; l = l->next)
    {
        //ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc);

        if (l->alloc)
        {
            free(l->alloc);
        }
    }

#if (MEM_POOL_DEBUG)

    /*
     * we could allocate the pool->log from this pool
     * so we cannot use this log while free()ing the pool
     */

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next)
    {
        //ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
        //               "free: %p, unused: %uz", p, p->d.end - p->d.last);

        if (NULL == n)
        {
            break;
        }
    }

#endif

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next)
    {
        free(p);

        if (NULL == n)
        {
            break;
        }
    }
}

/******************************************************************************
 **函数名称: mem_pool_reset
 **功    能: 重置内存池
 **输入参数: 
 **     pool: 内存池
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void mem_pool_reset(mem_pool_t *pool)
{
    mem_pool_t *p;
    mem_pool_large_t *l;

    for (l = pool->large; l; l = l->next)
    {
        if (l->alloc)
        {
            free(l->alloc);
        }
    }

    for (p = pool; p; p = p->d.next)
    {
        p->d.last = (u_char *) p + sizeof(mem_pool_t);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->large = NULL;
}

/******************************************************************************
 **函数名称: mem_pool_alloc
 **功    能: 申请内存
 **输入参数: 
 **     pool: 内存池
 **     size: 申请SIZE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void *mem_pool_alloc(mem_pool_t *pool, size_t size)
{
    u_char      *m;
    mem_pool_t  *p;

    if (size <= pool->max)
    {
        p = pool->current;

        do
        {
            m = mem_align_ptr(p->d.last, PTR_ALIGNMENT);
            if ((size_t) (p->d.end - m) >= size)
            {
                p->d.last = m + size;

                return m;
            }

            p = p->d.next;
        } while (p);

        return mem_pool_alloc_block(pool, size);
    }

    return mem_pool_alloc_large(pool, size);
}

/******************************************************************************
 **函数名称: mem_pool_nalloc
 **功    能: 申请内存
 **输入参数: 
 **     pool: 内存池
 **     size: 申请SIZE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void *mem_pool_nalloc(mem_pool_t *pool, size_t size)
{
    u_char      *m;
    mem_pool_t  *p;

    if (size <= pool->max)
    {
        p = pool->current;

        do
        {
            m = p->d.last;

            if ((size_t) (p->d.end - m) >= size)
            {
                p->d.last = m + size;

                return m;
            }

            p = p->d.next;

        } while (p);

        return mem_pool_alloc_block(pool, size);
    }

    return mem_pool_alloc_large(pool, size);
}

/******************************************************************************
 **函数名称: mem_pool_alloc_block
 **功    能: 申请内存块
 **输入参数: 
 **     pool: 内存池
 **     size: 申请SIZE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
static void *mem_pool_alloc_block(mem_pool_t *pool, size_t size)
{
    u_char      *m;
    size_t       psize;
    mem_pool_t  *p, *new, *current;

    psize = (size_t) (pool->d.end - (u_char *) pool);

    m = xdo_mem_align(MEM_POOL_ALIGNMENT, psize);
    if (NULL == m)
    {
        return NULL;
    }

    new = (mem_pool_t *) m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof(mem_pool_data_t);
    m = mem_align_ptr(m, PTR_ALIGNMENT);
    new->d.last = m + size;

    current = pool->current;

    for (p = current; p->d.next; p = p->d.next)
    {
        if (p->d.failed++ > 4)
        {
            current = p->d.next;
        }
    }

    p->d.next = new;

    pool->current = current ? current : new;

    return m;
}

/******************************************************************************
 **函数名称: mem_pool_alloc_large
 **功    能: 申请大内存块
 **输入参数: 
 **     pool: 内存池
 **     size: 申请SIZE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
static void *mem_pool_alloc_large(mem_pool_t *pool, size_t size)
{
    void   *p;
    int    n;
    mem_pool_large_t  *large;

    p = malloc(size);
    if (NULL == p)
    {
        return NULL;
    }

    n = 0;

    for (large = pool->large; large; large = large->next)
    {
        if (NULL == large->alloc)
        {
            large->alloc = p;
            return p;
        }

        if (n++ > 3)
        {
            break;
        }
    }

    large = mem_pool_alloc(pool, sizeof(mem_pool_large_t));
    if (NULL == large)
    {
        free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

/******************************************************************************
 **函数名称: mem_pool_mem_align
 **功    能: 申请内存对齐的空间
 **输入参数: 
 **     pool: 内存池
 **     size: 申请SIZE
 **     alignment: 对齐尺寸
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void *mem_pool_mem_align(mem_pool_t *pool, size_t size, size_t alignment)
{
    void *p;
    mem_pool_large_t *large;

    p = xdo_mem_align(alignment, size);
    if (NULL == p)
    {
        return NULL;
    }

    large = mem_pool_alloc(pool, sizeof(mem_pool_large_t));
    if (NULL == large)
    {
        free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

/******************************************************************************
 **函数名称: mem_pool_free
 **功    能: 释放内存
 **输入参数: 
 **     pool: 内存池
 **     p: 内存地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
int mem_pool_free(mem_pool_t *pool, void *p)
{
    mem_pool_large_t  *l;

    for (l = pool->large; l; l = l->next)
    {
        if (p == l->alloc)
        {
            //ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
            //               "free: %p", l->alloc);
            free(l->alloc);
            l->alloc = NULL;

            return 0;
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: mem_pool_calloc
 **功    能: 申请内存
 **输入参数: 
 **     pool: 内存池
 **     size: 申请SIZE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void *mem_pool_calloc(mem_pool_t *pool, size_t size)
{
    void *p;

    p = mem_pool_alloc(pool, size);
    if (p)
    {
        memset(p, 0, size);
    }

    return p;
}

/******************************************************************************
 **函数名称: mem_pool_cleanup_add
 **功    能: 添加Cleanup对象
 **输入参数: 
 **     pool: 内存池
 **     size: 清空SIZE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
mem_pool_cleanup_t *mem_pool_cleanup_add(mem_pool_t *p, size_t size)
{
    mem_pool_cleanup_t  *c;

    c = mem_pool_alloc(p, sizeof(mem_pool_cleanup_t));
    if (NULL == c)
    {
        return NULL;
    }

    if (size)
    {
        c->data = mem_pool_alloc(p, size);
        if (NULL == c->data)
        {
            return NULL;
        }
    }
    else
    {
        c->data = NULL;
    }

    c->handler = NULL;
    c->next = p->cleanup;

    p->cleanup = c;

    //ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, p->log, 0, "add cleanup: %p", c);

    return c;
}

/******************************************************************************
 **函数名称: mem_pool_run_cleanup_file
 **功    能: 执行Cleanup file
 **输入参数: 
 **     pool: 内存池
 **     fd: 文件描述符
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void mem_pool_run_cleanup_file(mem_pool_t *p, int fd)
{
    mem_pool_cleanup_t       *c;
    mem_pool_cleanup_file_t  *cf;

    for (c = p->cleanup; c; c = c->next)
    {
        if (c->handler == mem_pool_cleanup_file)
        {
            cf = c->data;
            if (cf->fd == fd)
            {
                c->handler(cf);
                c->handler = NULL;
                return;
            }
        }
    }
}

/******************************************************************************
 **函数名称: mem_pool_cleanup_file
 **功    能: Cleanup file
 **输入参数: 
 **     data:
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void mem_pool_cleanup_file(void *data)
{
    mem_pool_cleanup_file_t  *c = data;

    //ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d",
    //               c->fd);

    close(c->fd);
}

/******************************************************************************
 **函数名称: mem_pool_delete_file
 **功    能: Delete file
 **输入参数: 
 **     data:
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void mem_pool_delete_file(void *data)
{
    int  err;
    mem_pool_cleanup_file_t  *c = data;

    //ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d %s",
    //               c->fd, c->name);

    if (!unlink((const char *)c->name))
    {
        err = errno;

        if (ENOENT != err)
        {
            //ngx_log_error(NGX_LOG_CRIT, c->log, err,
            //              unlink_n " \"%s\" failed", c->name);
        }
    }

    close(c->fd);
}

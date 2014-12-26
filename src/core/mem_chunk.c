/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mem_chunk.c
 ** 版本号: 1.0
 ** 描  述: 内存块的实现(也属于内存池算法)
 **         所有数据块大小都一致.
 **         此内存池主要用于存储大小一致的数据块的使用场景
 ** 作  者: # Qifeng.zou # 2014.12.18 #
 ******************************************************************************/
#include "log.h"
#include "common.h"
#include "syscall.h"
#include "mem_chunk.h"


/******************************************************************************
 **函数名称: mem_chunk_creat
 **功    能: 创建内存池
 **输入参数: 
 **     num: 内存块数
 **     size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.18 #
 ******************************************************************************/
mem_chunk_t *mem_chunk_creat(int num, size_t size)
{
    int i, m, idx;
    uint32_t *bitmap;
    mem_chunk_t *chunk;
    mem_chunk_page_t *page;

    /* 1. 创建对象 */
    chunk = (mem_chunk_t *)calloc(1, sizeof(mem_chunk_t));
    if (NULL == chunk)
    {
        return NULL;
    }

    chunk->num = num;
    chunk->size = size;

    /* 2. 计算页数, 并分配页空间 */
    chunk->pages = math_ceiling(num, MEM_CHUNK_PAGE_SLOT_NUM);

    chunk->page = (mem_chunk_page_t *)calloc(chunk->pages, sizeof(mem_chunk_page_t));
    if (NULL == chunk->page)
    {
        free(chunk);
        return NULL;
    }

    /* 3 分配总内存空间 */
    chunk->addr = (char *)calloc(num, size);
    if (NULL == chunk->addr)
    {
        free(chunk->page);
        free(chunk);
        return NULL;
    }

    /* 4. 设置位图信息 */
    for (idx=0; idx<chunk->pages; ++idx)
    {
        page = &chunk->page[idx];

        spin_lock_init(&page->lock);

        /* 3.1 设置bitmap */
        if (idx == (chunk->pages - 1))
        {
            m = (num - idx*MEM_CHUNK_PAGE_SLOT_NUM) % 32;

            page->bitmaps = math_ceiling(num - idx*MEM_CHUNK_PAGE_SLOT_NUM, 32);
        }
        else
        {
            m = MEM_CHUNK_PAGE_SLOT_NUM % 32;

            page->bitmaps = math_ceiling(MEM_CHUNK_PAGE_SLOT_NUM, 32);
        }

        page->bitmap = (uint32_t *)calloc(page->bitmaps, sizeof(uint32_t));
        if (NULL == page->bitmap)
        {
            free(chunk->page);
            free(chunk->addr);
            free(chunk);
            return NULL;
        }

        if (m)
        {
            bitmap = &page->bitmap[page->bitmaps - 1];

            for (i=m; i<32; i++)
            {
                *bitmap |= (1 << i);
            }
        }

        /* 3.2 设置数据空间 */
        page->addr = chunk->addr + idx * MEM_CHUNK_PAGE_SLOT_NUM * size;
    }

    return chunk;
}

/******************************************************************************
 **函数名称: mem_chunk_alloc
 **功    能: 申请空间
 **输入参数: 
 **     chunk: 内存块对象
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.18 #
 ******************************************************************************/
void *mem_chunk_alloc(mem_chunk_t *chunk)
{
    uint32_t i, j, n, p;
    uint32_t *bitmap;
    mem_chunk_page_t *page;

    n = rand(); /* 随机选择页 */

    for (p=0; p<chunk->pages; ++p, ++n)
    {
        n %= chunk->pages;
        page = &chunk->page[n];

        spin_lock(&page->lock);

        for (i=0; i<page->bitmaps; ++i)
        {
            if (0xFFFFFFFF == page->bitmap[i])
            {
                continue;
            }

            bitmap = page->bitmap + i;
            for (j=0; j<32; ++j)
            {
                if (!((*bitmap >> j) & 1))
                {
                    *bitmap |= (1 << j);

                    spin_unlock(&page->lock);

                    return page->addr + (i*32 + j)*chunk->size;
                }
            }
        }

        spin_unlock(&page->lock);
    }
    return NULL;
}

/******************************************************************************
 **函数名称: mem_chunk_dealloc
 **功    能: 回收空间
 **输入参数: 
 **     chunk: 内存块对象
 **     p: 需要释放的空间地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.18 #
 ******************************************************************************/
void mem_chunk_dealloc(mem_chunk_t *chunk, void *p)
{
    int i, j, n;

    n = (p - chunk->addr) / (MEM_CHUNK_PAGE_SLOT_NUM * chunk->size);        /* 计算页号 */
    i = (p - chunk->page[n].addr) / (32 * chunk->size);                     /* 计算页内bitmap索引 */
    j = (p - (chunk->page[n].addr + i * (32 * chunk->size))) / chunk->size; /* 计算bitmap内偏移 */

    spin_lock(&chunk->page[n].lock);
    chunk->page[n].bitmap[i] &= ~(1 << j);
    spin_unlock(&chunk->page[n].lock);
}

/******************************************************************************
 **函数名称: mem_chunk_destroy
 **功    能: 销毁内存块
 **输入参数: 
 **     chunk: 内存块对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.18 #
 ******************************************************************************/
void mem_chunk_destroy(mem_chunk_t *chunk)
{
    int i;
    mem_chunk_page_t *page;

    for (i=0; i<chunk->pages; ++i)
    {
        page = &chunk->page[i];

        free(page->bitmap);
        spin_lock_destroy(&page->lock);
    }

    free(chunk->page);
    free(chunk->addr);
    free(chunk);
}

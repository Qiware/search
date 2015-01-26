/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: shm_pool.c
 ** 版本号: 1.0
 ** 描  述: 内存块的实现(也属于内存池算法)
 **         所有数据块大小都一致.
 **         此内存池主要用于存储大小一致的数据块的使用场景
 **
 **         内存池算法的空间布局:
 **          ---------- -------- -----------------------------------------------
 **         |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **         |\基础信息\|X页对象X|///////可///////用////////空////////间/////////|
 **         |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **          ---------- -------- -----------------------------------------------
 **         ^          ^        ^                                               ^
 **         |          |        |                                               |
 **        addr       page     data                                            end
 **
 ** 作  者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
#include "log.h"
#include "common.h"
#include "syscall.h"
#include "shm_opt.h"
#include "shm_pool.h"

#define SHM_POOL_BITMAP_BUSY    (0xFFFFFFFF)    /* 都已分配 */

#define shm_pool_get_page_addr(info, n)         /* 获取页地址 */\
    (shm_pool_page_t *)((void *)(info) + (info)->page_off + (n) * sizeof(shm_pool_page_t))


/******************************************************************************
 **函数名称: shm_pool_init
 **功    能: 创建内存池
 **输入参数:
 **     addr: 起始地址
 **     num: 内存块数
 **     unit_size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 ** 内存池算法的空间布局:
 **     ---------- -------- -----------------------------------------------
 **    |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **    |\基础信息\|X页对象X|///////可///////用////////空////////间/////////|
 **    |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **     ---------- -------- -----------------------------------------------
 **    ^          ^        ^                                               ^
 **    |          |        |                                               |
 **   addr       page     data                                            end
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
shm_pool_t *shm_pool_init(void *addr, int max, size_t unit_size)
{
    int i, m, idx, page_num, n;
    uint32_t *bitmap;
    shm_pool_info_t *info;
    shm_pool_page_t *page;
    shm_pool_t *pool;

    /* 1. 计算内存页 */
    page_num = div_ceiling(max, SHM_POOL_PAGE_SLOT_NUM);

    /* 2. 设置结构信息 */
    info = (shm_pool_info_t *)addr;

    info->num = max;
    info->unit_size = unit_size;
    info->page_size = SHM_POOL_PAGE_SLOT_NUM * unit_size;
    info->page_num = page_num;
    info->page_off = sizeof(shm_pool_info_t);
    info->data_off = info->page_off + page_num * sizeof(shm_pool_page_t);

    /* 3. 设置位图信息 */
    page = (shm_pool_page_t *)(addr + info->page_off);

    for (idx=0; idx<info->page_num; ++idx, ++page)
    {

        spin_lock_init(&page->lock);

        /* 3.1 设置bitmap */
        if (idx != (info->page_num - 1))
        {
            page->bitmap_num = div_ceiling(SHM_POOL_PAGE_SLOT_NUM, 32);
        }
        else
        {
            page->bitmap_num = div_ceiling(max - idx*SHM_POOL_PAGE_SLOT_NUM, 32);

            bitmap = &page->bitmap[page->bitmap_num - 1];

            m = (max - idx*SHM_POOL_PAGE_SLOT_NUM) % 32;
            for (i=m; i<32; ++i)
            {
                *bitmap |= (1 << i);
            }

            bitmap = &page->bitmap[page->bitmap_num];
            for (n = page->bitmap_num; n < SHM_POOL_BITMAP_MAX; ++n, ++bitmap)
            {
                for (i=0; i<32; ++i)
                {
                    *bitmap |= (1 << i);
                }
            }
        }

        /* 3.2 设置数据空间 */
        page->data_off = info->data_off + idx * info->page_size;
    }

    /* 4 创建内存池对象 */
    pool = (shm_pool_t *)calloc(1, sizeof(shm_pool_t));
    if (NULL == pool)
    {
        return NULL;
    }

    pool->addr = (void *)addr;
    pool->info = info;
    pool->page = page;
    pool->page_data = (void **)calloc(info->page_num, sizeof(void *));
    if (NULL == pool->page_data)
    {
        free(pool);
        return NULL;
    }

    for (idx=0; idx<info->page_num; ++idx)
    {
        pool->page_data[idx] = (void *)(addr + page[idx].data_off);
    }

    return pool;
}

/******************************************************************************
 **函数名称: shm_pool_get
 **功    能: 获取内存池对象
 **输入参数:
 **     addr: 起始地址
 **     num: 内存块数
 **     unit_size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 ** 内存池算法的空间布局:
 **     ---------- -------- -----------------------------------------------
 **    |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **    |\基础信息\|X页对象X|///////可///////用////////空////////间/////////|
 **    |\\\\\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **     ---------- -------- -----------------------------------------------
 **    ^          ^        ^                                               ^
 **    |          |        |                                               |
 **   addr       page     data                                            end
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
shm_pool_t *shm_pool_get(void *addr)
{
    int idx;
    shm_pool_info_t *info;
    shm_pool_page_t *page;
    shm_pool_t *pool;

    /* 1. 获取结构地址 */
    info = (shm_pool_info_t *)addr;
    page = (shm_pool_page_t *)(addr + info->page_off);

    /* 2. 创建内存池对象 */
    pool = (shm_pool_t *)calloc(1, sizeof(shm_pool_t));
    if (NULL == pool)
    {
        return NULL;
    }

    /* 3. 设置指针地址 */
    pool->addr = (void *)addr;
    pool->info = info;
    pool->page = page;
    pool->page_data = (void **)calloc(info->page_num, sizeof(void *));
    if (NULL == pool->page_data)
    {
        free(pool);
        return NULL;
    }

    for (idx=0; idx<info->page_num; ++idx)
    {
        pool->page_data[idx] = (void *)(addr + page[idx].data_off);
    }

    return pool;
}

/******************************************************************************
 **函数名称: shm_pool_alloc
 **功    能: 申请空间
 **输入参数: 
 **     pool: 内存池
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
void *shm_pool_alloc(shm_pool_t *pool)
{
    uint32_t i, j, idx, m;
    uint32_t *bitmap;
    shm_pool_page_t *page;
    shm_pool_info_t *info = pool->info;

    idx = rand(); /* 随机选择页 */

    for (m=0; m<info->page_num; ++m, ++idx)
    {
        idx %= info->page_num;
        page = pool->page + idx;

        spin_lock(&page->lock);

        for (i=0; i<page->bitmap_num; ++i)
        {
            if (SHM_POOL_BITMAP_BUSY == page->bitmap[i])
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

                    return (pool->page_data[idx] + (i*32 + j) * info->unit_size);
                }
            }
        }

        spin_unlock(&page->lock);
    }

    return NULL;
}

/******************************************************************************
 **函数名称: shm_pool_dealloc
 **功    能: 回收空间
 **输入参数: 
 **     info: 内存块对象
 **     p: 需要释放的空间地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
void shm_pool_dealloc(shm_pool_t *pool, void *p)
{
    int i, j, idx;
    shm_pool_page_t *page;
    shm_pool_info_t *info = pool->info;

    idx = (p - pool->page_data[0]) / info->page_size;   /* 计算页号 */

    page = pool->page + idx;

    i = (p - pool->page_data[idx]) / (info->unit_size << 5); /* 计算页内bitmap索引 */
    j = (p - (pool->page_data[idx] + i * (info->unit_size << 5))) / info->unit_size;   /* 计算bitmap内偏移 */

    spin_lock(&page->lock);
    page->bitmap[i] &= ~(1 << j);
    spin_unlock(&page->lock);
    return;
}

/******************************************************************************
 **函数名称: shm_pool_destroy
 **功    能: 销毁内存块
 **输入参数: 
 **     info: 内存块对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
void shm_pool_destroy(shm_pool_t *info)
{
    return;
}

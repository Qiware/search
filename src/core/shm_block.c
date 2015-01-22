/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: shm_block.c
 ** 版本号: 1.0
 ** 描  述: 内存块的实现(也属于内存池算法)
 **         所有数据块大小都一致.
 **         此内存池主要用于存储大小一致的数据块的使用场景
 ** 作  者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
#include "log.h"
#include "common.h"
#include "syscall.h"
#include "shm_opt.h"
#include "shm_block.h"


/******************************************************************************
 **函数名称: shm_block_creat
 **功    能: 创建内存池
 **输入参数: 
 **     num: 内存块数
 **     size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
shm_block_t *shm_block_creat(int num, size_t size)
{
    key_t key = 0x033034;
    void *addr;
    int i, m, idx, pages;
    uint32_t *bitmap;
    size_t shm_size;
    shm_block_t *blk;
    shm_block_page_t *page;

    /* 1. 计算内存大小 */
    pages = div_ceiling(num, SHM_BLOCK_PAGE_SLOT_NUM);

    shm_size = sizeof(shm_block_t) + pages * sizeof(shm_block_page_t) + num * size;

    /* 2. 创建共享内存 */
    addr = shm_creat(key, shm_size);
    if (NULL == addr)
    {
        return NULL;
    }

    /* 3. 设置结构信息 */
    blk = (shm_block_t *)addr;

    blk->num = num;
    blk->size = size;
    blk->pages = pages;
    blk->page = (shm_block_page_t *)(addr + sizeof(shm_block_t));
    blk->addr = (char *)(addr + sizeof(shm_block_t) + pages * sizeof(shm_block_page_t));

    /* 4. 设置位图信息 */
    for (idx=0; idx<blk->pages; ++idx)
    {
        page = &blk->page[idx];

        spin_lock_init(&page->lock);

        /* 3.1 设置bitmap */
        if (idx == (blk->pages - 1))
        {
            m = (num - idx*SHM_BLOCK_PAGE_SLOT_NUM) % 32;

            page->bitmaps = div_ceiling(num - idx*SHM_BLOCK_PAGE_SLOT_NUM, 32);
        }
        else
        {
            m = SHM_BLOCK_PAGE_SLOT_NUM % 32;

            page->bitmaps = div_ceiling(SHM_BLOCK_PAGE_SLOT_NUM, 32);
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
        page->addr = blk->addr + idx * SHM_BLOCK_PAGE_SLOT_NUM * size;
    }

    return blk;
}

/******************************************************************************
 **函数名称: shm_block_alloc
 **功    能: 申请空间
 **输入参数: 
 **     blk: 内存块对象
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
void *shm_block_alloc(shm_block_t *blk)
{
    uint32_t i, j, n, p;
    uint32_t *bitmap;
    shm_block_page_t *page;

    n = rand(); /* 随机选择页 */

    for (p=0; p<blk->pages; ++p, ++n)
    {
        n %= blk->pages;
        page = &blk->page[n];

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

                    return page->addr + (i*32 + j)*blk->size;
                }
            }
        }

        spin_unlock(&page->lock);
    }
    return NULL;
}

/******************************************************************************
 **函数名称: shm_block_dealloc
 **功    能: 回收空间
 **输入参数: 
 **     blk: 内存块对象
 **     p: 需要释放的空间地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
void shm_block_dealloc(shm_block_t *blk, void *p)
{
    int i, j, n;

    n = (p - blk->addr) / (SHM_BLOCK_PAGE_SLOT_NUM * blk->size);        /* 计算页号 */
    i = (p - blk->page[n].addr) / (32 * blk->size);                     /* 计算页内bitmap索引 */
    j = (p - (blk->page[n].addr + i * (32 * blk->size))) / blk->size;   /* 计算bitmap内偏移 */

    spin_lock(&blk->page[n].lock);
    blk->page[n].bitmap[i] &= ~(1 << j);
    spin_unlock(&blk->page[n].lock);
}

/******************************************************************************
 **函数名称: shm_block_destroy
 **功    能: 销毁内存块
 **输入参数: 
 **     blk: 内存块对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
void shm_block_destroy(shm_block_t *blk)
{
    return;
}

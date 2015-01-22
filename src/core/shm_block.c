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

#define shm_block_get_page_addr(blk, n) /* 获取页地址 */\
    (shm_block_page_t *)((void *)(blk) + (blk)->page_off + (n) * sizeof(shm_block_page_t))


/******************************************************************************
 **函数名称: shm_block_creat
 **功    能: 创建内存池
 **输入参数: 
 **     num: 内存块数
 **     unit_size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 ** 内存池算法的空间布局:
 **     ------ -------- -----------------------------------------------
 **    |\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **    |\对象\|X页对象X|///////可///////用////////空////////间/////////|
 **    |\\\\\\|XX|XX|XX|////////|////////|/////////|/////////|/////////|
 **     ------ -------- -----------------------------------------------
 **    ^      ^        ^                                               ^
 **    |      |        |                                               |
 **   addr   page     data                                            end
 **作    者: # Qifeng.zou # 2015.01.22 #
 ******************************************************************************/
shm_block_t *shm_block_creat(const char *path, int num, size_t unit_size)
{
    key_t key;
    void *addr;
    int i, m, idx, page_num, n;
    uint32_t *bitmap;
    size_t shm_size;
    shm_block_t *blk;
    shm_block_page_t *page;

    /* 1. 计算内存大小 */
    page_num = div_ceiling(num, SHM_BLOCK_PAGE_SLOT_NUM);

    shm_size = sizeof(shm_block_t) + page_num * sizeof(shm_block_page_t) + num * unit_size;

    /* 2. 创建共享内存 */
    key = shm_ftok(path, 0);
    if (-1 == key)
    {
        return NULL;
    }

    addr = shm_creat(key, shm_size);
    if (NULL == addr)
    {
        return NULL;
    }

    /* 3. 设置结构信息 */
    blk = (shm_block_t *)addr;

    blk->num = num;
    blk->unit_size = unit_size;
    blk->page_num = page_num;
    blk->page_off = sizeof(shm_block_t);
    blk->data_off = blk->page_off + page_num * sizeof(shm_block_page_t);

    /* 4. 设置位图信息 */
    page = (shm_block_page_t *)(addr + blk->page_off);

    for (idx=0; idx<blk->page_num; ++idx, ++page)
    {

        spin_lock_init(&page->lock);

        /* 3.1 设置bitmap */
        if (idx == (blk->page_num - 1))
        {
            m = (num - idx*SHM_BLOCK_PAGE_SLOT_NUM) % 32;

            page->bitmap_num = div_ceiling(num - idx*SHM_BLOCK_PAGE_SLOT_NUM, 32);
        }
        else
        {
            m = SHM_BLOCK_PAGE_SLOT_NUM % 32;

            page->bitmap_num = div_ceiling(SHM_BLOCK_PAGE_SLOT_NUM, 32);
        }

        if (m)
        {
            bitmap = &page->bitmap[page->bitmap_num - 1];

            for (i=m; i<32; i++)
            {
                *bitmap |= (1 << i);
            }

            n = page->bitmap_num;
            bitmap = &page->bitmap[page->bitmap_num];
            for (; n < SHM_BLOCK_PAGE_SLOT_NUM/32; ++n, ++bitmap)
            {
                for (i=0; i<32; i++)
                {
                    *bitmap |= (1 << i);
                }
            }
        }

        /* 3.2 设置数据空间 */
        page->data_off = blk->data_off + idx * SHM_BLOCK_PAGE_SLOT_NUM * unit_size;
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
    void *addr = (void *)blk;
    uint32_t i, j, n, p;
    uint32_t *bitmap;
    shm_block_page_t *page;

    n = rand(); /* 随机选择页 */

    for (p=0; p<blk->page_num; ++p, ++n)
    {
        n %= blk->page_num;
        page = shm_block_get_page_addr(blk, n);

        spin_lock(&page->lock);

        for (i=0; i<page->bitmap_num; ++i)
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

                    return (addr + page->data_off + (i*32 + j)*blk->unit_size);
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
    shm_block_page_t *page;
    void *addr = (void *)blk;

    n = (p - addr) / (SHM_BLOCK_PAGE_SLOT_NUM * blk->unit_size);   /* 计算页号 */

    page = shm_block_get_page_addr(blk, n);

    i = (p - (addr + page->data_off)) / (32 * blk->unit_size);     /* 计算页内bitmap索引 */
    j = (p - ((addr + page->data_off) + i * (32 * blk->unit_size))) / blk->unit_size;   /* 计算bitmap内偏移 */

    spin_lock(&page->lock);
    page->bitmap[i] &= ~(1 << j);
    spin_unlock(&page->lock);
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

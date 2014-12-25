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
#include "common.h"
#include "mem_chunk.h"


/******************************************************************************
 **函数名称: mem_chunk_init
 **功    能: 内存池初始化
 **输入参数: 
 **     num: 内存块数
 **     size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.18 #
 ******************************************************************************/
mem_chunk_t *mem_chunk_init(int num, size_t size)
{
    int i, m;
    uint32_t *bitmap;
    mem_chunk_t *chunk;

    /* 1. 创建对象 */
    chunk = (mem_chunk_t *)calloc(1, sizeof(mem_chunk_t));
    if (NULL == chunk)
    {
        return NULL;
    }

   /* 2. 新建BitMap */
    chunk->bitmap_num = num/32;
    m = num%32;
    if (m)
    {
        ++chunk->bitmap_num;
    }

    chunk->bitmap = (uint32_t *)calloc(chunk->bitmap_num, sizeof(uint32_t));
    if (NULL == chunk->bitmap)
    {
        free(chunk);
        return NULL;
    }

    if (m)
    {
        bitmap = &chunk->bitmap[chunk->bitmap_num - 1];

        for (i=m; i<32; i++)
        {
            *bitmap |= (1 << i);
        }
    }

    /* 3. 数据空间 */
    chunk->addr = (char *)calloc(num, size);
    if (NULL == chunk->addr)
    {
        free(chunk->bitmap);
        free(chunk);
        return NULL;
    }

    chunk->num = num;
    chunk->size = size;

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
    uint32_t i, j;
    uint32_t *bitmap;

    spin_lock(&chunk->lock);

    for (i=0; i<chunk->bitmap_num; ++i)
    {
        if (0xFFFFFFFF == chunk->bitmap[i])
        {
            continue;
        }

        bitmap = chunk->bitmap + i;
        for (j=0; j<32; ++j)
        {
            if (!((*bitmap >> j) & 1))
            {
                *bitmap |= (1 << j);
                spin_unlock(&chunk->lock);
                return chunk->addr + (i*32 + j)*chunk->size;
            }
        }
    }

    spin_unlock(&chunk->lock);
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
    int i, j;

    i = (p - chunk->addr) / (chunk->size << 5);
    j = (p - (chunk->addr + i * (chunk->size << 5)))%chunk->size;

    spin_lock(&chunk->lock);
    chunk->bitmap[i] &= ~(1 << j);
    spin_unlock(&chunk->lock);
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
    free(chunk->bitmap);
    free(chunk->addr);
    free(chunk);
    spin_lock_destroy(&chunk->lock);
}

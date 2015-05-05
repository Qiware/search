/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: slot.c
 ** 版本号: 1.0
 ** 描  述: 
 **     通过环形队列实现内存的申请和回收
 ** 作  者: # Qifeng.zou # Tue 05 May 2015 08:47:46 AM CST #
 ******************************************************************************/

#include "log.h"
#include "comm.h"
#include "slot.h"
#include "syscall.h"

/******************************************************************************
 **函数名称: slot_creat
 **功    能: 创建内存池
 **输入参数: 
 **     num: 内存块数
 **     size: 内存块大小
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
slot_t *slot_creat(int num, size_t size)
{
    int i;
    slot_t *slot;
    void *addr, *ptr;

    /* > 创建对象 */
    slot = (slot_t *)calloc(1, sizeof(slot_t));
    if (NULL == slot)
    {
        return NULL;
    }

    slot->max = num;
    slot->size = size;

    slot->ring = ring_creat(num);
    if (NULL == slot->ring)
    {
        free(slot);
        return NULL;
    }

    /* > 申请内存池空间 */
    addr = (void *)calloc(num, size);
    if (NULL == addr)
    {
        ring_destroy(slot->ring);
        free(slot);
        return NULL;
    }

    /* > 插入管理队列 */
    ptr = addr;
    for (i=0; i<num; ++i, ptr += size)
    {
        if (ring_push(slot->ring, ptr))
        {
            ring_destroy(slot->ring);
            free(slot);
            free(addr);
            return NULL;
        }
    }

    return addr;
}

/******************************************************************************
 **函数名称: slot_alloc
 **功    能: 申请空间
 **输入参数: 
 **     slot: 内存块对象
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
void *slot_alloc(slot_t *slot)
{
    return ring_pop(slot->ring);
}

/******************************************************************************
 **函数名称: slot_dealloc
 **功    能: 回收空间
 **输入参数: 
 **     slot: 内存块对象
 **     p: 需要释放的空间地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
void slot_dealloc(slot_t *slot, void *p)
{
    ring_push(slot->ring, p);
}

/******************************************************************************
 **函数名称: slot_destroy
 **功    能: 销毁内存块
 **输入参数: 
 **     slot: 内存块对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
void slot_destroy(slot_t *slot)
{
    ring_destroy(slot->ring);
    free(slot->addr);
    free(slot);
}

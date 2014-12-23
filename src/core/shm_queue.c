/******************************************************************************
 ** Coypright(C) 2013-2014 Xundao technology Co., Ltd
 **
 ** 文件名: shm_queue.c
 ** 版本号: 1.0
 ** 描  述: 共享内存版队列
 **         使用SHM_SLAB机制管理共享内存.
 ** 作  者: # Qifeng.zou # 2014.09.09 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "syscall.h"
#include "shm_slab.h"
#include "shm_queue.h"

/******************************************************************************
 **函数名称: shm_queue_creat
 **功    能: 创建共享内存队列
 **输入参数:
 **     key: 共享内存KEY
 **     max: 队列单元数
 **     size: 队列单元SIZE
 **输出参数:
 **返    回: 共享内存队列
 **实现描述: 
 **     1. 计算内存空间
 **     2. 创建共享内存
 **     3. 初始化标志量
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.10 #
 ******************************************************************************/
shm_queue_t *shm_queue_creat(int key, int max, int size)
{
    int idx;
    void *addr;
    size_t shm_size, off;
    shm_queue_t *shmq;
    shm_queue_node_t *node;

    /* 1. 计算内存空间 */
    shm_size =  sizeof(shm_queue_t) + max * sizeof(shm_queue_node_t);
    shm_size += ((max * size)/getpagesize() + 1) * getpagesize();
    
    /* 2. 创建共享内存 */
    addr = shm_creat(key, shm_size);
    if (NULL == addr)
    {
        return NULL;
    }

    /* 3. 初始化标志量 */
    off = 0;
    shmq = (shm_queue_t *)addr;

    spin_lock_init(&shmq->lock);

    spin_lock(&shmq->lock);

    shmq->num = 0;
    shmq->max = max;
    shmq->size = size;

    off += sizeof(shm_queue_t);
    shmq->base = off;
    shmq->head = off;
    shmq->tail = off;

    node = (shm_queue_node_t *)(addr + shmq->base);
    for (idx=0; idx<max-1; ++idx, ++node)
    {
        off += sizeof(shm_queue_node_t);

        node->id = idx;
        node->status = SHMQ_NODE_STAT_IDLE;
        node->next = off;
        node->data = 0;
    }
    
    node->id = idx;
    node->status = SHMQ_NODE_STAT_IDLE;
    node->next = shmq->base;
    node->data = 0;

    spin_unlock(&shmq->lock);

    return shmq;
}

/******************************************************************************
 **函数名称: shm_queue_attach
 **功    能: 附着共享内存队列
 **输入参数:
 **     key: 共享内存KEY
 **     max: 队列单元数
 **     size: 队列单元SIZE
 **输出参数:
 **返    回: 共享内存队列
 **实现描述: 
 **     1. 计算内存空间
 **     2. 创建共享内存
 **     3. 初始化标志量
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.10 #
 ******************************************************************************/
shm_queue_t *shm_queue_attach(int key, int max, int size)
{
    size_t shm_size;

    /* 1. 计算内存空间 */
    shm_size =  sizeof(shm_queue_t) + max * sizeof(shm_queue_node_t);
    shm_size += ((max * size)/getpagesize() + 1) * getpagesize();
    
    /* 2. 创建共享内存 */
    return (shm_queue_t *)shm_attach(key, shm_size);
}

/******************************************************************************
 **函数名称: shm_queue_push
 **功    能: 压入队列
 **输入参数:
 **     shmq: 共享内存队列
 **     p: 内存地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     注意：内存地址p必须是从shm_queue_malloc()分配的，否则会出严重错误！
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
int shm_queue_push(shm_queue_t *shmq, void *p)
{
    int idx;
    void *addr = (void *)shmq;
    shm_queue_node_t *node = (shm_queue_node_t *)(addr + shmq->tail);

    spin_lock(&shmq->lock);

    /* 1. 检查队列空间 */
    if (shmq->num >= shmq->max)
    {
        spin_unlock(&shmq->lock);
        return -1;
    }

    /* 2. 占用空闲结点 */
    for (idx=0; idx<shmq->max; ++idx)
    {
        if (SHMQ_NODE_STAT_IDLE == node->status)
        {
            break;
        }

        node = (shm_queue_node_t *)(addr + node->next);
    }

    if (idx == shmq->max)
    {
        spin_unlock(&shmq->lock);
        return -1;
    }

    node->status = SHMQ_NODE_STAT_USED;
    node->data = p - addr;

    shmq->tail = node->next;
    ++shmq->num;

    spin_unlock(&shmq->lock);

    return 0;
}

/******************************************************************************
 **函数名称: shm_queue_pop
 **功    能: 出队列
 **输入参数:
 **     shmq: 共享内存队列
 **输出参数:
 **     data: 数据地址
 **返    回: 队列ID
 **实现描述: 
 **注意事项: 
 **     执行该操作后，
 **作    者: # Qifeng.zou # 2014.09.09 #
 ******************************************************************************/
void *shm_queue_pop(shm_queue_t *shmq)
{
    int idx;
    void *p, *addr = (void *)shmq;
    shm_queue_node_t *node;

    if (0 == shmq->num)
    {
        return NULL;
    }

    spin_lock(&shmq->lock);
    if (0 == shmq->num)
    {
        spin_unlock(&shmq->lock);
        return NULL;
    }

    node = (shm_queue_node_t *)(addr + shmq->head);
    for (idx=0; idx<shmq->max; ++idx)
    {
        if (SHMQ_NODE_STAT_USED == node->status)
        {
            shmq->head = node->next;
            break;
        }

        node = (shm_queue_node_t *)(addr + node->next);
    }

    p = (void *)(addr + node->data);

    node->status = SHMQ_NODE_STAT_IDLE;
    node->data = 0;

    spin_unlock(&shmq->lock);

    return p;
}

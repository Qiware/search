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
#include <assert.h>

#include "shm_opt.h"
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
 **      -----------------------------------------------------------------
 **     |      |        |                                                 |
 **     | 队列 | 内存池 |                   可分配空间                    |
 **     |      |        |                                                 |
 **      -----------------------------------------------------------------
 **作    者: # Qifeng.zou # 2014.09.10 #
 ******************************************************************************/
shm_queue_t *shm_queue_creat(int key, int max, int size)
{
    int idx;
    void *addr;
    size_t total, off;
    shm_pool_t *pool;
    shm_queue_t *shmq;
    shm_queue_info_t *info;
    shm_queue_node_t *node;


    /* 1. 新建全局对象 */
    shmq = (shm_queue_t *)calloc(1, sizeof(shm_queue_t));
    if (NULL == shmq)
    {
        return NULL;
    }

    /* 2. 计算内存空间 */
    total = sizeof(shm_queue_info_t);
    total += max * sizeof(shm_queue_node_t);

    total += shm_pool_total(max, size);

    /* 3. 创建共享内存 */
    addr = shm_creat(key, total);
    if (NULL == addr)
    {
        free(shmq);
        return NULL;
    }

    /* 3. 初始化标志量 */
    off = 0;
    info = (shm_queue_info_t *)addr;

    spin_lock_init(&info->lock);

    spin_lock(&info->lock);

    info->num = 0;
    info->max = max;
    info->size = size;

    off += sizeof(shm_queue_info_t);
    info->base = off;
    info->head = off;
    info->tail = off;

    node = (shm_queue_node_t *)(addr + info->base);
    for (idx=0; idx<max-1; ++idx, ++node)
    {
        off += sizeof(shm_queue_node_t);

        node->id = idx;
        node->flag = SHMQ_NODE_FLAG_IDLE;
        node->next = off;
        node->data = 0;
    }
    
    node->id = idx;
    node->flag = SHMQ_NODE_FLAG_IDLE;
    node->next = info->base;
    node->data = 0;

    /* 4. 初始化SHM内存池 */
    info->pool_off = sizeof(shm_queue_info_t) + max * sizeof(shm_queue_node_t);

    pool = shm_pool_init(addr + info->pool_off, max, size);
    if (NULL == pool)
    {
        spin_unlock(&info->lock);
        free(shmq);
        syslog_error("Initialize shm slab failed!");
        return NULL;
    }

    spin_unlock(&info->lock);

    shmq->info = info;
    shmq->pool = pool;

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
 **作    者: # Qifeng.zou # 2015.01.25 #
 ******************************************************************************/
shm_queue_t *shm_queue_attach(int key)
{
    void *addr;
    shm_queue_t *shmq;

    /* 1. 新建队列对象 */
    shmq = (shm_queue_t *)calloc(1, sizeof(shm_queue_t));
    if (NULL == shmq)
    {
        return NULL;
    }

    /* 2. 附着共享内存 */
    addr = (shm_queue_info_t *)shm_attach(key);
    if (NULL == addr)
    {
        free(shmq);
        return NULL;
    }

    /* 3. 设置指针对象 */
    shmq->info = (shm_queue_info_t *)addr;
    shmq->pool = shm_pool_get(addr + shmq->info->pool_off);
    if (NULL == shmq->pool)
    {
        free(shmq);
        return NULL;
    }

    return shmq;
}

/******************************************************************************
 **函数名称: shm_queue_push
 **功    能: 压入队列
 **输入参数:
 **     info: 共享内存队列
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
    void *addr = (void *)shmq->info;
    shm_pool_t *pool = shmq->pool;
    shm_queue_node_t *node;
    shm_queue_info_t *info = shmq->info;

    spin_lock(&info->lock);

    /* 1. 检查队列空间 */
    if (info->num >= info->max)
    {
        spin_unlock(&info->lock);
        return -1;
    }

    /* 2. 占用空闲结点 */
    node = (shm_queue_node_t *)(addr + info->tail);

    node->flag = SHMQ_NODE_FLAG_USED;
    node->data = p - pool->page_data[0];

    info->tail = node->next;
    ++info->num;

    spin_unlock(&info->lock);

    return 0;
}

/******************************************************************************
 **函数名称: shm_queue_pop
 **功    能: 出队列
 **输入参数:
 **     info: 共享内存队列
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
    void *p, *addr = (void *)shmq->info;
    shm_queue_info_t *info = shmq->info;
    shm_queue_node_t *node;
    shm_pool_t *pool = shmq->pool;

    if (0 == info->num)
    {
        return NULL;
    }

    spin_lock(&info->lock);
    if (0 == info->num)
    {
        spin_unlock(&info->lock);
        return NULL;
    }

    node = (shm_queue_node_t *)(addr + info->head);
    if (SHMQ_NODE_FLAG_USED != node->flag)
    {
        assert(0);
    }

    p = (void *)(pool->page_data[0] + node->data);

    node->flag = SHMQ_NODE_FLAG_IDLE;
    node->data = 0;

    info->head = node->next;
    --info->num;

    spin_unlock(&info->lock);

    return p;
}

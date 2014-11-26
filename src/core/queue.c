/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: queue.c
 ** 版本号: 1.0
 ** 描  述: 队列模块
 **     1. 先进先出的一种数据结构
 ** 作  者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "queue.h"

/******************************************************************************
 **函数名称: queue_init
 **功    能: 队列初始化
 **输入参数:
 **     q: 队列
 **     max: 队列容量
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **     1. 申请内存空间
 **     2. 设为循环队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int queue_init(Queue_t *q, int max)
{
    int idx;
    Qnode_t *node;
    
    memset(q, 0, sizeof(Queue_t));

    /* 1. 申请内存空间 */
    q->base = (Qnode_t *)calloc(max, sizeof(Qnode_t));
    if (NULL == q->base)
    {
        return -1;
    }

    /* 2. 设为循环队列 */
    node = (Qnode_t *)q->base;
    for (idx=0; idx<max-1; ++idx, ++node)
    {
        node->next = node + 1;
        node->data = NULL;
    }
    
    node->next = q->base;
    node->data = NULL;
    q->head = q->tail = q->base;
    q->max = max;

    return 0;
}

/******************************************************************************
 **函数名称: queue_push
 **功    能: 入队列
 **输入参数:
 **     q: 队列
 **     addr: 数据地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     addr指向的地址必须为堆地址
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int queue_push(Queue_t *q, void *addr)
{
    Qnode_t *node = q->tail;

    /* 1. 检查合法性 */
    if (q->num >= q->max)
    {
        return -1;
    }

    /* 2. 设置数据地址 */
    node->data = addr;
    q->tail = q->tail->next;
    ++q->num;

    return 0;
}

/******************************************************************************
 **函数名称: queue_pop
 **功    能: 出队列
 **输入参数:
 **     q: 队列
 **输出参数:
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
void *queue_pop(Queue_t *q)
{
    void *data;

    if (0 == q->num)
    {
        return NULL;
    }

    data = q->head->data;
    q->head->data = NULL;
    q->head = q->head->next;
    --q->num;

    return data;
}

/******************************************************************************
 **函数名称: queue_destroy
 **功    能: 销毁队列
 **输入参数:
 **     q: 队列
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
void queue_destroy(Queue_t *q)
{
    free(q->base);

    q->base = NULL;
    q->head = NULL;
    q->tail = NULL;
    q->max = 0;
    q->num = 0;
}

/******************************************************************************
 **函数名称: lqueue_init
 **功    能: 初始化加锁队列
 **输入参数: 
 **     lq: 加锁队列
 **     max: 队列长度
 **     memsz: 内存池总空间
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化线程读写锁
 **     2. 创建队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
int lqueue_init(lqueue_t *lq, int max, size_t memsz)
{
    int ret;
    void *addr;

    /* 1. 创建队列 */
    pthread_rwlock_init(&lq->lock, NULL);

    ret = queue_init(&lq->queue, max);
    if (0 != ret)
    {
        pthread_rwlock_destroy(&lq->lock);
        return -1;
    }

    /* 2. 创建内存池 */
    pthread_rwlock_init(&lq->slab_lock, NULL);

    addr = calloc(1, memsz);
    if (NULL == addr)
    {
        pthread_rwlock_destroy(&lq->slab_lock);
        pthread_rwlock_destroy(&lq->lock);
        queue_destroy(&lq->queue);
        return -1;
    }

    lq->slab = slab_init(addr, memsz);
    if (NULL != lq->slab)
    {
        free(addr);
        pthread_rwlock_destroy(&lq->slab_lock);
        pthread_rwlock_destroy(&lq->lock);
        queue_destroy(&lq->queue);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: lqueue_push
 **功    能: 放入加锁队列
 **输入参数: 
 **     lq: 加锁队列
 **     addr: 数据地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
int lqueue_push(lqueue_t *lq, void *addr)
{
    int ret;

    pthread_rwlock_wrlock(&lq->lock);
    ret = queue_push(&lq->queue, addr);
    pthread_rwlock_unlock(&lq->lock);

    return (ret? -1: 0);
}

/******************************************************************************
 **函数名称: lqueue_pop
 **功    能: 弹出加锁队列
 **输入参数: 
 **     lq: 加锁队列
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void *lqueue_pop(lqueue_t *lq)
{
    void *addr;

    pthread_rwlock_wrlock(&lq->lock);
    addr = queue_pop(&lq->queue);
    pthread_rwlock_unlock(&lq->lock);

    return addr;
}

/******************************************************************************
 **函数名称: lqueue_trypop
 **功    能: 尝试弹出加锁队列
 **输入参数: 
 **     lq: 加锁队列
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.20 #
 ******************************************************************************/
void *lqueue_trypop(lqueue_t *lq)
{
    void *addr;

    if (pthread_rwlock_trywrlock(&lq->lock))
    {
        return NULL;
    }

    addr = queue_pop(&lq->queue);
    pthread_rwlock_unlock(&lq->lock);

    return addr;
}

/******************************************************************************
 **函数名称: lqueue_mem_alloc
 **功    能: 申请队列内存空间
 **输入参数: 
 **     lq: 加锁队列
 **     size: 空间大小
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.24 #
 ******************************************************************************/
void *lqueue_mem_alloc(lqueue_t *lq, size_t size)
{
    void *addr;

    pthread_rwlock_wrlock(&lq->slab_lock);
    addr = slab_alloc(lq->slab, size);
    pthread_rwlock_unlock(&lq->slab_lock);

    return addr;
}

/******************************************************************************
 **函数名称: lqueue_mem_dealloc
 **功    能: 释放队列内存空间
 **输入参数: 
 **     lq: 加锁队列
 **     p: 内存地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.24 #
 ******************************************************************************/
void lqueue_mem_dealloc(lqueue_t *lq, void *p)
{
    pthread_rwlock_wrlock(&lq->slab_lock);
    slab_dealloc(lq->slab, p);
    pthread_rwlock_unlock(&lq->slab_lock);
}

/******************************************************************************
 **函数名称: lqueue_destroy
 **功    能: 销毁加锁队列
 **输入参数: 
 **     lq: 加锁队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void lqueue_destroy(lqueue_t *lq)
{
    pthread_rwlock_destroy(&lq->lock);
    queue_destroy(&lq->queue);
    free(lq->slab);
}

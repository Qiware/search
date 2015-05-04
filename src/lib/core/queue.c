/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: queue.c
 ** 版本号: 1.0
 ** 描  述: 队列模块
 **     1. 先进先出的一种数据结构
 **     2. 循环无锁队列
 ** 作  者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
#include "queue.h"
#include "syscall.h"

/******************************************************************************
 **函数名称: ring_queue_creat
 **功    能: 环形队列初始化
 **输入参数:
 **     max: 队列容量
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: max必须为2的n次方
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
ring_queue_t *ring_queue_creat(int max)
{
    ring_queue_t *rq;

    /* > max必须为2的n次方 */
    if (!ISPOWEROF2(max)) { return NULL; }

    /* > 申请内存空间 */
    rq = (ring_queue_t *)calloc(1, sizeof(ring_queue_t));
    if (NULL == rq)
    {
        return NULL;
    }

    rq->data = (void **)calloc(max, sizeof(void *));
    if (NULL == rq->data)
    {
        FREE(rq);
        return NULL;
    }

    /* > 设置相关标志 */
    rq->max = max;
    rq->mask = max - 1;
    rq->prod.head = rq->prod.tail = 0;
    rq->cons.head = rq->cons.tail = 0;

    return rq;
}

/******************************************************************************
 **函数名称: ring_queue_push
 **功    能: 入队列
 **输入参数:
 **     rq: 环形队列
 **     addr: 数据地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: addr指向的地址必须为堆地址
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int ring_queue_push(ring_queue_t *rq, void *addr)
{
    int succ;
    unsigned int prod_head, prod_next, cons_tail;

    /* > 申请队列空间 */
    do
    {
        prod_head = rq->prod.head;
        cons_tail = rq->cons.tail;
        if (1 > (rq->mask + cons_tail - prod_head))
        {
            return -1; /* 空间不足 */
        }

        prod_next = prod_head + 1;

        succ = atomic32_cmpset(&rq->prod.head, prod_head, prod_next);
    } while (0 == succ);

    /* > 放入队列空间 */
    rq->data[prod_head & rq->mask] = addr;
    while (rq->prod.tail != prod_head) { NULL; }
    rq->prod.tail = prod_next;

    atomic32_inc(&rq->num); /* 计数 */

    return 0;
}

/******************************************************************************
 **函数名称: ring_queue_pop
 **功    能: 出队列
 **输入参数:
 **     rq: 队列
 **输出参数:
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
void *ring_queue_pop(ring_queue_t *rq)
{
    void *data;
    int succ;
    unsigned int cons_head, cons_next, prod_tail;

    do
    {
        cons_head = rq->cons.head;
        prod_tail = rq->prod.tail;

        if (1 > prod_tail - cons_head)
        {
            return NULL; /* 无数据 */
        }

        cons_next = cons_head + 1;

        succ = atomic32_cmpset(&rq->cons.head, cons_head, cons_next);
    } while(0 == succ);

    data = rq->data[cons_head];

    while (rq->cons.tail != cons_head) { NULL; }

    rq->cons.tail = cons_next;

    atomic32_dec(&rq->num); /* 计数 */

    return data;
}

/******************************************************************************
 **函数名称: ring_queue_destroy
 **功    能: 销毁环形队列
 **输入参数:
 **     rq: 环形队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 释放环形队列空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
void ring_queue_destroy(ring_queue_t *rq)
{
    FREE(rq->data);
    rq->max = 0;
    rq->num = 0;
}

/******************************************************************************
 **函数名称: queue_creat
 **功    能: 初始化加锁队列
 **输入参数: 
 **     max: 队列长度(必须为2的次方)
 **     size: 内存单元SIZE
 **输出参数: NONE
 **返    回: 加锁队列对象
 **实现描述: 
 **注意事项: 内存池中的块数必须与队列长度一致, 否则可能出现PUSH失败的情况出现!
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
queue_t *queue_creat(int max, int size)
{
    queue_t *queue;
    chunk_t *chunk;

    /* > 新建对象 */
    queue = (queue_t *)calloc(1, sizeof(queue_t));
    if (NULL == queue)
    {
        return NULL;
    }

    /* > 创建队列 */
    queue->ring = ring_queue_creat(max);
    if (NULL == queue->ring)
    {
        FREE(queue);
        return NULL;
    }

    /* > 创建内存池 */
    chunk = chunk_creat(max, size);
    if (NULL == chunk)
    {
        FREE(queue);
        ring_queue_destroy(queue->ring);
        return NULL;
    }

    queue->chunk = chunk;

    return queue;
}

/******************************************************************************
 **函数名称: queue_destroy
 **功    能: 销毁加锁队列
 **输入参数: 
 **     queue: 队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void queue_destroy(queue_t *queue)
{
    ring_queue_destroy(queue->ring);
    chunk_destroy(queue->chunk);
    free(queue);
}

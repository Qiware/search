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
    q->head = q->head->next;
    --q->num;

    return data;
}

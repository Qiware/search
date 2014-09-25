/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: queue.c
 ** 版本号: 1.0
 ** 描  述: 队列模块
 **         1. 先进先出的一种数据结构
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
 **      q: 队列
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int queue_init(Queue_t *q, int max, int size)
{
    int idx;
    void *data;
    Qnode_t *node;
    
    memset(q, 0, sizeof(Queue_t));

    /* 1. Alloc memory for queue */
    q->base = (Qnode_t *)calloc(max, sizeof(Qnode_t));
    if (NULL == q->base)
    {
        return -1;
    }

    data = (void *)calloc(max, size*sizeof(char));
    if (NULL == data)
    {
        free(q->base), q->base = NULL;
        return -1;
    }

    /* 2. Set circular point32_ter */
    node = (Qnode_t *)q->base;
    for (idx=0; idx<max-1; ++idx, ++node)
    {
        node->next = node + 1;
        node->data = data + idx*size;
    }
    
    node->next = q->base;
    node->data = data + idx*size;
    q->head = q->tail = q->base;
    q->max = max;
    q->size = size;

    return 0;
}

/******************************************************************************
 **函数名称: queue_push
 **功    能: 入队列
 **输入参数:
 **      q: 队列
 **     addr: 数据地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int queue_push(Queue_t *q, void *addr, int size)
{
    Qnode_t *node = q->tail;

    /* 1. Check valid attribute */
    if (size > q->size)
    {
        return -1;
    }
    else if (q->num >= q->max)
    {
        return -1;
    }

    /* 2. Check valid attribute */
    memcpy(node->data, addr, size);

    q->tail = q->tail->next;
    ++q->num;

    return 0;
}

/******************************************************************************
 **函数名称: queue_pop
 **功    能: 出队列
 **输入参数:
 **      q: 队列
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

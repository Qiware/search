/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: queue.c
 ** 版本号: 1.0
 ** 描  述: 队列模块
 **     1. 先进先出的一种数据结构
 ** 作  者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
#include "queue.h"

/******************************************************************************
 **函数名称: _queue_creat
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
int _queue_creat(_queue_t *q, int max)
{
    int idx;
    queue_node_t *node;
    
    memset(q, 0, sizeof(_queue_t));

    /* 1. 申请内存空间 */
    q->base = (queue_node_t *)calloc(max, sizeof(queue_node_t));
    if (NULL == q->base)
    {
        return -1;
    }

    /* 2. 设为循环队列 */
    node = (queue_node_t *)q->base;
    for (idx=0; idx<max-1; ++idx, ++node)
    {
        node->next = node + 1;
        node->data = NULL;
    }
    
    node->next = q->base;
    node->data = NULL;
    q->head = q->tail = q->base;
    q->max = max;

    spin_lock_init(&q->lock);

    return 0;
}

/******************************************************************************
 **函数名称: queue_push_lock
 **功    能: 入队列
 **输入参数:
 **     q: 队列
 **     addr: 数据地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: addr指向的地址必须为堆地址
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int queue_push_lock(_queue_t *q, void *addr)
{
    queue_node_t *node = q->tail;

    spin_lock(&q->lock);

    /* 1. 检查合法性 */
    if (q->num >= q->max)
    {
        spin_unlock(&q->lock);
        return -1;
    }

    /* 2. 设置数据地址 */
    node->data = addr;
    q->tail = q->tail->next;
    ++q->num;

    spin_unlock(&q->lock);

    return 0;
}

/******************************************************************************
 **函数名称: queue_pop_lock
 **功    能: 出队列
 **输入参数:
 **     q: 队列
 **输出参数:
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
void *queue_pop_lock(_queue_t *q)
{
    void *data;

    if (0 == q->num)
    {
        return NULL;
    }

    spin_lock(&q->lock);

    if (0 == q->num)
    {
        spin_unlock(&q->lock);
        return NULL;
    }

    data = q->head->data;
    q->head->data = NULL;
    q->head = q->head->next;
    --q->num;

    spin_unlock(&q->lock);

    return data;
}

/******************************************************************************
 **函数名称: _queue_destroy
 **功    能: 销毁队列
 **输入参数:
 **     q: 队列
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
void _queue_destroy(_queue_t *q)
{
    spin_lock(&q->lock);

    free(q->base);

    q->base = NULL;
    q->head = NULL;
    q->tail = NULL;
    q->max = 0;
    q->num = 0;

    spin_unlock(&q->lock);

    spin_lock_destroy(&q->lock);
}

/******************************************************************************
 **函数名称: queue_creat
 **功    能: 初始化加锁队列
 **输入参数: 
 **     max: 队列长度
 **     size: 内存单元SIZE
 **输出参数: NONE
 **返    回: 加锁队列对象
 **实现描述: 
 **注意事项: 内存池中的块数必须与队列长度一致, 否则可能出现PUSH失败的情况出现!
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
queue_t *queue_creat(int max, int size)
{
    queue_t *q;
    mem_blk_t *chunk;

    /* 1. 新建对象 */
    q = (queue_t *)calloc(1, sizeof(queue_t));
    if (NULL == q)
    {
        return NULL;
    }

    /* 2. 创建内存池 */
    chunk = mem_blk_creat(max, size);
    if (NULL == chunk)
    {
        free(q);
        return NULL;
    }

    q->chunk = chunk;

    /* 3. 创建队列 */
    if (_queue_creat(&q->queue, max))
    {
        free(q);
        mem_blk_destroy(chunk);
        return NULL;
    }

    q->queue.size = size;

    return q;
}

/******************************************************************************
 **函数名称: queue_destroy
 **功    能: 销毁加锁队列
 **输入参数: 
 **     q: 加锁队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void queue_destroy(queue_t *q)
{
    _queue_destroy(&q->queue);
    mem_blk_destroy(q->chunk);
    free(q);
}

/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: queue.c
 ** 版本号: 1.0
 ** 描  述: 队列模块
 **     1. 先进先出的一种数据结构
 **     2. 循环无锁队列
 ** 作  者: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/

#include "redo.h"
#include "queue.h"

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

    /* > 新建对象 */
    queue = (queue_t *)calloc(1, sizeof(queue_t));
    if (NULL == queue) {
        return NULL;
    }

    /* > 创建队列 */
    queue->ring = ring_creat(max);
    if (NULL == queue->ring) {
        FREE(queue);
        return NULL;
    }

    /* > 创建内存池 */
    queue->slot = slot_creat(max, size);
    if (NULL == queue->slot) {
        ring_destroy(queue->ring);
        FREE(queue);
        return NULL;
    }

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
    ring_destroy(queue->ring);
    slot_destroy(queue->slot);
    free(queue);
}

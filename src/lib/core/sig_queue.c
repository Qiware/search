#include "sig_queue.h"

/******************************************************************************
 **函数名称: sig_queue_creat
 **功    能: 创建队列
 **输入参数:
 **     max: 单元个数
 **     size: 单元大小
 **输出参数: NONE
 **返    回: 队列对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.05 #
 ******************************************************************************/
sig_queue_t *sig_queue_creat(int max, int size)
{
    sig_queue_t *sq;

    sq = (sig_queue_t *)calloc(1, sizeof(sig_queue_t));
    if (NULL == sq) {
        return NULL;
    }

    sq->queue = queue_creat(max, size);
    if (NULL == sq->queue) {
        free(sq);
        return NULL;
    }

    pthread_cond_init(&sq->ready, NULL);
    pthread_mutex_init(&sq->lock, NULL);

    return sq;
}

/******************************************************************************
 **函数名称: sig_queue_push
 **功    能: 插入队列
 **输入参数:
 **     sq: 队列
 **     addr: 将被放入的数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.05 #
 ******************************************************************************/
int sig_queue_push(sig_queue_t *sq, void *addr)
{
    int ret;

    ret = queue_push(sq->queue, addr);

    pthread_cond_signal(&sq->ready);

    return ret;
}

/******************************************************************************
 **函数名称: sig_queue_pop
 **功    能: 弹出队列
 **输入参数:
 **     sq: 队列
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述:
 **注意事项: 由于queue_t本身就是无锁队列, 因此, 将POP操作放在锁之外.
 **作    者: # Qifeng.zou # 2015.08.05 #
 ******************************************************************************/
void *sig_queue_pop(sig_queue_t *sq)
{
    pthread_mutex_lock(&sq->lock);
    while (queue_empty(sq->queue)) {
        pthread_cond_wait(&sq->ready, &sq->lock);
    }
    pthread_mutex_unlock(&sq->lock);

    return queue_pop(sq->queue);
}

/******************************************************************************
 **函数名称: sig_queue_destroy
 **功    能: 销毁队列
 **输入参数:
 **     sq: 队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.06 #
 ******************************************************************************/
void sig_queue_destroy(sig_queue_t *sq)
{
    pthread_mutex_destroy(&sq->lock);
    pthread_cond_destroy(&sq->ready);
    queue_destroy(sq->queue);
    free(sq);
}

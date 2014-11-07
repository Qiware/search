/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: thread_pool.c
 ** 版本号: 1.0
 ** 描  述: 线程池模块的实现.
 **         通过线程池模块, 可有效的简化多线程编程的处理, 加快开发速度, 同时有
 **         效增强模块的复用性和程序的稳定性。
 ** 作  者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

#include "thread_pool.h"

static void *thread_routine(void *arg);

/******************************************************************************
 **函数名称: thread_pool_init
 **功    能: 初始化线程池
 **输入参数:
 **      num: 线程数目
 **输出参数:
 **返    回: 线程池
 **实现描述: 
 **     1. 分配线程池空间, 并初始化
 **     2. 创建指定数目的线程
 **注意事项: 
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
thread_pool_t *thread_pool_init(int num)
{
    int idx;
    thread_pool_t *tp;

    /* 1. 分配线程池空间, 并初始化 */
    tp = (thread_pool_t *)calloc(1, sizeof(thread_pool_t));
    if (NULL == tp)
    {
        return NULL;
    }

    pthread_mutex_init(&(tp->queue_lock), NULL);
    pthread_cond_init(&(tp->queue_ready), NULL);
    tp->head = NULL;
    tp->queue_size = 0;
    tp->shutdown = 0;
    if (eslab_init(&tp->eslab, 16*KB))
    {
        free(tp);
        return NULL;
    }

    tp->tid = (pthread_t *)eslab_alloc(&tp->eslab, num*sizeof(pthread_t));
    if (NULL == tp->tid)
    {
        eslab_destroy(&tp->eslab);
        free(tp);
        return NULL;
    }

    /* 2. 创建指定数目的线程 */
    for (idx=0; idx<num; idx++)
    {
        if (thread_creat(&tp->tid[idx], thread_routine, tp))
        {
            thread_pool_destroy(tp);
            return NULL;
        }
        tp->num++;
    }

    return tp;
}

/******************************************************************************
 **函数名称: thread_pool_add_worker
 **功    能: 注册处理任务(回调函数)
 **输入参数:
 **     tp: 线程池
 **     process: 回调函数
 **     arg: 回调函数的参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 新建任务结点
 **     2. 将回调函数加入工作队列
 **     3. 唤醒正在等待的线程
 **注意事项: 
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_add_worker(thread_pool_t *tp, void *(*process)(void *arg), void *arg)
{
    thread_worker_t *worker=NULL, *member=NULL;

    /* 1. 新建任务节点 */
    worker = (thread_worker_t*)eslab_alloc(&tp->eslab, sizeof(thread_worker_t));
    if (NULL == worker)
    {
        return -1;
    }

    worker->process = process;
    worker->arg = arg;
    worker->next = NULL;

    /* 2. 将回调函数加入工作队列 */
    pthread_mutex_lock(&(tp->queue_lock));

    member = tp->head;
    if (NULL != member)
    {
        while (NULL != member->next)
        {
            member = member->next;
        }
        member->next = worker;
    }
    else
    {
        tp->head = worker;
    }

    tp->queue_size++;

    pthread_mutex_unlock(&(tp->queue_lock));

    /* 3. 唤醒正在等待的线程 */
    pthread_cond_signal(&(tp->queue_ready));

    return 0;
}

/******************************************************************************
 **函数名称: thread_pool_keepalive
 **功    能: 线程保活处理
 **输入参数:
 **     tp: 线程池
 **     process: 回调函数
 **     arg: 回调函数的参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断线程是否正在运行
 **     2. 如果线程已退出, 则重新启动线程
 **注意事项: 
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_keepalive(thread_pool_t *tp)
{
    int idx;

     for (idx=0; idx<tp->num; idx++)
    {
        if (ESRCH == pthread_kill(tp->tid[idx], 0))
        {
            if (thread_creat(&tp->tid[idx], thread_routine, tp) < 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

/******************************************************************************
 **函数名称: thread_pool_keepalive_ex
 **功    能: 线程保活处理
 **输入参数:
 **     tp: 线程池
 **     process: 回调函数
 **     arg: 回调函数的参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断线程是否正在运行
 **     2. 如果线程已退出, 则重新启动线程并注册回调函数
 **注意事项: 
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_keepalive_ext(thread_pool_t *tp, void *(*process)(void *arg), void *arg)
{
    int idx;

     for (idx=0; idx<tp->num; idx++)
    {
        if (ESRCH == pthread_kill(tp->tid[idx], 0))
        {
            if (thread_creat(&tp->tid[idx], thread_routine, tp) < 0)
            {
                return -1;
            }

            thread_pool_add_worker(tp, process, arg);
        }

    }

    return 0;
}

/******************************************************************************
 **函数名称: thread_pool_get_tidx
 **功    能: 获取当前线程在线程池中的序列号
 **输入参数:
 **     tp: 线程池
 **输出参数:
 **返    回: 线程序列号
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_get_tidx(thread_pool_t *tp)
{
    int idx;
    pthread_t tid = pthread_self();

    for (idx=0; idx<tp->num; ++idx)
    {
        if (tp->tid[idx] == tid)
        {
            return idx;
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: thread_pool_destroy
 **功    能: 销毁线程池
 **输入参数:
 **     tp: 线程池
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 设置销毁标志
 **     2. 唤醒所有线程
 **     3. 等待所有线程结束
 **注意事项: 
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_destroy(thread_pool_t *tp)
{
    int idx;

    if (0 != tp->shutdown)
    {
        return -1;
    }

    /* 1. 设置销毁标志 */
    tp->shutdown = 1;

    /* 2. 唤醒所有等待的线程 */
    pthread_cond_broadcast(&(tp->queue_ready));

    /* 3. 等待线程结束 */
    idx = 0;
    while (idx < tp->num)
    {
        if (ESRCH == pthread_kill(tp->tid[idx], 0))
        {
            idx++;
            continue;
        }
        pthread_cancel(tp->tid[idx]);
        idx++;
    }

    eslab_destroy(&tp->eslab);
    pthread_mutex_destroy(&(tp->queue_lock));
    pthread_cond_destroy(&(tp->queue_ready));
    free(tp);
    
    return 0;
}

/******************************************************************************
 **函数名称: thread_pool_destroy_ex
 **功    能: 销毁线程池
 **输入参数:
 **     tp: 线程池对象
 **     _destroy: 释放tp->data的空间
 **     ctx: 其他相关参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 设置销毁标志
 **     2. 唤醒所有线程
 **     3. 杀死所有线程
 **     4. 释放参数空间
 **     5. 释放线程池对象
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int thread_pool_destroy_ex(
    thread_pool_t *tp, void (*_destroy)(void *ctx, void *args), void *ctx)
{
    int idx;

    if (0 != tp->shutdown)
    {
        return -1;
    }

    /* 1. 设置销毁标志 */
    tp->shutdown = 1;

    /* 2. 唤醒所有线程 */
    pthread_cond_broadcast(&(tp->queue_ready));

    /* 3. 杀死所有线程 */
    for (idx=0; idx < tp->num; ++idx)
    {
        pthread_cancel(tp->tid[idx]);
    }

    /* 4. 释放参数空间 */
    _destroy(ctx, tp->data);

    /* 5. 释放对象空间 */
    pthread_mutex_destroy(&(tp->queue_lock));
    pthread_cond_destroy(&(tp->queue_ready));

    eslab_destroy(&tp->eslab);
    free(tp);
    
    return 0;
}

/******************************************************************************
 **函数名称: thread_routine
 **功    能: 线程运行函数
 **输入参数:
 **     _tp: 线程池
 **输出参数:
 **返    回: VOID *
 **实现描述: 
 **     判断是否有任务: 如无, 则等待; 如有, 则处理.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
static void *thread_routine(void *_tp)
{
    thread_worker_t *worker;
    thread_pool_t *tp = (thread_pool_t*)_tp;

    while (1)
    {
        pthread_mutex_lock(&(tp->queue_lock));
        while ((0 == tp->shutdown)
            && (0 == tp->queue_size))
        {
            pthread_cond_wait(&(tp->queue_ready), &(tp->queue_lock));
        }

        if (0 != tp->shutdown)
        {
            pthread_mutex_unlock(&(tp->queue_lock));
            pthread_exit(NULL);
        }

        tp->queue_size--;
        worker = tp->head;
        tp->head = worker->next;
        pthread_mutex_unlock(&(tp->queue_lock));

        (*(worker->process))(worker->arg);

        pthread_mutex_lock(&(tp->queue_lock));
        eslab_dealloc(&tp->eslab, worker);
        pthread_mutex_unlock(&(tp->queue_lock));
    }
}

/******************************************************************************
 **函数名称: thread_creat
 **功    能: 创建线程
 **输入参数:
 **     process: 线程回调函数
 **     args: 回调函数参数
 **输出参数:
 **     tid: 线程ID
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int thread_creat(pthread_t *tid, void *(*process)(void *args), void *args)
{
    pthread_attr_t attr;

    for (;;)
    {
        /* 属性初始化 */
        if (pthread_attr_init(&attr))
        {
            break;
        }

        /* 设置为分离线程 */
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
        {
            break;
        }

        /* 设置线程栈大小 */
        if (pthread_attr_setstacksize(&attr, THREAD_ATTR_STACK_SIZE))
        {
            break;
        }

        /* 创建线程 */
        if (pthread_create(tid, &attr, process, args))
        {
            if (EINTR == errno)
            {
                pthread_attr_destroy(&attr);
                continue;
            }

            break;
        }

        pthread_attr_destroy(&attr);
        return 0;
    }

    pthread_attr_destroy(&attr);
    return -1;
}

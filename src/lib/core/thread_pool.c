/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: thread_pool.c
 ** 版本号: 1.0
 ** 描  述: 线程池模块的实现.
 **         通过线程池模块, 可有效的简化多线程编程的处理, 加快开发速度, 同时有
 **         效增强模块的复用性和程序的稳定性。
 ** 作  者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
#include "thread_pool.h"

static void *thread_routine(void *_tpool);

/******************************************************************************
 **函数名称: thread_pool_init
 **功    能: 初始化线程池
 **输入参数:
 **     num: 线程数目
 **     opt: 选项信息
 **     args: 附加参数信息
 **输出参数:
 **返    回: 线程池
 **实现描述:
 **     1. 分配线程池空间, 并初始化
 **     2. 创建指定数目的线程
 **注意事项:
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
thread_pool_t *thread_pool_init(int num, const thread_pool_opt_t *opt, void *args)
{
    int idx;
    thread_pool_t *tpool;
    thread_pool_opt_t _opt;

    if (NULL == opt) {
        memset(&_opt, 0, sizeof(_opt));
        _opt.pool = (void *)NULL;
        _opt.alloc = (mem_alloc_cb_t)mem_alloc;
        _opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;
        opt = &_opt;
    }

    /* 1. 分配线程池空间, 并初始化 */
    tpool = (thread_pool_t *)opt->alloc(opt->pool, sizeof(thread_pool_t));
    if (NULL == tpool) {
        return NULL;
    }

    pthread_mutex_init(&(tpool->queue_lock), NULL);
    pthread_cond_init(&(tpool->queue_ready), NULL);
    tpool->head = NULL;
    tpool->queue_size = 0;
    tpool->shutdown = 0;
    tpool->data = (void *)args;

    tpool->mem_pool = opt->pool;
    tpool->alloc = opt->alloc;
    tpool->dealloc = opt->dealloc;

    tpool->tid = (pthread_t *)tpool->alloc(tpool->mem_pool, num*sizeof(pthread_t));
    if (NULL == tpool->tid) {
        opt->dealloc(opt->pool, tpool);
        return NULL;
    }

    /* 2. 创建指定数目的线程 */
    for (idx=0; idx<num; ++idx) {
        if (thread_creat(&tpool->tid[idx], thread_routine, tpool)) {
            thread_pool_destroy(tpool);
            return NULL;
        }
        ++tpool->num;
    }

    return tpool;
}

/******************************************************************************
 **函数名称: thread_pool_add_worker
 **功    能: 注册处理任务(回调函数)
 **输入参数:
 **     tpool: 线程池
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
int thread_pool_add_worker(thread_pool_t *tpool, void *(*process)(void *arg), void *arg)
{
    thread_worker_t *worker, *member;

    /* 1. 新建任务节点 */
    worker = (thread_worker_t*)tpool->alloc(tpool->mem_pool, sizeof(thread_worker_t));
    if (NULL == worker) {
        return -1;
    }

    worker->process = process;
    worker->arg = arg;
    worker->next = NULL;

    /* 2. 将回调函数加入工作队列 */
    pthread_mutex_lock(&(tpool->queue_lock));

    member = tpool->head;
    if (NULL != member) {
        while (NULL != member->next) {
            member = member->next;
        }
        member->next = worker;
    }
    else {
        tpool->head = worker;
    }

    tpool->queue_size++;

    pthread_mutex_unlock(&(tpool->queue_lock));

    /* 3. 唤醒正在等待的线程 */
    pthread_cond_signal(&(tpool->queue_ready));

    return 0;
}

/******************************************************************************
 **函数名称: thread_pool_keepalive
 **功    能: 线程保活处理
 **输入参数:
 **     tpool: 线程池
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
int thread_pool_keepalive(thread_pool_t *tpool)
{
    int idx;

    for (idx=0; idx<tpool->num; idx++) {
        if (ESRCH == pthread_kill(tpool->tid[idx], 0)) {
            if (thread_creat(&tpool->tid[idx], thread_routine, tpool) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

/******************************************************************************
 **函数名称: thread_pool_get_tidx
 **功    能: 获取当前线程在线程池中的序列号
 **输入参数:
 **     tpool: 线程池
 **输出参数:
 **返    回: 线程序列号
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_get_tidx(thread_pool_t *tpool)
{
    int idx;
    pthread_t tid = pthread_self();

    for (idx=0; idx<tpool->num; ++idx) {
        if (tpool->tid[idx] == tid) {
            return idx;
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: thread_pool_destroy
 **功    能: 销毁线程池
 **输入参数:
 **     tpool: 线程池
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 设置销毁标志
 **     2. 唤醒所有线程
 **     3. 等待所有线程结束
 **注意事项:
 **作    者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_destroy(thread_pool_t *tpool)
{
    int idx;

    if (0 != tpool->shutdown) {
        return -1;
    }

    /* 1. 设置销毁标志 */
    tpool->shutdown = 1;

    /* 2. 唤醒所有等待的线程 */
    pthread_cond_broadcast(&(tpool->queue_ready));

    /* 3. 等待线程结束 */
    for (idx=0; idx<tpool->num; ++idx) {
        if (ESRCH == pthread_kill(tpool->tid[idx], 0)) {
            continue;
        }
        pthread_cancel(tpool->tid[idx]);
    }

    pthread_mutex_destroy(&(tpool->queue_lock));
    pthread_cond_destroy(&(tpool->queue_ready));
    tpool->dealloc(tpool->mem_pool, tpool);

    return 0;
}

/******************************************************************************
 **函数名称: thread_routine
 **功    能: 线程运行函数
 **输入参数:
 **     _tpool: 线程池
 **输出参数:
 **返    回: VOID *
 **实现描述:
 **     判断是否有任务: 如无, 则等待; 如有, 则处理.
 **注意事项:
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
static void *thread_routine(void *_tpool)
{
    thread_worker_t *worker;
    thread_pool_t *tpool = (thread_pool_t*)_tpool;

    while (1) {
        pthread_mutex_lock(&(tpool->queue_lock));
        while ((0 == tpool->shutdown)
            && (0 == tpool->queue_size))
        {
            pthread_cond_wait(&(tpool->queue_ready), &(tpool->queue_lock));
        }

        if (0 != tpool->shutdown) {
            pthread_mutex_unlock(&(tpool->queue_lock));
            pthread_exit(NULL);
        }

        tpool->queue_size--;
        worker = tpool->head;
        tpool->head = worker->next;
        pthread_mutex_unlock(&(tpool->queue_lock));

        (*(worker->process))(worker->arg);

        pthread_mutex_lock(&(tpool->queue_lock));
        tpool->dealloc(tpool->mem_pool, worker);
        pthread_mutex_unlock(&(tpool->queue_lock));
    }

    return NULL;
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

    for (;;) {
        /* 属性初始化 */
        if (pthread_attr_init(&attr)) {
            break;
        }

        /* 设置为分离线程 */
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
            break;
        }

        /* 设置线程栈大小 */
        if (pthread_attr_setstacksize(&attr, THREAD_ATTR_STACK_SIZE)) {
            break;
        }

        /* 创建线程 */
        if (pthread_create(tid, &attr, process, args)) {
            if (EINTR == errno) {
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

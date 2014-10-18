/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_sched.c
 ** 版本号: 1.0
 ** 描  述: 爬虫任务分配
 **         负责将REDIS任务队列中的数据分发到不同的爬虫工作队列中
 ** 作  者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
#include <stdint.h>

#include "log.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "crwl_task.h"
#include "xd_socket.h"
#include "crwl_worker.h"
#include "crwl_sched.h"

static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx);
static void crwl_sched_destroy(crwl_sched_t *sched);

static int crwl_sched_timeout_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_event_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_fetch_undo_task(crwl_cntx_t *ctx, crwl_sched_t *sched);

/******************************************************************************
 **函数名称: crwl_sched_routine
 **功    能: 运行任务调度线程
 **输入参数: 
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化调度器
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void *crwl_sched_routine(void *_ctx)
{
    int ret, max;
    struct timeval tv;
    crwl_sched_t *sched;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;

    /* 1. 初始化调度器 */
    sched = crwl_sched_init(ctx);
    if (NULL == sched)
    {
        log_error(ctx->log, "Create schedule failed!");
        pthread_exit((void *)-1);
        return (void *)CRWL_ERR;
    }

    while (1)
    {
        /* 2. 等待事件通知 */
        FD_ZERO(&sched->rdset);
        FD_ZERO(&sched->wrset);

        max = sched->cmd_sck_id;

        tv.tv_sec = 0;
        tv.tv_usec = 50000;

        ret = select(max+1, &sched->rdset, &sched->wrset, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            /* 超时处理 */
            crwl_sched_timeout_hdl(ctx, sched);
            continue;
        }

        /* 3. 进行事件处理 */
        crwl_sched_event_hdl(ctx, sched);
    }

    crwl_sched_destroy(sched);

    return (void *)CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_sched_init
 **功    能: 初始化调度对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 调队对象
 **实现描述: 
 **     1. 初始化调度器
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx)
{
    struct timeval tv;
    crwl_sched_t *sched;
    crwl_conf_t *conf = &ctx->conf;

    /* 1. 创建调度器对象 */
    sched = (crwl_sched_t *)calloc(1, sizeof(crwl_sched_t));
    if (NULL == sched)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 连接Redis服务 */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    sched->redis_ctx = redisConnectWithTimeout(conf->redis.ipaddr, conf->redis.port, tv);
    if (NULL == sched->redis_ctx)
    {
        free(sched);
        log_error(ctx->log, "Connect redis failed! IP:[%s:%d]",
                conf->redis.ipaddr, conf->redis.port);
        return NULL;
    }

    /* 3. 创建命令套接字 */
    sched->cmd_sck_id = -1;

    return sched;
}

/******************************************************************************
 **函数名称: crwl_sched_destroy
 **功    能: 销毁调度对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
static void crwl_sched_destroy(crwl_sched_t *sched)
{
    redisFree(sched->redis_ctx);
    Close(sched->cmd_sck_id);
    free(sched);
}

/******************************************************************************
 **函数名称: crwl_sched_timeout_hdl
 **功    能: 超时处理
 **输入参数: 
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_timeout_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int ret;

    /* 1. 取Undo任务, 并放入Worker队列 */
    ret = crwl_sched_fetch_undo_task(ctx, sched);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Fetch task failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_event_hdl
 **功    能: 时间处理
 **输入参数: 
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_event_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int ret;    

    ret = crwl_sched_fetch_undo_task(ctx, sched);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Fetch task failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_fetch_undo_task
 **功    能: 从UNDO队列中取数据，并放入到Worker队列中
 **输入参数: 
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 选空闲Worker队列
 **     2. 取Undo任务数据
 **     3. 新建crwl_task_t对象
 **     4. 放入Worker任务队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_fetch_undo_task(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int ret, times;
    void *addr;
    redisReply *r;
    crwl_worker_t *worker;
    crwl_conf_t *conf = &ctx->conf;

    crwl_task_t *task;
    crwl_task_down_webpage_by_uri_t *dw;
    size_t size = sizeof(crwl_task_t) + sizeof(crwl_task_space_u);

    worker = (crwl_worker_t *)ctx->workers->data;

    times = 0;
    while (1)
    {
        /* 1. 选空闲Worker队列 */
        ++sched->last_idx;
        sched->last_idx %= conf->worker.thread_num;

        if (!crwl_worker_undo_taskq_space(worker + sched->last_idx))
        {
            ++times;
            if (times >= conf->worker.thread_num)
            {
                times = 0;
                log_warn(ctx->log, "Undo task queue space isn't enough!");
                return CRWL_OK;
            }
            continue;
        }

        times = 0;

        /* 2. 取Undo任务数据 */
        r = redisCommand(sched->redis_ctx, "LPOP %s", conf->redis.undo_taskq);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            return CRWL_OK;
        }

        log_trace(ctx->log, "[%02d] URL:%s!", sched->last_idx, r->str);

        /* 3. 新建crwl_task_t对象 */
        addr = crwl_slab_alloc(ctx, size);
        if (NULL == addr)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Alloc memory from slab failed!");
            return CRWL_OK;
        }

        task = (crwl_task_t *)addr;
        dw = (crwl_task_down_webpage_by_uri_t *)(addr + sizeof(crwl_task_t));

        task->type = CRWL_TASK_DOWN_WEBPAGE_BY_URL;
        task->length = sizeof(crwl_task_t) + sizeof(crwl_task_down_webpage_by_uri_t);

        snprintf(dw->uri, sizeof(dw->uri), "%s", r->str);
        dw->port = CRWL_WEB_SVR_PORT;

        /* 4. 放入Worker任务队列 */
        ret = crwl_task_queue_push(&worker[sched->last_idx].undo_taskq, addr);
        if (CRWL_OK != ret)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Push into worker queue failed! uri:%s port:%d",
                    dw->uri, dw->port);
            return CRWL_OK;
        }

        freeReplyObject(r);
    }

    return CRWL_OK;
}

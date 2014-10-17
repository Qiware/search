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
    int ret, idx, times = 0;
    void *addr;
    redisReply *r;
    crwl_sched_t *sched;
    crwl_worker_t *worker;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;
    crwl_conf_t *conf = &ctx->conf;
    char cmd[CMD_LINE_MAX_LEN];
    crwl_task_t *task;
    crwl_task_down_webpage_by_uri_t *lw_uri;
    size_t size = sizeof(crwl_task_t) + sizeof(crwl_task_space_u);

    worker = (crwl_worker_t *)ctx->workers->data;

    /* 1. 初始化调度器 */
    sched = crwl_sched_init(ctx);
    if (NULL == sched)
    {
        log_error(ctx->log, "Create schedule failed!");
        pthread_exit((void *)-1);
        return (void *)CRWL_ERR;
    }

    idx = 0;
    while (1)
    {
        ++idx;
        idx = idx % conf->worker.thread_num;

        if (worker[idx].task.queue.num >= worker[idx].task.queue.max)
        {
            ++times;
            if (times >= conf->worker.thread_num)
            {
                times = 0;
                usleep(500);
            }
            log_error(ctx->log, "Queue space isn't enough! idx:%d", idx);
            continue;
        }

        times = 0;

       /* 2. 从队列取数据 */
        snprintf(cmd, sizeof(cmd), "LPOP CRWL_REDIS_TASK_QUEUE");

        r = redisCommand(sched->redis_ctx, cmd);
        if (REDIS_REPLY_NIL == r->type)
        {
            --idx;
            usleep(500);
            continue;
        }

        log_debug(ctx->log, "URL:%s! idx:%d", r->str);

        /* 3. 新建crwl_task_t对象 */
        addr = crwl_slab_alloc(ctx, size);
        if (NULL == addr)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Alloc memory from slab failed!");
            break;
        }

        task = (crwl_task_t *)addr;
        lw_uri = (crwl_task_down_webpage_by_uri_t *)(addr + sizeof(crwl_task_t));

        task->type = CRWL_TASK_DOWN_WEBPAGE_BY_URL;
        task->length = sizeof(crwl_task_t) + sizeof(crwl_task_down_webpage_by_uri_t);

        snprintf(lw_uri->uri, sizeof(lw_uri->uri), "%s", r->str);
        lw_uri->port = CRWL_WEB_SVR_PORT;

        /* 4. 放入Worker任务队列 */
        ret = crwl_task_queue_push(&worker[idx].task, addr);
        if (CRWL_OK != ret)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Push into worker queue failed! uri:%s port:%d",
                    lw_uri->uri, lw_uri->port);
            continue;
        }

        freeReplyObject(r);
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

    sched = (crwl_sched_t *)calloc(1, sizeof(crwl_sched_t));
    if (NULL == sched)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    tv.tv_sec = 30;
    tv.tv_usec = 0;
    sched->redis_ctx = redisConnectWithTimeout("127.0.0.1", 6379, tv);
    if (NULL == sched->redis_ctx)
    {
        free(sched);
        log_error(ctx->log, "Connect redis server failed!");
        return NULL;
    }

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
    free(sched);
}

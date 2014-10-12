#include <netdb.h>

#include "log.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "crwl_task.h"
#include "xdo_socket.h"
#include "crwl_worker.h"
#include "crwl_sched.h"


static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx);

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
    crwl_sched_t *sched;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;

    sched = crwl_sched_init(ctx);
    if (NULL == sched)
    {
        log_error(ctx->log, "Create schedule failed!");
        pthread_exit((void *)-1);
        return (void *)CRWL_ERR;
    }

    while (1)
    {
    }

    return (void *)CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_sched_init
 **功    能: 初始化调度线程
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
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

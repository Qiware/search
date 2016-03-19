/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: logsvr.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年03月19日 星期六 19时56分51秒 #
 ******************************************************************************/
#include "log.h"
#include "redo.h"

static void *log_sync_proc(void *_ctx);
static int _log_sync_proc(log_cycle_t *log, void *args);

/******************************************************************************
 **函数名称: log_init
 **功    能: 初始化日志服务
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 日志服务
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.03.19 18:06:45 #
 ******************************************************************************/
log_cntx_t *log_init(void)
{
    log_cntx_t *ctx;

    /* > 创建日志服务对象 */
    ctx = (log_cntx_t *)calloc(1, sizeof(log_cntx_t));
    if (NULL == ctx) {
        return NULL;
    }

    ctx->timeout = 1;
    pthread_mutex_init(&ctx->lock, NULL);

    ctx->logs = avl_creat(NULL, (key_cb_t)key_cb_ptr, (cmp_cb_t)cmp_cb_ptr);
    if (NULL == ctx->logs) {
        free(ctx);
        return NULL;
    }

    ctx->tp = thread_pool_init(1, NULL, (void *)ctx);
    if (NULL == ctx->tp) {
        avl_destroy(ctx->logs, mem_dealloc, NULL);
        free(ctx);
        return NULL;
    }

    /* > 执行同步操作 */
    thread_pool_add_worker(ctx->tp, log_sync_proc, (void *)ctx);

    return ctx;
}

/******************************************************************************
 **函数名称: log_sync_proc
 **功    能: 同步日志
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static void *log_sync_proc(void *_ctx)
{
    log_cntx_t *ctx = (log_cntx_t *)_ctx;

    while (1) {
        pthread_mutex_lock(&ctx->lock);
        avl_trav(ctx->logs, (trav_cb_t)_log_sync_proc, NULL);
        pthread_mutex_unlock(&ctx->lock);
        Sleep(ctx->timeout);
    }
    return (void *)NULL;
}

/******************************************************************************
 **函数名称: _log_sync_proc
 **功    能: 同步日志
 **输入参数:
 **     log: 日志对象
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.03.19 #
 ******************************************************************************/
static int _log_sync_proc(log_cycle_t *log, void *args)
{
    return log_sync(log);
}

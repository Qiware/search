/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp_send.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Tue 19 May 2015 06:18:11 PM CST #
 ******************************************************************************/
#include "syscall.h"
#include "sdtp_send.h"

/******************************************************************************
 **函数名称: sdtp_send_creat_worktp
 **功    能: 创建工作线程线程池
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int sdtp_send_creat_worktp(sdtp_sctx_t *ctx)
{
    int idx;
    sdtp_worker_t *worker;
    thread_pool_opt_t opt;
    sdtp_ssvr_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    worker = (sdtp_worker_t *)calloc(conf->work_thd_num, sizeof(sdtp_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->worktp = thread_pool_init(conf->work_thd_num, &opt, (void *)worker);
    if (NULL == ctx->worktp)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        FREE(worker);
        return SDTP_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        if (sdtp_swrk_init(ctx, worker+idx, idx))
        {
            log_fatal(ctx->log, "Initialize work thread failed!");
            FREE(worker);
            thread_pool_destroy(ctx->worktp);
            return SDTP_ERR;
        }
    }

    /* > 注册线程回调 */
    for (idx=0; idx<conf->work_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->worktp, sdtp_swrk_routine, ctx);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_send_creat_sendtp
 **功    能: 创建发送线程线程池
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int sdtp_send_creat_sendtp(sdtp_sctx_t *ctx)
{
    int idx;
    sdtp_ssvr_t *ssvr;
    thread_pool_opt_t opt;
    sdtp_ssvr_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    ssvr = (sdtp_ssvr_t *)calloc(conf->send_thd_num, sizeof(sdtp_ssvr_t));
    if (NULL == ssvr)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->sendtp = thread_pool_init(conf->send_thd_num, &opt, (void *)ssvr);
    if (NULL == ctx->sendtp)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        FREE(ssvr);
        return SDTP_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        if (sdtp_ssvr_init(ctx, ssvr+idx, idx))
        {
            log_fatal(ctx->log, "Initialize send thread failed!");
            FREE(ssvr);
            thread_pool_destroy(ctx->sendtp);
            return SDTP_ERR;
        }
    }

    /* > 注册线程回调 */
    for (idx=0; idx<conf->send_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, sdtp_ssvr_routine, ctx);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: _sdtp_send_startup
 **功    能: 启动发送端
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建发送线程池
 **     2. 创建发送线程对象
 **     3. 设置发送线程对象
 **     4. 注册发送线程回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int _sdtp_send_startup(sdtp_sctx_t *ctx)
{
    /* > 创建内存池 */
    ctx->slab = slab_creat_by_calloc(30 * MB);
    if (NULL == ctx->slab)
    {
        log_error(ctx->log, "Initialize slab failed!");
        return SDTP_ERR;
    }

    /* > 创建工作线程池 */
    if (sdtp_send_creat_worktp(ctx))
    {
        log_error(ctx->log, "Create work thread pool failed!");
        FREE(ctx->slab);
        return SDTP_ERR;
    }

    /* > 创建发送线程池 */
    if (sdtp_send_creat_sendtp(ctx))
    {
        log_error(ctx->log, "Create send thread pool failed!");
        FREE(ctx->slab);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_send_startup
 **功    能: 启动发送端
 **输入参数: 
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建上下文对象
 **     2. 加载配置文件
 **     3. 启动各发送服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
sdtp_sctx_t *sdtp_send_startup(const sdtp_ssvr_conf_t *conf, log_cycle_t *log)
{
    sdtp_sctx_t *ctx;

    /* 1. 创建上下文对象 */
    ctx = (sdtp_sctx_t *)calloc(1, sizeof(sdtp_sctx_t));
    if (NULL == ctx)
    {
        log_fatal(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(sdtp_ssvr_conf_t));

    /* 3. 启动各发送服务 */
    if (_sdtp_send_startup(ctx))
    {
        log_fatal(log, "Startup send server failed!");
        return NULL;
    }

    return ctx;
}

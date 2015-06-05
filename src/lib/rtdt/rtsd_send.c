#include "syscall.h"
#include "rtsd_send.h"

/******************************************************************************
 **函数名称: rtsd_creat_worktp
 **功    能: 创建工作线程线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int rtsd_creat_worktp(rtsd_cntx_t *ctx)
{
    int idx;
    rtdt_worker_t *worker;
    thread_pool_opt_t opt;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    worker = (rtdt_worker_t *)slab_alloc(ctx->slab, conf->work_thd_num * sizeof(rtdt_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTDT_ERR;
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
        slab_dealloc(ctx->slab, worker);
        return RTDT_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        if (rtsd_worker_init(ctx, worker+idx, idx))
        {
            log_fatal(ctx->log, "Initialize work thread failed!");
            slab_dealloc(ctx->slab, worker);
            thread_pool_destroy(ctx->worktp);
            return RTDT_ERR;
        }
    }

    /* > 注册线程回调 */
    for (idx=0; idx<conf->work_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->worktp, rtsd_worker_routine, ctx);
    }

    return RTDT_OK;
}

/******************************************************************************
 **函数名称: rtsd_creat_sendtp
 **功    能: 创建发送线程线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int rtsd_creat_sendtp(rtsd_cntx_t *ctx)
{
    int idx;
    rtsd_ssvr_t *ssvr;
    thread_pool_opt_t opt;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    ssvr = (rtsd_ssvr_t *)slab_alloc(ctx->slab, conf->send_thd_num * sizeof(rtsd_ssvr_t));
    if (NULL == ssvr)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTDT_ERR;
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
        slab_dealloc(ctx->slab, ssvr);
        return RTDT_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        if (rtsd_ssvr_init(ctx, ssvr+idx, idx))
        {
            log_fatal(ctx->log, "Initialize send thread failed!");
            slab_dealloc(ctx->slab, ssvr);
            thread_pool_destroy(ctx->sendtp);
            return RTDT_ERR;
        }
    }

    /* > 注册线程回调 */
    for (idx=0; idx<conf->send_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, rtsd_ssvr_routine, ctx);
    }

    return RTDT_OK;
}


/******************************************************************************
 **函数名称: rtsd_creat_recvq
 **功    能: 创建接收队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
static int rtsd_creat_recvq(rtsd_cntx_t *ctx)
{
    int idx;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 创建队列对象 */
    ctx->recvq = (queue_t **)calloc(conf->send_thd_num, sizeof(queue_t *));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTDT_ERR;
    }

    /* > 创建接收队列 */
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx])
        {
            log_error(ctx->log, "Create recvq failed!");
            return RTDT_ERR;
        }
    }

    return RTDT_OK;
}

/******************************************************************************
 **函数名称: rtsd_init
 **功    能: 初始化发送端
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
rtsd_cntx_t *rtsd_init(const rtsd_conf_t *conf, log_cycle_t *log)
{
    rtsd_cntx_t *ctx;
    slab_pool_t *slab;

    /* > 创建内存池 */
    slab = slab_creat_by_calloc(30 * MB);
    if (NULL == slab)
    {
        log_error(log, "Initialize slab failed!");
        return NULL;
    }

    /* > 创建对象 */
    ctx = (rtsd_cntx_t *)slab_alloc(slab, sizeof(rtsd_cntx_t));
    if (NULL == ctx)
    {
        log_fatal(log, "errmsg:[%d] %s!", errno, strerror(errno));
        slab_destroy(slab);
        return NULL;
    }

    ctx->log = log;
    ctx->slab = slab;

    /* > 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(rtsd_conf_t));

    /* > 创建接收队列 */
    if (rtsd_creat_recvq(ctx))
    {
        log_fatal(log, "Create recv-queue failed!");
        slab_destroy(slab);
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: rtsd_start
 **功    能: 启动发送端
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 创建工作线程池
 **     2. 创建发送线程池
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
int rtsd_start(rtsd_cntx_t *ctx)
{
    /* > 创建工作线程池 */
    if (rtsd_creat_worktp(ctx))
    {
        log_fatal(ctx->log, "Create work thread pool failed!");
        return RTDT_ERR;
    }

    /* > 创建发送线程池 */
    if (rtsd_creat_sendtp(ctx))
    {
        log_fatal(ctx->log, "Create send thread pool failed!");
        return RTDT_ERR;
    }

    return RTDT_OK;
}

/******************************************************************************
 **函数名称: rtsd_register
 **功    能: 消息处理的注册接口
 **输入参数:
 **     ctx: 全局对象
 **     type: 扩展消息类型 Range:(0 ~ RTDT_TYPE_MAX)
 **     proc: 回调函数
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     1. 只能用于注册处理扩展数据类型的处理
 **     2. 不允许重复注册
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
int rtsd_register(rtsd_cntx_t *ctx, int type, rtdt_reg_cb_t proc, void *args)
{
    rtdt_reg_t *reg;

    if (type >= RTDT_TYPE_MAX)
    {
        log_error(ctx->log, "Data type [%d] is out of range!", type);
        return RTDT_ERR;
    }

    if (0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Repeat register type [%d]!", type);
        return RTDT_ERR_REPEAT_REG;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->proc = proc;
    reg->args = args;
    reg->flag = 1;

    return RTDT_OK;
}

/******************************************************************************
 **函数名称: rtsd_destroy
 **功    能: 销毁发送端
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
int rtsd_destroy(rtsd_cntx_t *ctx)
{
    slab_destroy(ctx->slab);
    return RTDT_OK;
}

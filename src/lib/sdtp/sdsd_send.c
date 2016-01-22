#include "redo.h"
#include "syscall.h"
#include "sdsd_send.h"

/******************************************************************************
 **函数名称: sdsd_creat_worktp
 **功    能: 创建工作线程线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int sdsd_creat_worktp(sdsd_cntx_t *ctx)
{
    int idx;
    sdtp_worker_t *worker;
    thread_pool_opt_t opt;
    sdsd_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    worker = (sdtp_worker_t *)calloc(1, conf->work_thd_num * sizeof(sdtp_worker_t));
    if (NULL == worker) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    ctx->worktp = thread_pool_init(conf->work_thd_num, &opt, (void *)worker);
    if (NULL == ctx->worktp) {
        log_error(ctx->log, "Initialize thread pool failed!");
        FREE(worker);
        return SDTP_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->work_thd_num; ++idx) {
        if (sdsd_worker_init(ctx, worker+idx, idx)) {
            log_fatal(ctx->log, "Initialize work thread failed!");
            FREE(worker);
            thread_pool_destroy(ctx->worktp);
            return SDTP_ERR;
        }
    }

    /* > 注册线程回调 */
    for (idx=0; idx<conf->work_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->worktp, sdsd_worker_routine, ctx);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_creat_sendtp
 **功    能: 创建发送线程线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int sdsd_creat_sendtp(sdsd_cntx_t *ctx)
{
    int idx;
    sdsd_ssvr_t *ssvr;
    thread_pool_opt_t opt;
    sdsd_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    ssvr = (sdsd_ssvr_t *)calloc(1, conf->send_thd_num * sizeof(sdsd_ssvr_t));
    if (NULL == ssvr) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    ctx->sendtp = thread_pool_init(conf->send_thd_num, &opt, (void *)ssvr);
    if (NULL == ctx->sendtp) {
        log_error(ctx->log, "Initialize thread pool failed!");
        FREE(ssvr);
        return SDTP_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->send_thd_num; ++idx) {
        if (sdsd_ssvr_init(ctx, ssvr+idx, idx)) {
            log_fatal(ctx->log, "Initialize send thread failed!");
            FREE(ssvr);
            thread_pool_destroy(ctx->sendtp);
            return SDTP_ERR;
        }
    }

    /* > 注册线程回调 */
    for (idx=0; idx<conf->send_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, sdsd_ssvr_routine, ctx);
    }

    return SDTP_OK;
}


/******************************************************************************
 **函数名称: sdsd_creat_recvq
 **功    能: 创建接收队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
static int sdsd_creat_recvq(sdsd_cntx_t *ctx)
{
    int idx;
    sdsd_conf_t *conf = &ctx->conf;

    /* > 创建队列对象 */
    ctx->recvq = (queue_t **)calloc(conf->send_thd_num, sizeof(queue_t *));
    if (NULL == ctx->recvq) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建接收队列 */
    for (idx=0; idx<conf->send_thd_num; ++idx) {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx]) {
            log_error(ctx->log, "Create recvq failed!");
            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_init
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
sdsd_cntx_t *sdsd_init(const sdsd_conf_t *conf, log_cycle_t *log)
{
    sdsd_cntx_t *ctx;

    /* > 创建对象 */
    ctx = (sdsd_cntx_t *)calloc(1, sizeof(sdsd_cntx_t));
    if (NULL == ctx) {
        log_fatal(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* > 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(sdsd_conf_t));

    /* > 创建接收队列 */
    if (sdsd_creat_recvq(ctx)) {
        log_fatal(log, "Create recv-queue failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: sdsd_launch
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
int sdsd_launch(sdsd_cntx_t *ctx)
{
    /* > 创建工作线程池 */
    if (sdsd_creat_worktp(ctx)) {
        log_fatal(ctx->log, "Create work thread pool failed!");
        return SDTP_ERR;
    }

    /* > 创建发送线程池 */
    if (sdsd_creat_sendtp(ctx)) {
        log_fatal(ctx->log, "Create send thread pool failed!");
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_register
 **功    能: 消息处理的注册接口
 **输入参数:
 **     ctx: 全局对象
 **     type: 扩展消息类型 Range:(0 ~ SDTP_TYPE_MAX)
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
int sdsd_register(sdsd_cntx_t *ctx, int type, sdtp_reg_cb_t proc, void *args)
{
    sdtp_reg_t *reg;

    if (type >= SDTP_TYPE_MAX) {
        log_error(ctx->log, "Data type [%d] is out of range!", type);
        return SDTP_ERR;
    }

    if (0 != ctx->reg[type].flag) {
        log_error(ctx->log, "Repeat register type [%d]!", type);
        return SDTP_ERR_REPEAT_REG;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->proc = proc;
    reg->args = args;
    reg->flag = 1;

    return SDTP_OK;
}

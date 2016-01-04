#include "syscall.h"
#include "rtmq_cmd.h"
#include "rtsd_send.h"

static int rtsd_creat_cmd_usck(rtsd_cntx_t *ctx);

/******************************************************************************
 **函数名称: rtsd_creat_workers
 **功    能: 创建工作线程线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int rtsd_creat_workers(rtsd_cntx_t *ctx)
{
    int idx;
    rtmq_worker_t *worker;
    thread_pool_opt_t opt;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    worker = (rtmq_worker_t *)slab_alloc(ctx->slab, conf->work_thd_num * sizeof(rtmq_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
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
        return RTMQ_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        if (rtsd_worker_init(ctx, worker+idx, idx))
        {
            log_fatal(ctx->log, "Initialize work thread failed!");
            slab_dealloc(ctx->slab, worker);
            thread_pool_destroy(ctx->worktp);
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtsd_creat_sends
 **功    能: 创建发送线程线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int rtsd_creat_sends(rtsd_cntx_t *ctx)
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
        return RTMQ_ERR;
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
        return RTMQ_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        if (rtsd_ssvr_init(ctx, ssvr+idx, idx))
        {
            log_fatal(ctx->log, "Initialize send thread failed!");
            slab_dealloc(ctx->slab, ssvr);
            thread_pool_destroy(ctx->sendtp);
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
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
        return RTMQ_ERR;
    }

    /* > 创建接收队列 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx])
        {
            log_error(ctx->log, "Create recvq failed!");
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtsd_ssvr_creat_sendq
 **功    能: 创建发送线程的发送队列
 **输入参数:
 **     ctx: 发送对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.01.01 22:32:21 #
 ******************************************************************************/
static int rtsd_creat_sendq(rtsd_cntx_t *ctx)
{
    int idx;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 创建队列对象 */
    ctx->sendq = (queue_t **)calloc(conf->send_thd_num, sizeof(queue_t *));
    if (NULL == ctx->sendq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 创建发送队列 */
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        ctx->sendq[idx] = queue_creat(conf->sendq.max, conf->sendq.size);
        if (NULL == ctx->sendq[idx])
        {
            log_error(ctx->log, "Create send queue failed!");
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
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
    slab = slab_creat_by_calloc(30 * MB, log);
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

    /* > 创建通信套接字 */
    if (rtsd_creat_cmd_usck(ctx))
    {
        log_fatal(log, "Create recv-queue failed!");
        slab_destroy(slab);
        return NULL;
    }

    /* > 创建接收队列 */
    if (rtsd_creat_recvq(ctx))
    {
        log_fatal(log, "Create recv-queue failed!");
        slab_destroy(slab);
        return NULL;
    }

    /* > 创建发送队列 */
    if (rtsd_creat_sendq(ctx))
    {
        log_fatal(log, "Create send queue failed!");
        slab_destroy(slab);
        return NULL;
    }

    /* > 创建工作线程池 */
    if (rtsd_creat_workers(ctx))
    {
        log_fatal(ctx->log, "Create work thread pool failed!");
        slab_destroy(slab);
        return NULL;
    }

    /* > 创建发送线程池 */
    if (rtsd_creat_sends(ctx))
    {
        log_fatal(ctx->log, "Create send thread pool failed!");
        slab_destroy(slab);
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: rtsd_launch
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
int rtsd_launch(rtsd_cntx_t *ctx)
{
    int idx;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 注册Worker线程回调 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        thread_pool_add_worker(ctx->worktp, rtsd_worker_routine, ctx);
    }

    /* > 注册Send线程回调 */
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        thread_pool_add_worker(ctx->sendtp, rtsd_ssvr_routine, ctx);
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtsd_register
 **功    能: 消息处理的注册接口
 **输入参数:
 **     ctx: 全局对象
 **     type: 扩展消息类型 Range:(0 ~ RTMQ_TYPE_MAX)
 **     proc: 回调函数
 **     param: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     1. 只能用于注册处理扩展数据类型的处理
 **     2. 不允许重复注册
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
int rtsd_register(rtsd_cntx_t *ctx, int type, rtmq_reg_cb_t proc, void *param)
{
    rtmq_reg_t *reg;

    if (type >= RTMQ_TYPE_MAX)
    {
        log_error(ctx->log, "Data type [%d] is out of range!", type);
        return RTMQ_ERR;
    }

    if (0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Repeat register type [%d]!", type);
        return RTMQ_ERR_REPEAT_REG;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->proc = proc;
    reg->param = param;
    reg->flag = 1;

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtsd_creat_cmd_usck
 **功    能: 创建命令套接字
 **输入参数:
 **     ctx: 上下文信息
 **     idx: 目标队列序号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtsd_creat_cmd_usck(rtsd_cntx_t *ctx)
{
    char path[FILE_NAME_MAX_LEN];

    rtsd_comm_usck_path(&ctx->conf, path);

    spin_lock_init(&ctx->cmd_sck_lck);
    ctx->cmd_sck_id = unix_udp_creat(path);
    if (ctx->cmd_sck_id < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtsd_cli_cmd_send_req
 **功    能: 通知Send服务线程
 **输入参数:
 **     ctx: 上下文信息
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtsd_cli_cmd_send_req(rtsd_cntx_t *ctx, int idx)
{
    rtmq_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];
    rtsd_conf_t *conf = &ctx->conf;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTMQ_CMD_SEND_ALL;
    rtsd_ssvr_usck_path(conf, path, idx);

    if (spin_trylock(&ctx->cmd_sck_lck))
    {
        return RTMQ_OK;
    }

    if (unix_udp_send(ctx->cmd_sck_id, path, &cmd, sizeof(cmd)) < 0)
    {
        spin_unlock(&ctx->cmd_sck_lck);
        if (EAGAIN != errno)
        {
            log_debug(ctx->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        }
        return RTMQ_ERR;
    }

    spin_unlock(&ctx->cmd_sck_lck);

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtsd_cli_send
 **功    能: 发送指定数据(对外接口)
 **输入参数:
 **     ctx: 上下文信息
 **     type: 数据类型
 **     nodeid: 源结点ID
 **     data: 数据地址
 **     size: 数据长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将数据按照约定格式放入队列中
 **注意事项:
 **     1. 只能用于发送自定义数据类型, 而不能用于系统数据类型
 **     2. 不用关注变量num在多线程中的值, 因其不影响安全性
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
int rtsd_cli_send(rtsd_cntx_t *ctx, int type, const void *data, size_t size)
{
    int idx;
    void *addr;
    rtmq_header_t *head;
    static uint8_t num = 0;
    rtsd_conf_t *conf = &ctx->conf;

    /* > 选择发送队列 */
    idx = (num++) % conf->send_thd_num;

    addr = queue_malloc(ctx->sendq[idx], sizeof(rtmq_header_t)+size);
    if (NULL == addr)
    {
        log_error(ctx->log, "Alloc from SHMQ failed! size:%d/%d",
                size+sizeof(rtmq_header_t), queue_size(ctx->sendq[idx]));
        return RTMQ_ERR;
    }

    /* > 设置发送数据 */
    head = (rtmq_header_t *)addr;

    head->type = type;
    head->nodeid = conf->nodeid;
    head->length = size;
    head->flag = RTMQ_EXP_MESG;
    head->checksum = RTMQ_CHECK_SUM;

    memcpy(head+1, data, size);

    log_debug(ctx->log, "rq:%p Head type:%d nodeid:%d length:%d flag:%d checksum:%d!",
            ctx->sendq[idx]->ring, head->type, head->nodeid, head->length, head->flag, head->checksum);

    /* > 放入发送队列 */
    if (queue_push(ctx->sendq[idx], addr))
    {
        log_error(ctx->log, "Push into shmq failed!");
        queue_dealloc(ctx->sendq[idx], addr);
        return RTMQ_ERR;
    }

    /* > 通知发送线程 */
    rtsd_cli_cmd_send_req(ctx, idx);

    return RTMQ_OK;
}

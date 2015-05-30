/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp_recv.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/

#include "log.h"
#include "shm_opt.h"
#include "syscall.h"
#include "sdtp_cmd.h"
#include "sdtp_comm.h"
#include "sdtp_recv.h"
#include "thread_pool.h"

static int _sdtp_recv_init(sdtp_rctx_t *ctx);

static int sdtp_creat_recvq(sdtp_rctx_t *ctx);
static int sdtp_creat_sendq(sdtp_rctx_t *ctx);

static int sdtp_creat_recvtp(sdtp_rctx_t *ctx);
void sdtp_recvtp_destroy(void *_ctx, void *args);

static int sdtp_creat_worktp(sdtp_rctx_t *ctx);
void sdtp_worktp_destroy(void *_ctx, void *args);

static int sdtp_proc_def_hdl(int type, char *buff, size_t len, void *args);

/******************************************************************************
 **函数名称: sdtp_recv_init
 **功    能: 初始化SDTP接收端
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述:
 **     1. 创建全局对象
 **     2. 备份配置信息
 **     3. 初始化接收端
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
sdtp_rctx_t *sdtp_recv_init(const sdtp_conf_t *conf, log_cycle_t *log)
{
    sdtp_rctx_t *ctx;

    /* > 创建全局对象 */
    ctx = (sdtp_rctx_t *)calloc(1, sizeof(sdtp_rctx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* > 备份配置信息 */
    memcpy(&ctx->conf, conf, sizeof(sdtp_conf_t));

    ctx->conf.rqnum = SDTP_WORKER_HDL_QNUM * conf->work_thd_num;

    /* > 初始化接收端 */
    if (_sdtp_recv_init(ctx))
    {
        FREE(ctx);
        log_error(ctx->log, "Initialize recv failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: sdtp_recv_startup
 **功    能: 启动SDTP接收端
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 设置接收线程回调
 **     2. 设置工作线程回调
 **     3. 创建侦听线程
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int sdtp_recv_startup(sdtp_rctx_t *ctx)
{
    int idx;
    pthread_t tid;
    thread_pool_t *tp;
    sdtp_rlsn_t *lsn = &ctx->listen;

    /* > 设置接收线程回调 */
    tp = ctx->recvtp;
    for (idx=0; idx<tp->num; ++idx)
    {
        thread_pool_add_worker(tp, sdtp_rsvr_routine, ctx);
    }

    /* > 设置工作线程回调 */
    tp = ctx->worktp;
    for (idx=0; idx<tp->num; ++idx)
    {
        thread_pool_add_worker(tp, sdtp_rwrk_routine, ctx);
    }

    /* > 创建侦听线程 */
    if (thread_creat(&lsn->tid, sdtp_rlsn_routine, ctx))
    {
        log_error(ctx->log, "Start listen failed");
        return SDTP_ERR;
    }

    /* > 创建分发线程 */
    if (thread_creat(&tid, sdtp_disp_routine, ctx))
    {
        log_error(ctx->log, "Start dispatch failed");
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_recv_register
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
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int sdtp_recv_register(sdtp_rctx_t *ctx, int type, sdtp_reg_cb_t proc, void *args)
{
    sdtp_reg_t *reg;

    if (type >= SDTP_TYPE_MAX)
    {
        log_error(ctx->log, "Data type is out of range!");
        return SDTP_ERR;
    }

    if (0 != ctx->reg[type].flag)
    {
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

/******************************************************************************
 **函数名称: sdtp_recv_destroy
 **功    能: 销毁SDTP对象
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int sdtp_recv_destroy(sdtp_rctx_t *ctx)
{
    /* > 销毁侦听线程 */
    sdtp_rlsn_destroy(&ctx->listen);

#if 0
    /* > 销毁接收线程池 */
    thread_pool_destroy_ext(ctx->recvtp, sdtp_recvtp_destroy, ctx);

    /* > 销毁工作线程池 */
    thread_pool_destroy_ext(ctx->worktp, sdtp_worktp_destroy, ctx);
#endif

    FREE(ctx);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_reg_init
 **功    能: 初始化注册对象
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_reg_init(sdtp_rctx_t *ctx)
{
    int idx;
    sdtp_reg_t *reg = &ctx->reg[0];

    for (idx=0; idx<SDTP_TYPE_MAX; ++idx, ++reg)
    {
        reg->type = idx;
        reg->proc = sdtp_proc_def_hdl;
        reg->flag = 0;
        reg->args = NULL;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: _sdtp_recv_init
 **功    能: 初始化接收对象
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 初始化注册信息
 **     2. 创建接收队列
 **     3. 创建接收线程池
 **     4. 创建工作线程池
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int _sdtp_recv_init(sdtp_rctx_t *ctx)
{
    avl_opt_t opt;

    /* > 创建SLAB内存池 */
    ctx->pool = slab_creat_by_calloc(SDTP_CTX_POOL_SIZE);
    if (NULL == ctx->pool)
    {
        log_error(ctx->log, "Initialize slab mem-pool failed!");
        return SDTP_ERR;
    }

    /* > 创构建建SCK->DEV的映射表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->sck2dev_map = avl_creat(&opt,
                (key_cb_t)avl_key_cb_int64,
                (avl_cmp_cb_t)avl_cmp_cb_int64);
    if (NULL == ctx->sck2dev_map)
    {
        log_error(ctx->log, "Create sck2dev map table failed!");
        return SDTP_ERR;
    }

    /* > 创建DEV->SCK的映射表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->dev2sck_map = avl_creat(&opt,
                (key_cb_t)avl_key_cb_int64,
                (avl_cmp_cb_t)avl_cmp_cb_int64);
    if (NULL == ctx->dev2sck_map)
    {
        log_error(ctx->log, "Create dev2sck map table failed!");
        return SDTP_ERR;
    }

    /* > 初始化注册信息 */
    sdtp_reg_init(ctx);

    /* > 创建接收队列 */
    if (sdtp_creat_recvq(ctx))
    {
        log_error(ctx->log, "Create recv queue failed!");
        return SDTP_ERR;
    }

    /* > 创建发送队列 */
    if (sdtp_creat_sendq(ctx))
    {
        log_error(ctx->log, "Create send queue failed!");
        return SDTP_ERR;
    }

    /* > 创建接收线程池 */
    if (sdtp_creat_recvtp(ctx))
    {
        log_error(ctx->log, "Create recv thread pool failed!");
        return SDTP_ERR;
    }

    /* > 创建工作线程池 */
    if (sdtp_creat_worktp(ctx))
    {
        log_error(ctx->log, "Create worker thread pool failed!");
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_creat_recvq
 **功    能: 创建接收队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 创建队列数组
 **     2. 依次创建接收队列
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_creat_recvq(sdtp_rctx_t *ctx)
{
    int idx;
    sdtp_conf_t *conf = &ctx->conf;

    /* > 创建队列数组 */
    ctx->recvq = calloc(conf->rqnum, sizeof(queue_t *));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 依次创建接收队列 */
    for(idx=0; idx<conf->rqnum; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx])
        {
            log_error(ctx->log, "Create queue failed! max:%d size:%d",
                    conf->recvq.max, conf->recvq.size);
            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_creat_shm_sendq
 **功    能: 创建SHM发送队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 通过路径生成KEY，再根据KEY创建共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.20 #
 ******************************************************************************/
static int sdtp_creat_shm_sendq(sdtp_rctx_t *ctx)
{
    key_t key;
    char path[FILE_NAME_MAX_LEN];
    sdtp_conf_t *conf = &ctx->conf;

    /* > 通过路径生成KEY */
    sdtp_sendq_shm_path(conf, path);

    key = shm_ftok(path, 0);
    if ((key_t)-1 == key)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 通过KEY创建共享内存队列 */
    ctx->shm_sendq = shm_queue_creat(key, conf->sendq.max, conf->sendq.size);
    if (NULL == ctx->shm_sendq)
    {
        log_error(ctx->log, "errmsg:[%d] %s! key:%lu max:%d size:%d",
                errno, strerror(errno), key, conf->sendq.max, conf->sendq.size);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: _sdtp_creat_sendq
 **功    能: 创建发送队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 创建队列数组
 **     2. 依次创建接收队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int _sdtp_creat_sendq(sdtp_rctx_t *ctx)
{
    int idx;
    sdtp_conf_t *conf = &ctx->conf;

    /* > 创建队列数组 */
    ctx->sendq = calloc(conf->recv_thd_num, sizeof(queue_t *));
    if (NULL == ctx->sendq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 依次创建发送队列 */
    for(idx=0; idx<conf->recv_thd_num; ++idx)
    {
        ctx->sendq[idx] = queue_creat(conf->sendq.max, conf->sendq.size);
        if (NULL == ctx->sendq[idx])
        {
            log_error(ctx->log, "Create send-queue failed! max:%d size:%d",
                    conf->sendq.max, conf->sendq.size);
            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_creat_sendq
 **功    能: 创建发送队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 通过路径生成KEY，再根据KEY创建共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.20 #
 ******************************************************************************/
static int sdtp_creat_sendq(sdtp_rctx_t *ctx)
{
    if (sdtp_creat_shm_sendq(ctx))
    {
        log_error(ctx->log, "Create shm-queue failed!");
        return SDTP_ERR;
    }

    if (_sdtp_creat_sendq(ctx))
    {
        log_error(ctx->log, "Create queue failed!");
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_creat_recvtp
 **功    能: 创建接收线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 创建线程池
 **     2. 创建接收对象
 **     3. 初始化接收对象
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int sdtp_creat_recvtp(sdtp_rctx_t *ctx)
{
    int idx;
    sdtp_rsvr_t *rsvr;
    thread_pool_opt_t opt;
    sdtp_conf_t *conf = &ctx->conf;

    memset(&opt, 0, sizeof(opt));

    /* > 创建接收对象 */
    rsvr = (sdtp_rsvr_t *)calloc(conf->recv_thd_num, sizeof(sdtp_rsvr_t));
    if (NULL == rsvr)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建线程池 */
    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->recvtp = thread_pool_init(conf->recv_thd_num, &opt, (void *)rsvr);
    if (NULL == ctx->recvtp)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        free(rsvr);
        return SDTP_ERR;
    }

    /* > 初始化接收对象 */
    for (idx=0; idx<conf->recv_thd_num; ++idx)
    {
        if (sdtp_rsvr_init(ctx, rsvr+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));

            free(rsvr);
            thread_pool_destroy(ctx->recvtp);
            ctx->recvtp = NULL;

            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_recvtp_destroy
 **功    能: 销毁接收线程池
 **输入参数:
 **     ctx: 全局对象
 **     args: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void sdtp_recvtp_destroy(void *_ctx, void *args)
{
    int idx;
    sdtp_rctx_t *ctx = (sdtp_rctx_t *)_ctx;
    sdtp_rsvr_t *rsvr = (sdtp_rsvr_t *)ctx->recvtp->data;

    for (idx=0; idx<ctx->conf.recv_thd_num; ++idx, ++rsvr)
    {
        /* > 关闭命令套接字 */
        CLOSE(rsvr->cmd_sck_id);

        /* > 关闭通信套接字 */
        sdtp_rsvr_del_all_conn_hdl(ctx, rsvr);

        slab_destroy(rsvr->pool);
    }

    FREE(ctx->recvtp->data);
    thread_pool_destroy(ctx->recvtp);

    return;
}

/******************************************************************************
 **函数名称: sdtp_creat_worktp
 **功    能: 创建工作线程池
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 创建线程池
 **     2. 创建工作对象
 **     3. 初始化工作对象
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static int sdtp_creat_worktp(sdtp_rctx_t *ctx)
{
    int idx;
    sdtp_worker_t *wrk;
    thread_pool_opt_t opt;
    sdtp_conf_t *conf = &ctx->conf;


    /* > 创建工作对象 */
    wrk = (void *)calloc(conf->work_thd_num, sizeof(sdtp_worker_t));
    if (NULL == wrk)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->worktp = thread_pool_init(conf->work_thd_num, &opt, (void *)wrk);
    if (NULL == ctx->worktp)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        free(wrk);
        return SDTP_ERR;
    }

    /* > 初始化工作对象 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        if (sdtp_rwrk_init(ctx, wrk+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            free(wrk);
            thread_pool_destroy(ctx->recvtp);
            ctx->recvtp = NULL;
            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_worktp_destroy
 **功    能: 销毁工作线程池
 **输入参数:
 **     ctx: 全局对象
 **     args: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
void sdtp_worktp_destroy(void *_ctx, void *args)
{
    int idx;
    sdtp_rctx_t *ctx = (sdtp_rctx_t *)_ctx;
    sdtp_conf_t *conf = &ctx->conf;
    sdtp_worker_t *wrk = (sdtp_worker_t *)ctx->worktp->data;

    for (idx=0; idx<conf->work_thd_num; ++idx, ++wrk)
    {
        CLOSE(wrk->cmd_sck_id);
    }

    FREE(ctx->worktp->data);
    thread_pool_destroy(ctx->recvtp);

    return;
}

/******************************************************************************
 **函数名称: sdtp_proc_def_hdl
 **功    能: 默认消息处理函数
 **输入参数:
 **     type: 消息类型
 **     buff: 消息内容
 **     len: 内容长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static int sdtp_proc_def_hdl(int type, char *buff, size_t len, void *args)
{
    return SDTP_OK;
}

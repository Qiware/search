/******************************************************************************
 ** Copyright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: rtrd_recv.c
 ** 版本号: 1.0
 ** 描  述: 实时消息队列(Real-Time Message Queue)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/

#include "log.h"
#include "redo.h"
#include "shm_opt.h"
#include "rtmq_cmd.h"
#include "rtmq_comm.h"
#include "rtrd_recv.h"
#include "thread_pool.h"

static int rtrd_reg_init(rtrd_cntx_t *ctx);

static int rtrd_creat_recvq(rtrd_cntx_t *ctx);
static int rtrd_creat_sendq(rtrd_cntx_t *ctx);
static int rtrd_creat_distq(rtrd_cntx_t *ctx);

static int rtrd_creat_recvs(rtrd_cntx_t *ctx);
void rtrd_recvs_destroy(void *_ctx, void *param);

static int rtrd_creat_workers(rtrd_cntx_t *ctx);
void rtrd_workers_destroy(void *_ctx, void *param);

static int rtrd_proc_def_hdl(int type, int orig, char *buff, size_t len, void *param);

/******************************************************************************
 **函数名称: rtrd_init
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
rtrd_cntx_t *rtrd_init(const rtrd_conf_t *cf, log_cycle_t *log)
{
    rtrd_cntx_t *ctx;
    slab_pool_t *slab;
    rtrd_conf_t *conf;

    /* > 创建SLAB内存池 */
    slab = slab_creat_by_calloc(RTMQ_CTX_POOL_SIZE, log);
    if (NULL == slab)
    {
        log_error(log, "Initialize slab mem-pool failed!");
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (rtrd_cntx_t *)slab_alloc(slab, sizeof(rtrd_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        free(slab);
        return NULL;
    }

    ctx->log = log;
    ctx->pool = slab;
    conf = &ctx->conf;
    memcpy(conf, cf, sizeof(rtrd_conf_t));  /* 配置信息 */
    conf->recvq_num = RTMQ_WORKER_HDL_QNUM * cf->work_thd_num;

    do
    {
        /* > 构建NODE->SVR映射表 */
        if (rtrd_node_to_svr_map_init(ctx))
        {
            log_error(ctx->log, "Initialize sck-dev map table failed!");
            break;
        }

        /* > 初始化注册信息 */
        rtrd_reg_init(ctx);

        /* > 创建接收队列 */
        if (rtrd_creat_recvq(ctx))
        {
            log_error(ctx->log, "Create recv queue failed!");
            break;
        }

        /* > 创建发送队列 */
        if (rtrd_creat_sendq(ctx))
        {
            log_error(ctx->log, "Create send queue failed!");
            break;
        }

        /* > 创建分发队列 */
        if (rtrd_creat_distq(ctx))
        {
            log_error(ctx->log, "Create send queue failed!");
            break;
        }

        /* > 创建接收线程池 */
        if (rtrd_creat_recvs(ctx))
        {
            log_error(ctx->log, "Create recv thread pool failed!");
            break;
        }

        /* > 创建工作线程池 */
        if (rtrd_creat_workers(ctx))
        {
            log_error(ctx->log, "Create worker thread pool failed!");
            break;
        }

        /* > 初始化侦听服务 */
        if (rtrd_lsn_init(ctx))
        {
            log_error(ctx->log, "Create worker thread pool failed!");
            break;
        }

        return ctx;
    } while(0);

    free(slab);
    return NULL;
}

/******************************************************************************
 **函数名称: rtrd_startup
 **功    能: 启动SDTP接收端
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int rtrd_startup(rtrd_cntx_t *ctx)
{
    int idx;
    pthread_t tid;
    thread_pool_t *tp;
    rtrd_listen_t *lsn = &ctx->listen;

    /* > 设置接收线程回调 */
    tp = ctx->recvtp;
    for (idx=0; idx<tp->num; ++idx)
    {
        thread_pool_add_worker(tp, rtrd_rsvr_routine, ctx);
    }

    /* > 设置工作线程回调 */
    tp = ctx->worktp;
    for (idx=0; idx<tp->num; ++idx)
    {
        thread_pool_add_worker(tp, rtrd_worker_routine, ctx);
    }

    /* > 创建侦听线程 */
    if (thread_creat(&lsn->tid, rtrd_lsn_routine, ctx))
    {
        log_error(ctx->log, "Start listen failed");
        return RTMQ_ERR;
    }

    /* > 创建分发线程 */
    if (thread_creat(&tid, rtrd_dsvr_routine, ctx))
    {
        log_error(ctx->log, "Start distribute thread failed");
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtrd_register
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
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int rtrd_register(rtrd_cntx_t *ctx, int type, rtmq_reg_cb_t proc, void *param)
{
    rtmq_reg_t *reg;

    if (type >= RTMQ_TYPE_MAX)
    {
        log_error(ctx->log, "Data type is out of range!");
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
 **函数名称: rtrd_reg_init
 **功    能: 初始化注册对象
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int rtrd_reg_init(rtrd_cntx_t *ctx)
{
    int idx;
    rtmq_reg_t *reg = &ctx->reg[0];

    for (idx=0; idx<RTMQ_TYPE_MAX; ++idx, ++reg)
    {
        reg->type = idx;
        reg->proc = rtrd_proc_def_hdl;
        reg->flag = 0;
        reg->param = NULL;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtrd_creat_recvq
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
static int rtrd_creat_recvq(rtrd_cntx_t *ctx)
{
    int idx;
    rtrd_conf_t *conf = &ctx->conf;

    /* > 创建队列数组 */
    ctx->recvq = calloc(conf->recvq_num, sizeof(queue_t *));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 依次创建接收队列 */
    for(idx=0; idx<conf->recvq_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx])
        {
            log_error(ctx->log, "Create queue failed! max:%d size:%d",
                    conf->recvq.max, conf->recvq.size);
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtrd_creat_sendq
 **功    能: 创建发送队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int rtrd_creat_sendq(rtrd_cntx_t *ctx)
{
    int idx;
    rtrd_conf_t *conf = &ctx->conf;

    /* > 创建队列数组 */
    ctx->sendq = slab_alloc(ctx->pool, conf->recv_thd_num*sizeof(queue_t *));
    if (NULL == ctx->sendq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 依次创建发送队列 */
    for(idx=0; idx<conf->recv_thd_num; ++idx)
    {
        ctx->sendq[idx] = queue_creat(conf->sendq.max, conf->sendq.size);
        if (NULL == ctx->sendq[idx])
        {
            log_error(ctx->log, "Create send-queue failed! max:%d size:%d",
                    conf->sendq.max, conf->sendq.size);
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtrd_creat_distq
 **功    能: 创建分发队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015-07-06 11:21:28 #
 ******************************************************************************/
static int rtrd_creat_distq(rtrd_cntx_t *ctx)
{
    int idx;
    rtrd_conf_t *conf = &ctx->conf;

    /* > 申请对象空间 */
    ctx->distq = (shm_queue_t **)slab_alloc(ctx->pool, conf->distq_num*sizeof(shm_queue_t *));
    if (NULL == ctx->distq)
    {
        log_error(ctx->log, "Alloc memory from slab failed!");
        return RTMQ_ERR;
    }

    /* > 依次创建队列 */
    for (idx=0; idx<conf->distq_num; ++idx)
    {
        ctx->distq[idx] = rtrd_shm_distq_creat(conf, idx);
        if (NULL == ctx->distq[idx])
        {
            log_error(ctx->log, "Create shm-queue failed!");
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}



/******************************************************************************
 **函数名称: rtrd_creat_recvs
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
static int rtrd_creat_recvs(rtrd_cntx_t *ctx)
{
    int idx;
    rtrd_rsvr_t *rsvr;
    thread_pool_opt_t opt;
    rtrd_conf_t *conf = &ctx->conf;

    memset(&opt, 0, sizeof(opt));

    /* > 创建接收对象 */
    rsvr = (rtrd_rsvr_t *)calloc(conf->recv_thd_num, sizeof(rtrd_rsvr_t));
    if (NULL == rsvr)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
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
        return RTMQ_ERR;
    }

    /* > 初始化接收对象 */
    for (idx=0; idx<conf->recv_thd_num; ++idx)
    {
        if (rtrd_rsvr_init(ctx, rsvr+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));

            free(rsvr);
            thread_pool_destroy(ctx->recvtp);
            ctx->recvtp = NULL;

            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtrd_recvs_destroy
 **功    能: 销毁接收线程池
 **输入参数:
 **     ctx: 全局对象
 **     param: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void rtrd_recvs_destroy(void *_ctx, void *param)
{
    int idx;
    rtrd_cntx_t *ctx = (rtrd_cntx_t *)_ctx;
    rtrd_rsvr_t *rsvr = (rtrd_rsvr_t *)ctx->recvtp->data;

    for (idx=0; idx<ctx->conf.recv_thd_num; ++idx, ++rsvr)
    {
        /* > 关闭命令套接字 */
        CLOSE(rsvr->cmd_sck_id);

        /* > 关闭通信套接字 */
        rtrd_rsvr_del_all_conn_hdl(ctx, rsvr);

        slab_destroy(rsvr->pool);
    }

    FREE(ctx->recvtp->data);
    thread_pool_destroy(ctx->recvtp);

    return;
}

/******************************************************************************
 **函数名称: rtrd_creat_workers
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
static int rtrd_creat_workers(rtrd_cntx_t *ctx)
{
    int idx;
    rtmq_worker_t *wrk;
    thread_pool_opt_t opt;
    rtrd_conf_t *conf = &ctx->conf;

    /* > 创建工作对象 */
    wrk = (void *)calloc(conf->work_thd_num, sizeof(rtmq_worker_t));
    if (NULL == wrk)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
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
        return RTMQ_ERR;
    }

    /* > 初始化工作对象 */
    for (idx=0; idx<conf->work_thd_num; ++idx)
    {
        if (rtrd_worker_init(ctx, wrk+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            free(wrk);
            thread_pool_destroy(ctx->recvtp);
            ctx->recvtp = NULL;
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtrd_workers_destroy
 **功    能: 销毁工作线程池
 **输入参数:
 **     ctx: 全局对象
 **     param: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
void rtrd_workers_destroy(void *_ctx, void *param)
{
    int idx;
    rtrd_cntx_t *ctx = (rtrd_cntx_t *)_ctx;
    rtrd_conf_t *conf = &ctx->conf;
    rtmq_worker_t *wrk = (rtmq_worker_t *)ctx->worktp->data;

    for (idx=0; idx<conf->work_thd_num; ++idx, ++wrk)
    {
        CLOSE(wrk->cmd_sck_id);
    }

    FREE(ctx->worktp->data);
    thread_pool_destroy(ctx->recvtp);

    return;
}

/******************************************************************************
 **函数名称: rtrd_proc_def_hdl
 **功    能: 默认消息处理函数
 **输入参数:
 **     type: 消息类型
 **     orig: 源设备ID
 **     buff: 消息内容
 **     len: 内容长度
 **     param: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static int rtrd_proc_def_hdl(int type, int orig, char *buff, size_t len, void *param)
{
    return RTMQ_OK;
}

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: smti.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输协议
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/
#include <memory.h>
#include <assert.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "smti_cmd.h"
#include "xml_tree.h"
#include "smti_comm.h"
#include "smti_recv.h"
#include "thread_pool.h"
#include "smti_svr.h"

static int _smti_init(smti_cntx_t *ctx);
static int smti_creat_recvq(smti_cntx_t *ctx);

static int smti_creat_recvtp(smti_cntx_t *ctx);
static void smti_recvtp_destroy(void *_ctx);

/******************************************************************************
 **函数名称: smti_init
 **功    能: 初始化SMTI接收端
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
smti_cntx_t *smti_init(const smti_conf_t *conf, log_cycle_t *log)
{
    smti_cntx_t *ctx;

    /* 1. 创建全局对象 */
    ctx = (smti_cntx_t *)calloc(1, sizeof(smti_cntx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* 2. 备份配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smti_conf_t));

    ctx->conf.rqnum = SMTI_RECV_WORKER_HDL_QNUM * conf->wrk_thd_num;

    /* 3. 初始化接收端 */
    if (_smti_init(ctx))
    {
        Free(ctx);
        log_error(ctx->log, ctx->log, "Initialize recv failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: smti_startup
 **功    能: 启动SMTI接收端
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
int smti_startup(smti_cntx_t *ctx)
{
    int idx;
    thread_pool_t *tp;
    smti_listen_t *lsn = &ctx->listen;

    /* 1. 设置接收线程回调 */
    tp = ctx->recvtp;
    for (idx=0; idx<tp->num; ++idx)
    {
        thread_pool_add_worker(tp, smti_rsvr_routine, ctx);
    }

    /* 2. 设置工作线程回调 */
    tp = ctx->worktp;
    for (idx=0; idx<tp->num; ++idx)
    {
        thread_pool_add_worker(tp, smti_worker_routine, ctx);
    }
    
    /* 3. 创建侦听线程 */
    if (creat_thread(&lsn->tid, smti_listen_routine, ctx))
    {
        log_error(ctx->log, ctx->log, "Start listen failed");
        return SMTI_ERR;
    }
    
    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_register
 **功    能: 消息处理的注册接口
 **输入参数: 
 **     ctx: 全局对象
 **     type: 扩展消息类型 Range:(0 ~ SMTI_TYPE_MAX)
 **     cb: 回调函数
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     1. 只能用于注册处理扩展数据类型的处理
 **     2. 不允许重复注册 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int smti_register(smti_cntx_t *ctx, uint32_t type, smti_reg_cb_t cb, void *args)
{
    smti_reg_t *reg;

    if (type >= SMTI_TYPE_MAX)
    {
        log_error(ctx->log, "Data type is out of range!");
        return SMTI_ERR;
    }

    if (0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Repeat register type [%d]!", type);
        return SMTI_ERR_REPEAT_REG;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->cb = cb;
    reg->args = args;
    reg->flag = 1;

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_destroy
 **功    能: 销毁SMTI对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int smti_destroy(smti_cntx_t **ctx)
{
    /* 1. 销毁侦听线程 */
    smti_lsn_destroy(&(*ctx)->listen);

    /* 2. 销毁接收线程池 */
    thread_pool_destroy_ext((*ctx)->recvtp, smti_recvtp_destroy, *ctx);

    /* 3. 销毁工作线程池 */
    thread_pool_destroy_ext((*ctx)->worktp, smti_worktp_destroy, *ctx);

    Free(*ctx);
    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_reg_init
 **功    能: 初始化注册对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int smti_reg_init(smti_cntx_t *ctx)
{
    int idx;
    smti_reg_t *reg = &ctx->reg[0];

    pthread_rwlock_init(&ctx->reg_rw_lck, NULL);

    for (idx=0; idx<SMTI_TYPE_MAX; ++idx, ++reg)
    {
        reg->type = idx;
        reg->cb = smti_work_def_hdl;
        reg->flag = 0;
        reg->args = NULL;
    }
    
    return SMTI_OK;
}

/******************************************************************************
 **函数名称: _smti_init
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
static int _smti_init(smti_cntx_t *ctx)
{
    /* 1. 初始化注册信息 */
    smti_reg_init(ctx);

    /* 2. 创建接收队列 */
    if (smti_creat_recvq(ctx))
    {
        log_error(ctx->log, ctx->log, "Create recv queue failed!");
        return SMTI_ERR;
    }

    /* 3. 创建接收线程池 */
    if (smti_creat_recvtp(ctx))
    {
        log_error(ctx->log, ctx->log, "Create recv thread pool failed!");
        return SMTI_ERR;
    }

    /* 4. 创建工作线程池 */
    if (smti_creat_worktp(ctx))
    {
        log_error(ctx->log, ctx->log, "Create worker thread pool failed!");
        return SMTI_ERR;
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_creat_recvq
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
static int smti_creat_recvq(smti_cntx_t *ctx)
{
    int idx;
    smti_conf_t *conf = &ctx->conf;

    /* 1. 创建队列数组 */
    ctx->recvq = calloc(conf->rqnum, sizeof(queue_t *));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTI_ERR;
    }

    /* 2. 依次创建接收队列 */
    for(idx=0; idx<conf->rqnum; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx])
        {
            log_error(ctx->log, "Create queue failed! max:%d size:%d",
                    conf->recvq.max, conf->recvq.size);
            return SMTI_ERR;
        }
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_creat_recvtp
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
static int smti_creat_recvtp(smti_cntx_t *ctx)
{
    int idx;
    smti_rsvr_t *rsvr;
    smti_conf_t *conf = &ctx->conf;

    /* 1. 创建线程池 */
    if (thread_pool_init(&ctx->recvtp, conf->recv_thd_num))
    {
        log_error(rsvr->log, "Initialize thread pool failed!");
        return SMTI_ERR;
    }

    /* 2. 创建接收对象 */
    ctx->recvtp->data = (void *)calloc(conf->recv_thd_num, sizeof(smti_rsvr_t));
    if (NULL == ctx->recvtp->data)
    {
        log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));

        thread_pool_destroy(ctx->recvtp);
        ctx->recvtp = NULL;
        return SMTI_ERR;
    }

    /* 3. 初始化接收对象 */
    rsvr = (smti_rsvr_t *)ctx->recvtp->data;
    for (idx=0; idx<conf->recv_thd_num; ++idx, ++rsvr)
    {
        rsvr->tidx = idx;

        if (smti_rsvr_init(ctx, rsvr))
        {
            thread_pool_destroy(ctx->recvtp);
            ctx->recvtp = NULL;

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return SMTI_ERR;
        }
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_recvtp_destroy
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
static void smti_recvtp_destroy(void *_ctx, void *args)
{
    int idx;
    smti_cntx_t *ctx = (smti_cntx_t *)_ctx;
    smti_rsvr_t *rsvr = (smti_rsvr_t *)ctx->recvtp->data;

    for (idx=0; idx<ctx->conf.recv_thd_num; ++idx, ++rsvr)
    {
        /* 1. 关闭命令套接字 */
        Close(rsvr->cmd_sck_id);

        /* 2. 关闭通信套接字 */
        smti_rsvr_del_all_sck_hdl(rsvr);

        slab_destroy(&rsvr->pool);
    }

    Free(args);

    return;
}

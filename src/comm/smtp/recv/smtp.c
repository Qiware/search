/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: smtp.c
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

#include "smtp_cmd.h"
#include "xml_tree.h"
#include "smtp_comm.h"
#include "smtp_recv.h"
#include "thread_pool.h"
#include "smtp_svr.h"

static int _smtp_init(smtp_cntx_t *ctx);
static int smtp_creat_recvq(smtp_cntx_t *ctx);

/******************************************************************************
 **函数名称: smtp_init
 **功    能: 初始化SMTP接收端
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
smtp_cntx_t *smtp_init(const smtp_conf_t *conf, log_cycle_t *log)
{
    smtp_cntx_t *ctx;

    /* 1. 创建全局对象 */
    ctx = (smtp_cntx_t *)calloc(1, sizeof(smtp_cntx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* 2. 备份配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smtp_conf_t));

    ctx->conf.recvq_num = SMTP_RECV_WORKER_HDL_QNUM * conf->wrk_thd_num;

    /* 3. 初始化接收端 */
    if (_smtp_init(ctx))
    {
        Free(ctx);
        log_error(ctx->log, ctx->log, "Initialize recv failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: smtp_startup
 **功    能: 启动SMTP接收端
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
int smtp_startup(smtp_cntx_t *ctx)
{
    int idx;
    thread_pool_t *tpool;
    smtp_listen_t *lsn = &ctx->listen;

    /* 1. 设置接收线程回调 */
    tpool = ctx->recvtp;
    for (idx=0; idx<tpool->num; ++idx)
    {
        thread_pool_add_worker(tpool, smtp_rsvr_routine, ctx);
    }

    /* 2. 设置工作线程回调 */
    tpool = ctx->worktp;
    for (idx=0; idx<tpool->num; ++idx)
    {
        thread_pool_add_worker(tpool, smtp_worker_routine, ctx);
    }
    
    /* 3. 创建侦听线程 */
    if (creat_thread(&lsn->tid, smtp_listen_routine, ctx))
    {
        log_error(ctx->log, ctx->log, "Start listen failed");
        return SMTP_ERR;
    }
    
    return SMTP_OK;
}

/******************************************************************************
 **函数名称: smtp_register
 **功    能: 消息处理的注册接口
 **输入参数: 
 **     ctx: 全局对象
 **     type: 扩展消息类型 Range:(0 ~ SMTP_TYPE_MAX)
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
int smtp_register(smtp_cntx_t *ctx, uint32_t type, smtp_reg_cb_t cb, void *args)
{
    smtp_reg_t *reg;

    if (type >= SMTP_TYPE_MAX)
    {
        log_error(ctx->log, "Data type is out of range!");
        return SMTP_ERR;
    }

    if (0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Repeat register type [%d]!", type);
        return SMTP_ERR_REPEAT_REG;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->cb = cb;
    reg->args = args;
    reg->flag = 1;

    return SMTP_OK;
}

/******************************************************************************
 **函数名称: smtp_destroy
 **功    能: 销毁SMTP对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
int smtp_destroy(smtp_cntx_t **ctx)
{
    /* 1. 销毁侦听线程 */
    smtp_lsn_destroy(&(*ctx)->listen);

    /* 2. 销毁接收线程池 */
    thread_pool_destroy_ext((*ctx)->recvtp, smtp_recvtp_destroy, *ctx);

    /* 3. 销毁工作线程池 */
    thread_pool_destroy_ext((*ctx)->worktp, smtp_worktp_destroy, *ctx);

    Free(*ctx);
    return SMTP_OK;
}

/******************************************************************************
 **函数名称: smtp_reg_init
 **功    能: 初始化注册对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int smtp_reg_init(smtp_cntx_t *ctx)
{
    int idx;
    smtp_reg_t *reg = &ctx->reg[0];

    pthread_rwlock_init(&ctx->reg_rw_lck, NULL);

    for (idx=0; idx<SMTP_TYPE_MAX; ++idx, ++reg)
    {
        reg->type = idx;
        reg->cb = smtp_work_def_hdl;
        reg->flag = 0;
        reg->args = NULL;
    }
    
    return SMTP_OK;
}

/******************************************************************************
 **函数名称: _smtp_init
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
static int _smtp_init(smtp_cntx_t *ctx)
{
    /* 1. 初始化注册信息 */
    smtp_reg_init(ctx);

    /* 2. 创建接收队列 */
    if (smtp_creat_recvq(ctx))
    {
        log_error(ctx->log, ctx->log, "Create recv queue failed!");
        return SMTP_ERR;
    }

    /* 3. 创建接收线程池 */
    if (smtp_creat_recvtp(ctx))
    {
        log_error(ctx->log, ctx->log, "Create recv thread pool failed!");
        return SMTP_ERR;
    }

    /* 4. 创建工作线程池 */
    if (smtp_creat_worktp(ctx))
    {
        log_error(ctx->log, ctx->log, "Create worker thread pool failed!");
        return SMTP_ERR;
    }

    return SMTP_OK;
}

/******************************************************************************
 **函数名称: smtp_creat_recvq
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
static int smtp_creat_recvq(smtp_cntx_t *ctx)
{
    int idx;
    smtp_conf_t *conf = &ctx->conf;

    /* 1. 创建队列数组 */
    ctx->recvq = calloc(conf->recvq_num, sizeof(queue_t *));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTP_ERR;
    }

    /* 2. 依次创建接收队列 */
    for(idx=0; idx<conf->recvq_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq[idx])
        {
            log_error(ctx->log, "Create queue failed! max:%d size:%d",
                    conf->recvq.max, conf->recvq.size);
            return SMTP_ERR;
        }
    }

    return SMTP_OK;
}

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: smtc.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
#include <memory.h>
#include <assert.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "smtc.h"
#include "xml_tree.h"
#include "smtc_cmd.h"
#include "smtc_priv.h"
#include "thread_pool.h"


/* 静态函数 */
static smtc_worker_t *smtc_worker_get_curr(smtc_cntx_t *ctx);
static int smtc_worker_event_core_hdl(smtc_cntx_t *ctx, smtc_worker_t *worker);
static int smtc_worker_cmd_proc_req_hdl(smtc_cntx_t *ctx, smtc_worker_t *worker, const smtc_cmd_t *cmd);

/******************************************************************************
 **函数名称: smtc_worker_routine
 **功    能: 运行工作线程
 **输入参数: 
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: VOID *
 **实现描述: 
 **     1. 获取工作对象
 **     2. 等待事件通知
 **     3. 进行事件处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
void *smtc_worker_routine(void *_ctx)
{
    int ret, idx;
    smtc_worker_t *worker;
    smtc_cmd_proc_req_t *req;
    struct timeval timeout;
    smtc_cntx_t *ctx = (smtc_cntx_t *)_ctx;

    /* 1. 获取工作对象 */
    worker = smtc_worker_get_curr(ctx);
    if (NULL == worker)
    {
        log_fatal(ctx->log, "Get current worker failed!");
        abort();
        return (void *)-1;
    }

    for (;;)
    {
        /* 2. 等待事件通知 */
        FD_ZERO(&worker->rdset);

        FD_SET(worker->cmd_sck_id, &worker->rdset);
        worker->max = worker->cmd_sck_id;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        ret = select(worker->max+1, &worker->rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_fatal(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret)
        {
            /* 超时: 模拟处理命令 */
            smtc_cmd_t cmd;
            req = (smtc_cmd_proc_req_t *)&cmd.args;

            for (idx=0; idx<SMTC_WORKER_HDL_QNUM; ++idx)
            {
                memset(&cmd, 0, sizeof(cmd));

                cmd.type = SMTC_CMD_PROC_REQ;
                req->num = -1;
                req->rqidx = SMTC_WORKER_HDL_QNUM * worker->tidx + idx;

                smtc_worker_cmd_proc_req_hdl(ctx, worker, &cmd);
            }
            continue;
        }

        /* 3. 进行事件处理 */
        smtc_worker_event_core_hdl(ctx, worker);
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: smtc_worker_get_curr
 **功    能: 获取工作对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 工作对象
 **实现描述: 
 **     1. 获取线程编号
 **     2. 返回工作对象
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static smtc_worker_t *smtc_worker_get_curr(smtc_cntx_t *ctx)
{
    int tidx;
    
    /* 1. 获取线程编号 */
    tidx = thread_pool_get_tidx(ctx->worktp);
    if (tidx < 0)
    {
        log_fatal(ctx->log, "Get thread index failed!");
        return NULL;
    }

    /* 2. 返回工作对象 */
    return (smtc_worker_t *)(ctx->worktp->data + tidx * sizeof(smtc_worker_t));
}

/******************************************************************************
 **函数名称: smtc_worker_init
 **功    能: 初始化工作服务
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **     tidx: 工作对象编号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建命令套接字
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
int smtc_worker_init(smtc_cntx_t *ctx, smtc_worker_t *worker, int tidx)
{
    char path[FILE_PATH_MAX_LEN];
    smtc_conf_t *conf = &ctx->conf; 

    worker->tidx = tidx;
    worker->log = ctx->log;

    /* 1. 创建命令套接字 */
    smtc_worker_usck_path(conf, path, worker->tidx);
    
    worker->cmd_sck_id = unix_udp_creat(path);
    if (worker->cmd_sck_id < 0)
    {
        log_error(worker->log, "Create unix-udp socket failed!");
        return SMTC_ERR;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_worker_event_core_hdl
 **功    能: 核心事件处理
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建命令套接字
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static int smtc_worker_event_core_hdl(smtc_cntx_t *ctx, smtc_worker_t *worker)
{
    smtc_cmd_t cmd;

    if (!FD_ISSET(worker->cmd_sck_id, &worker->rdset))
    {
        return SMTC_OK; /* 无数据 */
    }

    if (unix_udp_recv(worker->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
        return SMTC_ERR_RECV_CMD;
    }

    switch (cmd.type)
    {
        case SMTC_CMD_PROC_REQ:
        {
            return smtc_worker_cmd_proc_req_hdl(ctx, worker, &cmd);
        }
        default:
        {
            log_error(worker->log, "Received unknown type! %d", cmd.type);
            return SMTC_ERR_UNKNOWN_CMD;
        }
    }

    return SMTC_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: smtc_worker_cmd_proc_req_hdl
 **功    能: 处理请求的处理
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **     cmd: 命令信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static int smtc_worker_cmd_proc_req_hdl(smtc_cntx_t *ctx, smtc_worker_t *worker, const smtc_cmd_t *cmd)
{
    void *addr;
    queue_t *rq;
    smtc_header_t *head;
    smtc_reg_t *reg;
    const smtc_cmd_proc_req_t *work_cmd = (const smtc_cmd_proc_req_t *)&cmd->args;

    /* 1. 获取接收队列 */
    rq = ctx->recvq[work_cmd->rqidx];
   
    while (1)
    {
        /* 2. 从接收队列获取数据 */
        addr = queue_pop(rq);
        if (NULL == addr)
        {   
            log_trace(worker->log, "Didn't get data from queue!");
            return SMTC_OK;
        }
        
        /* 3. 执行回调函数 */
        head = (smtc_header_t *)addr;

        reg = &ctx->reg[head->type];

        reg->proc(head->type, addr+sizeof(smtc_header_t), head->length, reg->args);

        /* 4. 释放内存空间 */
        queue_dealloc(rq, addr);

        ++worker->proc_total; /* 处理计数 */
    }

    return SMTC_OK;
}

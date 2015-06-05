/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/

#include "rtdt_cmd.h"
#include "rtdt_comm.h"
#include "rtrd_recv.h"
#include "thread_pool.h"

/* 静态函数 */
static rtdt_worker_t *rtrd_worker_get_curr(rtrd_cntx_t *ctx);
static int rtrd_worker_event_core_hdl(rtrd_cntx_t *ctx, rtdt_worker_t *wrk);
static int rtrd_worker_cmd_proc_req_hdl(rtrd_cntx_t *ctx, rtdt_worker_t *wrk, const rtdt_cmd_t *cmd);

/******************************************************************************
 **函数名称: rtrd_worker_routine
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
void *rtrd_worker_routine(void *_ctx)
{
    int ret, idx;
    rtdt_worker_t *wrk;
    rtdt_cmd_proc_req_t *req;
    struct timeval timeout;
    rtrd_cntx_t *ctx = (rtrd_cntx_t *)_ctx;

    /* 1. 获取工作对象 */
    wrk = rtrd_worker_get_curr(ctx);
    if (NULL == wrk)
    {
        log_fatal(ctx->log, "Get current wrk failed!");
        abort();
        return (void *)-1;
    }

    for (;;)
    {
        /* 2. 等待事件通知 */
        FD_ZERO(&wrk->rdset);

        FD_SET(wrk->cmd_sck_id, &wrk->rdset);
        wrk->max = wrk->cmd_sck_id;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        ret = select(wrk->max+1, &wrk->rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_fatal(wrk->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret)
        {
            /* 超时: 模拟处理命令 */
            rtdt_cmd_t cmd;
            req = (rtdt_cmd_proc_req_t *)&cmd.args;

            for (idx=0; idx<RTDT_WORKER_HDL_QNUM; ++idx)
            {
                memset(&cmd, 0, sizeof(cmd));

                cmd.type = RTDT_CMD_PROC_REQ;
                req->num = -1;
                req->rqidx = RTDT_WORKER_HDL_QNUM * wrk->tidx + idx;

                rtrd_worker_cmd_proc_req_hdl(ctx, wrk, &cmd);
            }
            continue;
        }

        /* 3. 进行事件处理 */
        rtrd_worker_event_core_hdl(ctx, wrk);
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: rtrd_worker_get_curr
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
static rtdt_worker_t *rtrd_worker_get_curr(rtrd_cntx_t *ctx)
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
    return (rtdt_worker_t *)(ctx->worktp->data + tidx * sizeof(rtdt_worker_t));
}

/******************************************************************************
 **函数名称: rtrd_worker_init
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
int rtrd_worker_init(rtrd_cntx_t *ctx, rtdt_worker_t *wrk, int tidx)
{
    char path[FILE_PATH_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;

    wrk->tidx = tidx;
    wrk->log = ctx->log;

    /* 1. 创建命令套接字 */
    rtrd_worker_usck_path(conf, path, wrk->tidx);

    wrk->cmd_sck_id = unix_udp_creat(path);
    if (wrk->cmd_sck_id < 0)
    {
        log_error(wrk->log, "Create unix-udp socket failed!");
        return RTDT_ERR;
    }

    return RTDT_OK;
}

/******************************************************************************
 **函数名称: rtrd_worker_event_core_hdl
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
static int rtrd_worker_event_core_hdl(rtrd_cntx_t *ctx, rtdt_worker_t *wrk)
{
    rtdt_cmd_t cmd;

    if (!FD_ISSET(wrk->cmd_sck_id, &wrk->rdset))
    {
        return RTDT_OK; /* 无数据 */
    }

    if (unix_udp_recv(wrk->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(wrk->log, "errmsg:[%d] %s", errno, strerror(errno));
        return RTDT_ERR_RECV_CMD;
    }

    switch (cmd.type)
    {
        case RTDT_CMD_PROC_REQ:
        {
            return rtrd_worker_cmd_proc_req_hdl(ctx, wrk, &cmd);
        }
        default:
        {
            log_error(wrk->log, "Received unknown type! %d", cmd.type);
            return RTDT_ERR_UNKNOWN_CMD;
        }
    }

    return RTDT_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: rtrd_worker_cmd_proc_req_hdl
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
static int rtrd_worker_cmd_proc_req_hdl(rtrd_cntx_t *ctx, rtdt_worker_t *wrk, const rtdt_cmd_t *cmd)
{
    void *addr;
    queue_t *rq;
    rtdt_reg_t *reg;
    rtdt_header_t *head;
    const rtdt_cmd_proc_req_t *work_cmd = (const rtdt_cmd_proc_req_t *)&cmd->args;

    /* 1. 获取接收队列 */
    rq = ctx->recvq[work_cmd->rqidx];

    while (1)
    {
        /* 2. 从接收队列获取数据 */
        addr = queue_pop(rq);
        if (NULL == addr)
        {
            return RTDT_OK;
        }

        /* 3. 执行回调函数 */
        head = (rtdt_header_t *)addr;

        reg = &ctx->reg[head->type];
        if (NULL == reg->proc)
        {
            ++wrk->drop_total;   /* 丢弃计数 */
            continue;
        }

        if (reg->proc(
                head->type, head->devid,
                addr + sizeof(rtdt_header_t),
                head->length, reg->args))
        {
            ++wrk->err_total;    /* 错误计数 */
        }
        else
        {
            ++wrk->proc_total;   /* 处理计数 */
        }

        /* > 释放内存空间 */
        queue_dealloc(rq, addr);
    }

    return RTDT_OK;
}

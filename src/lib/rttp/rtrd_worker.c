/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: rttp.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/

#include "rttp_cmd.h"
#include "rttp_comm.h"
#include "rtrd_recv.h"
#include "thread_pool.h"

/* 静态函数 */
static rttp_worker_t *rtrd_worker_get_curr(rtrd_cntx_t *ctx);
static int rtrd_worker_event_core_hdl(rtrd_cntx_t *ctx, rttp_worker_t *worker);
static int rtrd_worker_cmd_proc_req_hdl(rtrd_cntx_t *ctx, rttp_worker_t *worker, const rttp_cmd_t *cmd);

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
    rttp_worker_t *worker;
    rttp_cmd_proc_req_t *req;
    struct timeval timeout;
    rtrd_cntx_t *ctx = (rtrd_cntx_t *)_ctx;

    /* 1. 获取工作对象 */
    worker = rtrd_worker_get_curr(ctx);
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
            if (EINTR == errno) { continue; }
            log_fatal(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret)
        {
            /* 超时: 模拟处理命令 */
            rttp_cmd_t cmd;
            req = (rttp_cmd_proc_req_t *)&cmd.param;

            for (idx=0; idx<RTTP_WORKER_HDL_QNUM; ++idx)
            {
                memset(&cmd, 0, sizeof(cmd));

                cmd.type = RTTP_CMD_PROC_REQ;
                req->num = -1;
                req->rqidx = RTTP_WORKER_HDL_QNUM * worker->id + idx;

                rtrd_worker_cmd_proc_req_hdl(ctx, worker, &cmd);
            }
            continue;
        }

        /* 3. 进行事件处理 */
        rtrd_worker_event_core_hdl(ctx, worker);
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
static rttp_worker_t *rtrd_worker_get_curr(rtrd_cntx_t *ctx)
{
    int id;

    /* 1. 获取线程编号 */
    id = thread_pool_get_tidx(ctx->worktp);
    if (id < 0)
    {
        log_fatal(ctx->log, "Get thread index failed!");
        return NULL;
    }

    /* 2. 返回工作对象 */
    return (rttp_worker_t *)(ctx->worktp->data + id * sizeof(rttp_worker_t));
}

/******************************************************************************
 **函数名称: rtrd_worker_init
 **功    能: 初始化工作服务
 **输入参数:
 **     ctx: 全局对象
 **     worker: 工作对象
 **     id: 工作对象编号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
int rtrd_worker_init(rtrd_cntx_t *ctx, rttp_worker_t *worker, int id)
{
    char path[FILE_PATH_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;

    worker->id = id;
    worker->log = ctx->log;

    /* > 创建命令套接字 */
    rtrd_worker_usck_path(conf, path, worker->id);

    worker->cmd_sck_id = unix_udp_creat(path);
    if (worker->cmd_sck_id < 0)
    {
        log_error(worker->log, "Create unix-udp socket failed!");
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_worker_event_core_hdl
 **功    能: 核心事件处理
 **输入参数:
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 接收命令, 再根据命令类型调用相应的处理函数!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/
static int rtrd_worker_event_core_hdl(rtrd_cntx_t *ctx, rttp_worker_t *worker)
{
    rttp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    if (!FD_ISSET(worker->cmd_sck_id, &worker->rdset))
    {
        return RTTP_OK; /* 无数据 */
    }

    if (unix_udp_recv(worker->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
        return RTTP_ERR_RECV_CMD;
    }

    switch (cmd.type)
    {
        case RTTP_CMD_PROC_REQ:
        {
            return rtrd_worker_cmd_proc_req_hdl(ctx, worker, &cmd);
        }
        default:
        {
            log_error(worker->log, "Received unknown type! %d", cmd.type);
            return RTTP_ERR_UNKNOWN_CMD;
        }
    }

    return RTTP_ERR_UNKNOWN_CMD;
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
static int rtrd_worker_cmd_proc_req_hdl(rtrd_cntx_t *ctx, rttp_worker_t *worker, const rttp_cmd_t *cmd)
{
#define RTRD_WORK_POP_NUM     (32)
    int idx, num;
    void *addr[RTRD_WORK_POP_NUM];
    queue_t *rq;
    rttp_reg_t *reg;
    rttp_header_t *head;
    const rttp_cmd_proc_req_t *work_cmd = (const rttp_cmd_proc_req_t *)&cmd->param;

    /* > 获取接收队列 */
    rq = ctx->recvq[work_cmd->rqidx];

    while (1)
    {
        /* > 从接收队列获取数据 */
        num = MIN(queue_used(rq), RTRD_WORK_POP_NUM);
        if (0 == num)
        {
            return RTTP_OK;
        }

        num = queue_mpop(rq, addr, num);
        if (0 == num)
        {
            continue;
        }

        /* > 依次处理各条数据 */
        for (idx=0; idx<num; ++idx)
        {
            head = (rttp_header_t *)addr[idx];

            reg = &ctx->reg[head->type];
            if (NULL == reg->proc)
            {
                queue_dealloc(rq, addr[idx]);
                ++worker->drop_total;   /* 丢弃计数 */
                continue;
            }

            if (reg->proc(
                    head->type, head->nodeid,
                    addr[idx] + sizeof(rttp_header_t),
                    head->length, reg->param))
            {
                ++worker->err_total;    /* 错误计数 */
            }
            else
            {
                ++worker->proc_total;   /* 处理计数 */
            }

            /* > 释放内存空间 */
            queue_dealloc(rq, addr[idx]);
        }
    }

    return RTTP_OK;
}

/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: rtmq.c
 ** 版本号: 1.0
 ** 描  述: 实时消息队列(Real-Time Message Queue)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2015.01.06 #
 ******************************************************************************/

#include "mem_ref.h"
#include "rtmq_mesg.h"
#include "rtmq_comm.h"
#include "rtmq_recv.h"
#include "thread_pool.h"

#define RTRD_WORK_POP_NUM     (1024)

/* 静态函数 */
static rtmq_worker_t *rtmq_worker_get_curr(rtmq_cntx_t *ctx);
static int rtmq_worker_event_core_hdl(rtmq_cntx_t *ctx, rtmq_worker_t *worker);
static int rtmq_worker_cmd_proc_req_hdl(rtmq_cntx_t *ctx, rtmq_worker_t *worker, const rtmq_cmd_t *cmd);

/******************************************************************************
 **函数名称: rtmq_worker_routine
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
void *rtmq_worker_routine(void *_ctx)
{
    int ret, idx;
    rtmq_worker_t *worker;
    rtmq_cmd_proc_req_t *req;
    struct timeval timeout;
    rtmq_cntx_t *ctx = (rtmq_cntx_t *)_ctx;

    /* 1. 获取工作对象 */
    worker = rtmq_worker_get_curr(ctx);
    if (NULL == worker) {
        log_fatal(ctx->log, "Get current worker failed!");
        abort();
        return (void *)-1;
    }

    for (;;) {
        /* 2. 等待事件通知 */
        FD_ZERO(&worker->rdset);

        FD_SET(worker->cmd_sck_id, &worker->rdset);
        worker->max = worker->cmd_sck_id;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        ret = select(worker->max+1, &worker->rdset, NULL, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            log_fatal(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret) {
            /* 超时: 模拟处理命令 */
            rtmq_cmd_t cmd;
            req = (rtmq_cmd_proc_req_t *)&cmd.param;

            for (idx=0; idx<RTMQ_WORKER_HDL_QNUM; ++idx) {
                memset(&cmd, 0, sizeof(cmd));

                cmd.type = RTMQ_CMD_PROC_REQ;
                req->num = -1;
                req->rqidx = RTMQ_WORKER_HDL_QNUM * worker->id + idx;

                rtmq_worker_cmd_proc_req_hdl(ctx, worker, &cmd);
            }
            continue;
        }

        /* 3. 进行事件处理 */
        rtmq_worker_event_core_hdl(ctx, worker);
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: rtmq_worker_get_curr
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
static rtmq_worker_t *rtmq_worker_get_curr(rtmq_cntx_t *ctx)
{
    int id;

    /* 1. 获取线程编号 */
    id = thread_pool_get_tidx(ctx->worktp);
    if (id < 0) {
        log_fatal(ctx->log, "Get thread index failed!");
        return NULL;
    }

    /* 2. 返回工作对象 */
    return (rtmq_worker_t *)(ctx->worktp->data + id * sizeof(rtmq_worker_t));
}

/******************************************************************************
 **函数名称: rtmq_worker_init
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
int rtmq_worker_init(rtmq_cntx_t *ctx, rtmq_worker_t *worker, int id)
{
    char path[FILE_PATH_MAX_LEN];
    rtmq_conf_t *conf = &ctx->conf;

    worker->id = id;
    worker->log = ctx->log;

    /* > 创建命令套接字 */
    rtmq_worker_usck_path(conf, path, worker->id);

    worker->cmd_sck_id = unix_udp_creat(path);
    if (worker->cmd_sck_id < 0) {
        log_error(worker->log, "Create unix-udp socket failed!");
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_worker_event_core_hdl
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
static int rtmq_worker_event_core_hdl(rtmq_cntx_t *ctx, rtmq_worker_t *worker)
{
    rtmq_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    if (!FD_ISSET(worker->cmd_sck_id, &worker->rdset)) {
        return RTMQ_OK; /* 无数据 */
    }

    if (unix_udp_recv(worker->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0) {
        log_error(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
        return RTMQ_ERR_RECV_CMD;
    }

    switch (cmd.type) {
        case RTMQ_CMD_PROC_REQ:
        {
            return rtmq_worker_cmd_proc_req_hdl(ctx, worker, &cmd);
        }
        default:
        {
            log_error(worker->log, "Received unknown type! %d", cmd.type);
            return RTMQ_ERR_UNKNOWN_CMD;
        }
    }

    return RTMQ_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: rtmq_worker_cmd_proc_req_hdl
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
static int rtmq_worker_cmd_proc_req_hdl(rtmq_cntx_t *ctx, rtmq_worker_t *worker, const rtmq_cmd_t *cmd)
{
    int idx, num;
    queue_t *rq;
    rtmq_header_t *head;
    rtmq_reg_t *reg, key;
    rtmq_recv_item_t *item[RTRD_WORK_POP_NUM];
    const rtmq_cmd_proc_req_t *work_cmd = (const rtmq_cmd_proc_req_t *)&cmd->param;

    /* > 获取接收队列 */
    rq = ctx->recvq[work_cmd->rqidx];

    while (1) {
        /* > 从接收队列获取数据 */
        num = MIN(queue_used(rq), RTRD_WORK_POP_NUM);
        if (0 == num) {
            return RTMQ_OK;
        }

        num = queue_mpop(rq, (void **)item, num);
        if (0 == num) {
            continue;
        }

        /* > 依次处理各条数据 */
        for (idx=0; idx<num; ++idx) {
            head = (rtmq_header_t *)item[idx]->data;

            key.type = head->type;

            reg = (rtmq_reg_t *)avl_query(ctx->reg, (void *)&key);
            if (NULL == reg) {
                key.type = 0; // 未知命令
                reg = (rtmq_reg_t *)avl_query(ctx->reg, (void *)&key);
                if (NULL == reg) {
                    ++worker->drop_total;   /* 丢弃计数 */
                    mem_ref_decr(item[idx]->base);
                    queue_dealloc(rq, (void *)item[idx]);
                    log_trace(ctx->log, "Drop data! type:%u", head->type);
                    continue;
                }
            }

            if (reg->proc(head->type, head->nid,
                (void *)(head + 1), head->length, reg->param)) {
                ++worker->err_total;    /* 错误计数 */
            }
            else {
                ++worker->proc_total;   /* 处理计数 */
            }

            /* > 释放内存空间 */
            mem_ref_decr(item[idx]->base);
            queue_dealloc(rq, (void *)item[idx]);
        }
    }

    return RTMQ_OK;
}

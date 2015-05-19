/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp_swrk.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2015.05.18 #
 ******************************************************************************/

#include "xml_tree.h"
#include "sdtp_cmd.h"
#include "sdtp_send.h"
#include "thread_pool.h"

/* 静态函数 */
static sdtp_worker_t *sdtp_swrk_get_curr(sdtp_sctx_t *ctx);
static int sdtp_swrk_event_core_hdl(sdtp_sctx_t *ctx, sdtp_worker_t *wrk);
static int sdtp_swrk_cmd_proc_req_hdl(sdtp_sctx_t *ctx, sdtp_worker_t *wrk, const sdtp_cmd_t *cmd);

/******************************************************************************
 **函数名称: sdtp_swrk_routine
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
 **作    者: # Qifeng.zou # 2015.05.18 #
 ******************************************************************************/
void *sdtp_swrk_routine(void *_ctx)
{
    int ret, idx;
    sdtp_worker_t *wrk;
    sdtp_cmd_proc_req_t *req;
    struct timeval timeout;
    sdtp_sctx_t *ctx = (sdtp_sctx_t *)_ctx;
    sdtp_ssvr_conf_t *conf = (sdtp_ssvr_conf_t *)&ctx->conf;

    /* 1. 获取工作对象 */
    wrk = sdtp_swrk_get_curr(ctx);
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
            sdtp_cmd_t cmd;
            req = (sdtp_cmd_proc_req_t *)&cmd.args;

            for (idx=0; idx<conf->work_thd_num; ++idx)
            {
                memset(&cmd, 0, sizeof(cmd));

                cmd.type = SDTP_CMD_PROC_REQ;
                req->num = -1;
                req->rqidx = idx;

                sdtp_swrk_cmd_proc_req_hdl(ctx, wrk, &cmd);
            }
            continue;
        }

        /* 3. 进行事件处理 */
        sdtp_swrk_event_core_hdl(ctx, wrk);
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: sdtp_swrk_get_curr
 **功    能: 获取工作对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 工作对象
 **实现描述: 
 **     1. 获取线程编号
 **     2. 返回工作对象
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.18 #
 ******************************************************************************/
static sdtp_worker_t *sdtp_swrk_get_curr(sdtp_sctx_t *ctx)
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
    return (sdtp_worker_t *)(ctx->worktp->data + tidx * sizeof(sdtp_worker_t));
}

/******************************************************************************
 **函数名称: sdtp_swrk_init
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
 **作    者: # Qifeng.zou # 2015.05.18 #
 ******************************************************************************/
int sdtp_swrk_init(sdtp_sctx_t *ctx, sdtp_worker_t *wrk, int tidx)
{
    char path[FILE_PATH_MAX_LEN];
    sdtp_ssvr_conf_t *conf = &ctx->conf; 

    wrk->tidx = tidx;
    wrk->log = ctx->log;

    /* 1. 创建命令套接字 */
    sdtp_swrk_usck_path(conf, path, wrk->tidx);
    
    wrk->cmd_sck_id = unix_udp_creat(path);
    if (wrk->cmd_sck_id < 0)
    {
        log_error(wrk->log, "Create unix-udp socket failed!");
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_swrk_event_core_hdl
 **功    能: 核心事件处理
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建命令套接字
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.18 #
 ******************************************************************************/
static int sdtp_swrk_event_core_hdl(sdtp_sctx_t *ctx, sdtp_worker_t *wrk)
{
    sdtp_cmd_t cmd;

    if (!FD_ISSET(wrk->cmd_sck_id, &wrk->rdset))
    {
        return SDTP_OK; /* 无数据 */
    }

    if (unix_udp_recv(wrk->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(wrk->log, "errmsg:[%d] %s", errno, strerror(errno));
        return SDTP_ERR_RECV_CMD;
    }

    switch (cmd.type)
    {
        case SDTP_CMD_PROC_REQ:
        {
            return sdtp_swrk_cmd_proc_req_hdl(ctx, wrk, &cmd);
        }
        default:
        {
            log_error(wrk->log, "Received unknown type! %d", cmd.type);
            return SDTP_ERR_UNKNOWN_CMD;
        }
    }

    return SDTP_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: sdtp_swrk_cmd_proc_req_hdl
 **功    能: 处理请求的处理
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **     cmd: 命令信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.18 #
 ******************************************************************************/
static int sdtp_swrk_cmd_proc_req_hdl(sdtp_sctx_t *ctx, sdtp_worker_t *wrk, const sdtp_cmd_t *cmd)
{
    int *num, idx;
    void *addr, *ptr;
    queue_t *rq;
    sdtp_header_t *head;
    sdtp_reg_t *reg;
    const sdtp_cmd_proc_req_t *work_cmd = (const sdtp_cmd_proc_req_t *)&cmd->args;

    /* 1. 获取接收队列 */
    rq = ctx->recvq[work_cmd->rqidx];
   
    while (1)
    {
        /* 2. 从接收队列获取数据 */
        addr = queue_pop(rq);
        if (NULL == addr)
        {   
            log_trace(wrk->log, "Didn't get data from queue!");
            return SDTP_OK;
        }

        num = (int *)addr;
        ptr = addr + sizeof(int);
        
        for (idx=0; idx<*num; ++idx)
        {
            /* 3. 执行回调函数 */
            head = (sdtp_header_t *)ptr;

            reg = &ctx->reg[head->type];
            if (NULL == reg->proc)
            {
                ptr += head->length + sizeof(sdtp_header_t);
                ++wrk->drop_total;   /* 丢弃计数 */
                continue;
            }

            if (reg->proc(head->type, ptr+sizeof(sdtp_header_t), head->length, reg->args))
            {
                ++wrk->err_total;    /* 错误计数 */
            }
            else
            {
                ++wrk->proc_total;   /* 处理计数 */
            }

            ptr += head->length + sizeof(sdtp_header_t);
        }

        /* 4. 释放内存空间 */
        queue_dealloc(rq, addr);
    }

    return SDTP_OK;
}

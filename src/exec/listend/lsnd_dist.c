/******************************************************************************
 ** Copyright(C) 2014-2024 Toushi technology Co., Ltd
 **
 ** 文件名: lsnd_dist.c
 ** 版本号: 1.0
 ** 描  述: 将消息分发至各RSVR线程的发送队列中
 ** 作  者: # Qifeng.zou # Fri 19 Jun 2015 11:20:49 PM CST #
 ******************************************************************************/

#include "conf.h"
#include "mesg.h"
#include "command.h"
#include "listend.h"

#define LSND_DIST_POP_NUM   (1024)  /* 分发队列弹出数目 */
#define LSND_DIST_TMO_SEC   (5)     /* 分发服务超时间隔(秒) */
#define LSND_DIST_TMO_USEC  (0)     /* 分发服务超时间隔(微秒) */

static int lsnd_dsvr_event_hdl(lsnd_cntx_t *ctx, lsnd_dsvr_t *dsvr);
static int lsnd_dsvr_timeout_hdl(lsnd_cntx_t *ctx, lsnd_dsvr_t *dsvr);

/******************************************************************************
 **函数名称: lsnd_dsvr_init
 **功    能: 初始化分发服务
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-20 21:41:37 #
 ******************************************************************************/
int lsnd_dsvr_init(lsnd_cntx_t *ctx)
{
    char path[FILE_PATH_MAX_LEN];
    lsnd_conf_t *conf = &ctx->conf;
    lsnd_dsvr_t *dsvr = &ctx->dsvr;

    LSND_GET_DSVR_CMD_PATH(path, sizeof(path), conf->wdir);

    dsvr->cmd_sck_id = unix_udp_creat(path);
    if (dsvr->cmd_sck_id < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        free(dsvr);
        return LSND_ERR;
    }

    return LSND_OK;
}

/******************************************************************************
 **函数名称: lsnd_dsvr_routine
 **功    能: 运行分发线程
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述:
 **注意事项: 消息结构: 流水信息 + 消息头 + 消息体
 **作    者: # Qifeng.zou # 2015.05.15 #
 ******************************************************************************/
void *lsnd_dsvr_routine(void *_ctx)
{
    int ret;
    struct timeval timeout;
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)_ctx;
    lsnd_dsvr_t *dsvr = &ctx->dsvr;

    nice(-20);

    while (1)
    {
        FD_ZERO(&dsvr->rdset);
        FD_SET(dsvr->cmd_sck_id, &dsvr->rdset);

        timeout.tv_sec = LSND_DIST_TMO_SEC;
        timeout.tv_usec = LSND_DIST_TMO_USEC;
        ret = select(dsvr->cmd_sck_id+1, &dsvr->rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno) { continue; }
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            lsnd_dsvr_timeout_hdl(ctx, dsvr);
            continue;
        }

        lsnd_dsvr_event_hdl(ctx, dsvr);
    }

    return (void *)-1;
}

/******************************************************************************
 **函数名称: lsnd_dsvr_cmd_dist_hdl
 **功    能: 数据分发命令的处理
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 消息结构: 流水信息 + 消息头 + 消息体
 **注意事项: WARNNING: 千万勿将共享变量参与"?:"三目运算, 否则可能出现严重错误!!!!且很难找出原因!
 **          原因: "?:"三目运算不是原子运算, 使用共享变量可能导致判断成立后, 而返回时共
 **                享变量的值可能被其他进程或线程修改, 导致出现严重错误!
 **作    者: # Qifeng.zou # 2015.06.20 #
 ******************************************************************************/
static int lsnd_dsvr_cmd_dist_hdl(lsnd_cntx_t *ctx,
        lsnd_dsvr_t *dsvr, const cmd_dist_data_t *dist)
{
    int num, idx;
    agent_flow_t *flow;
    rtmq_header_t *head;
    void *addr[LSND_DIST_POP_NUM];

    while (1)
    {
        /* > 获取弹出个数 */
        num = MIN(shm_queue_used(ctx->distq[dist->qid]), LSND_DIST_POP_NUM);
        if (0 == num)
        {
            break;
        }

        /* > 弹出发送数据 */
        num = shm_queue_mpop(ctx->distq[dist->qid], addr, num);
        if (0 == num)
        {
            continue;
        }

        log_trace(ctx->log, "Pop data! qid:%d num:%d", dist->qid, num);

        /* > 逐条数据处理 */
        for (idx=0; idx<num; ++idx)
        {
            flow = (agent_flow_t *)addr[idx];       /* 流水信息 */
            head = (rtmq_header_t *)(flow + 1);     /* 消息头 */
            if (RTMQ_CHECK_SUM != head->checksum)   /* 校验消息头 */
            {
                assert(0);
            }

            log_debug(ctx->log, "Call %s()! type:%d len:%d", __func__, head->type, head->length);

            /* 放入发送队列 */
            agent_send(ctx->agent, head->type, flow->serial, (void *)(head+1), head->length);

            shm_queue_dealloc(ctx->distq[dist->qid], addr[idx]); /* 释放队列内存 */
        }
    }
    return LSND_OK;
}

/******************************************************************************
 **函数名称: lsnd_dsvr_cmd_data_hdl
 **功    能: 进行消息命令的处理
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发服务
 **     cmd: 命令数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 根据命令类型调用对应的处理函数
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.20 21:28:09 #
 ******************************************************************************/
static int lsnd_dsvr_cmd_data_hdl(lsnd_cntx_t *ctx, lsnd_dsvr_t *dsvr, const cmd_data_t *cmd)
{
    switch (cmd->type)
    {
        case CMD_DIST_DATA: /* 分发数据 */
        {
            const cmd_dist_data_t *dist = (const cmd_dist_data_t *)&cmd->param;

            return lsnd_dsvr_cmd_dist_hdl(ctx, dsvr, dist);
        }
        default:
        {
            log_error(ctx->log, "Unknown command type! type:%d", cmd->type);
            return LSND_OK;
        }
    }

    return LSND_OK;
}

/******************************************************************************
 **函数名称: lsnd_dsvr_event_hdl
 **功    能: 事件处理
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 接收事件通知, 并进行事件处理!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.20 21:11:50 #
 ******************************************************************************/
static int lsnd_dsvr_event_hdl(lsnd_cntx_t *ctx, lsnd_dsvr_t *dsvr)
{
    cmd_data_t cmd;

    if (FD_ISSET(dsvr->cmd_sck_id, &dsvr->rdset))
    {
        if (unix_udp_recv(dsvr->cmd_sck_id, &cmd, sizeof(cmd)) < 0)
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return LSND_ERR;
        }
        return lsnd_dsvr_cmd_data_hdl(ctx, dsvr, &cmd);
    }

    return LSND_OK;
}

/******************************************************************************
 **函数名称: lsnd_dsvr_timeout_hdl
 **功    能: 超时事件处理
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 调用分发处理接口
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.20 21:37:36 #
 ******************************************************************************/
static int lsnd_dsvr_timeout_hdl(lsnd_cntx_t *ctx, lsnd_dsvr_t *dsvr)
{
    int idx;
    cmd_dist_data_t dist;

    for (idx=0; idx<ctx->conf.distq.num; ++idx)
    {
        dist.qid = idx;

        lsnd_dsvr_cmd_dist_hdl(ctx, dsvr, &dist);
    }

    return 0;
}

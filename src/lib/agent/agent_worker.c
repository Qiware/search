#include "sck.h"
#include "redo.h"
#include "command.h"
#include "agent_mesg.h"
#include "agent_worker.h"

#define AGT_WSVR_POP_NUM    (1024) /* 一次最多弹出数据个数 */

/* 静态函数 */
static int agent_worker_event_handler(agent_cntx_t *ctx, agent_worker_t *worker);
static int agent_worker_timeout_handler(agent_cntx_t *ctx, agent_worker_t *worker);

/******************************************************************************
 **函数名称: agent_worker_self
 **功    能: 获取工作对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 工作对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
static agent_worker_t *agent_worker_self(agent_cntx_t *ctx)
{
    int id;
    agent_worker_t *worker;

    id = thread_pool_get_tidx(ctx->workers);
    if (id < 0) {
        return NULL;
    }

    worker = thread_pool_get_args(ctx->workers);

    return worker + id;
}

/******************************************************************************
 **函数名称: agent_worker_routine
 **功    能: 运行Worker线程
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 使用事件触发机制驱动业务处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
void *agent_worker_routine(void *_ctx)
{
    int max, ret;
    struct timeval timeout;
    agent_worker_t *worker;
    agent_cntx_t *ctx = (agent_cntx_t *)_ctx;

    nice(-20);

    worker = agent_worker_self(ctx);
    if (NULL == worker) {
        log_error(ctx->log, "Get worker failed!");
        return (void *)-1;
    }

    while (1) {
        FD_ZERO(&worker->rdset);

        max = worker->cmd_sck_id;
        FD_SET(worker->cmd_sck_id, &worker->rdset);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        ret = select(max+1, &worker->rdset, NULL, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; } 
            log_fatal(worker->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret) {
            agent_worker_timeout_handler(ctx, worker);
            continue;
        }

        agent_worker_event_handler(ctx, worker);
    }
    return NULL;
}

/******************************************************************************
 **函数名称: agent_worker_init
 **功    能: 初始化Worker线程
 **输入参数:
 **     ctx: 全局信息
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int agent_worker_init(agent_cntx_t *ctx, agent_worker_t *worker, int idx)
{
    char path[FILE_PATH_MAX_LEN];

    worker->id = idx;
    worker->log = ctx->log;

    /* > 创建命令套接字*/
    agent_wsvr_cmd_usck_path(ctx->conf, idx, path, sizeof(path));

    worker->cmd_sck_id = unix_udp_creat(path);
    if (worker->cmd_sck_id < 0) {
        log_error(worker->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return AGENT_ERR;
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_worker_destroy
 **功    能: 销毁Worker线程
 **输入参数:
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次释放Worker对象所创建的所有资源
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int agent_worker_destroy(agent_worker_t *worker)
{
    CLOSE(worker->cmd_sck_id);
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_worker_proc_data_hdl
 **功    能: 进行数据处理
 **输入参数:
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从队列中弹出数据, 并依次处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-26 11:06:09 #
 ******************************************************************************/
static int agent_worker_proc_data_hdl(agent_cntx_t *ctx, agent_worker_t *worker)
{
    int i, num, idx, rqid; /* rqid: 接收队列ID */
    agent_reg_t *reg;
    agent_flow_t *flow;
    agent_header_t *head;
    void *addr[AGT_WSVR_POP_NUM];
    agent_conf_t *conf = ctx->conf;

    /* 遍历接收队列 */
    rqid = rand(); /* 随机选择开始队列 */
    for (i=0; i<conf->agent_num; ++i, ++rqid) {
        rqid %= conf->agent_num;

        /* 计算弹出个数(WARN: 千万勿将共享变量参与"?:"三目运算, 否则可能出现严重错误!!!!且很难找出原因!) */
        num = MIN(queue_used(ctx->recvq[rqid]), AGT_WSVR_POP_NUM);
        if (0 == num) {
            continue;
        }

        /* > 从队列中取数据 */
        num = queue_mpop(ctx->recvq[rqid], addr, num);
        if (0 == num) {
            continue;
        }

        /* > 依次处理数据 */
        for (idx=0; idx<num; ++idx) {
            /* > 对数据进行处理 */
            flow = (agent_flow_t *)addr[idx];
            head = (agent_header_t *)(flow + 1);

            reg = &ctx->reg[head->type];

            /* > 插入SERIAL->SCK映射 */
            if (agent_serial_to_sck_map_insert(ctx, flow)) {
                log_error(worker->log, "Insert serial to sck map failed! serial:%lu sid:%lu",
                          flow->serial, flow->sid);
                continue;
            }

            /* > 调用处理回调 */
            if (reg->proc(head->type,
                    addr[idx] + sizeof(agent_flow_t),
                    head->length + sizeof(agent_header_t), reg->args))
            {
                agent_serial_to_sck_map_delete(ctx, flow->serial);
            }

            /* 3. 释放内存空间 */
            queue_dealloc(ctx->recvq[rqid], addr[idx]);
        }
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_worker_event_handler
 **功    能: 事件处理
 **输入参数:
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-26 11:06:09 #
 ******************************************************************************/
static int agent_worker_event_handler(agent_cntx_t *ctx, agent_worker_t *worker)
{
    cmd_data_t cmd;

    if (!FD_ISSET(worker->cmd_sck_id, &worker->rdset)) {
        return AGENT_ERR;
    }

    /* > 接收命令信息 */
    if (unix_udp_recv(worker->cmd_sck_id, &cmd, sizeof(cmd)) < 0) {
        log_error(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    /* > 进行命令处理 */
    switch (cmd.type) {
        case CMD_PROC_DATA:
        {
            return agent_worker_proc_data_hdl(ctx, worker);
        }
        default:
        {
            log_error(worker->log, "Unknown command type [%d]!", cmd.type);
            return AGENT_ERR;
        }
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_worker_timeout_handler
 **功    能: 超时处理
 **输入参数:
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-26 11:06:02 #
 ******************************************************************************/
static int agent_worker_timeout_handler(agent_cntx_t *ctx, agent_worker_t *worker)
{
    return agent_worker_proc_data_hdl(ctx, worker);
}



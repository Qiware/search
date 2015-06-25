#include "syscall.h"
#include "agent_mesg.h"
#include "agent_worker.h"

#define AGT_WSVR_POP_NUM    (128)

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
    int tidx;
    agent_worker_t *worker;

    tidx = thread_pool_get_tidx(ctx->workers);
    if (tidx < 0)
    {
        return NULL;
    }

    worker = thread_pool_get_args(ctx->workers);

    return worker + tidx;
}

/******************************************************************************
 **函数名称: agent_worker_routine
 **功    能: 运行Worker线程
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: TODO: 后续改成事件触发机制 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
void *agent_worker_routine(void *_ctx)
{
    int num, idx, rqid; /* rqid: 接收队列ID */
    agent_reg_t *reg;
    agent_flow_t *flow;
    agent_worker_t *worker;
    agent_header_t *head;
    void *addr[AGT_WSVR_POP_NUM];
    agent_cntx_t *ctx = (agent_cntx_t *)_ctx;

    worker = agent_worker_self(ctx);

    while (1)
    {
        rqid = rand() % ctx->conf->agent_num;
        num = MIN(AGT_WSVR_POP_NUM, queue_used(ctx->recvq[rqid]));
        if (0 == num)
        {
            usleep(50);
            continue;
        }

        /* > 从队列中取数据 */
        num = queue_mpop(ctx->recvq[rqid], addr, num);
        if (0 == num)
        {
            continue;
        }

        /* > 依次处理数据 */
        for (idx=0; idx<num; ++idx)
        {
            /* > 对数据进行处理 */
            flow = (agent_flow_t *)addr[idx];
            head = (agent_header_t *)(flow + 1);

            reg = &ctx->reg[head->type];

            /* > 插入SERIAL->SCK映射 */
            if (agent_serial_to_sck_map_insert(ctx, flow))
            {
                log_error(worker->log, "Insert serial to sck map failed! serial:%lu sck_serial:%lu",
                        flow->serial, flow->sck_serial);
                continue;
            }

            /* > 调用处理回调 */
            if (reg->proc(head->type, addr[idx], head->length + sizeof(agent_flow_t), reg->args))
            {
                agent_serial_to_sck_map_delete(ctx, flow->serial);
            }

            /* 3. 释放内存空间 */
            queue_dealloc(ctx->recvq[rqid], addr[idx]);
        }
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
    worker->log = ctx->log;
    worker->tidx = idx;
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_worker_destroy
 **功    能: 销毁Worker线程
 **输入参数:
 **     worker: 工作对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int agent_worker_destroy(agent_worker_t *worker)
{
    return AGENT_OK;
}

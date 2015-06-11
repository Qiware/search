#include "agent.h"
#include "syscall.h"
#include "agent_mesg.h"

/******************************************************************************
 **函数名称: agent_serial_to_sck_map_init
 **功    能: 初始化请求列表
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项: TODO: 使用内存池替代操作系统内存分配机制
 **作    者: # Qifeng.zou # 2015.06.04 18:31:34 #
 ******************************************************************************/
int agent_serial_to_sck_map_init(agent_cntx_t *ctx)
{
#define SERIAL_TO_SCK_MAP_LEN  (10)
    int i;
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    ctx->serial_to_sck_map_len = SERIAL_TO_SCK_MAP_LEN;
    ctx->serial_to_sck_map = (avl_tree_t **)calloc(ctx->serial_to_sck_map_len, sizeof(avl_tree_t *));
    if (NULL == ctx->serial_to_sck_map)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    ctx->serial_to_sck_map_lock = (spinlock_t *)calloc(ctx->serial_to_sck_map_len, sizeof(spinlock_t));
    if (NULL == ctx->serial_to_sck_map_lock)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        FREE(ctx->serial_to_sck_map);
        return AGENT_ERR;
    }

    for (i=0; i<ctx->serial_to_sck_map_len; ++i)
    {
        opt.pool = (void *)NULL;
        opt.alloc = (mem_alloc_cb_t)mem_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

        ctx->serial_to_sck_map[i] = avl_creat(&opt, (key_cb_t)avl_key_cb_int64, (avl_cmp_cb_t)avl_cmp_cb_int64);
        if (NULL == ctx->serial_to_sck_map[i])
        {
            log_error(ctx->log, "Create avl failed!");
            FREE(ctx->serial_to_sck_map);
            FREE(ctx->serial_to_sck_map_lock);
            return AGENT_ERR;
        }
        spin_lock_init(&ctx->serial_to_sck_map_lock[i]);
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_serial_to_sck_map_insert
 **功    能: 插入流水->SCK的映射
 **输入参数:
 **     ctx: 全局对象
 **     flow: 流水信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
int agent_serial_to_sck_map_insert(agent_cntx_t *ctx, agent_flow_t *_flow)
{
    int idx;
    agent_flow_t *flow;

    /* > 申请内存空间 */
    flow = (agent_flow_t *)calloc(1, sizeof(agent_flow_t));
    if (NULL == flow)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    flow->serial = _flow->serial;
    flow->agt_idx = _flow->agt_idx;
    flow->sck_serial = _flow->sck_serial;

    idx = flow->serial % ctx->serial_to_sck_map_len;

    /* > 插入流水->SCK映射 */
    spin_lock(&ctx->serial_to_sck_map_lock[idx]);

    if (avl_insert(ctx->serial_to_sck_map[idx], &flow->serial, sizeof(flow->serial), flow))
    {
        spin_unlock(&ctx->serial_to_sck_map_lock[idx]);
        free(flow);
        log_error(ctx->log, "Insert into avl failed! idx:%d serial:%lu sck_serial:%lu agt_idx:%d",
                idx, flow->serial, flow->sck_serial, flow->agt_idx);
        return AGENT_ERR;
    }

    spin_unlock(&ctx->serial_to_sck_map_lock[idx]);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_serial_to_sck_map_delete
 **功    能: 删除流水->SCK的映射
 **输入参数:
 **     ctx: 全局对象
 **     serial: 流水号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
int agent_serial_to_sck_map_delete(agent_cntx_t *ctx, uint64_t serial)
{
    int idx;
    agent_flow_t *flow;

    idx = serial % ctx->serial_to_sck_map_len;

    spin_lock(&ctx->serial_to_sck_map_lock[idx]);

    avl_delete(ctx->serial_to_sck_map[idx], &serial, sizeof(serial), (void **)&flow); 
    if (NULL == flow)
    {
        spin_unlock(&ctx->serial_to_sck_map_lock[idx]);
        log_error(ctx->log, "Delete serial to sck map failed! idx:%d serial:%lu", idx, serial);
        return AGENT_ERR;
    }

    spin_unlock(&ctx->serial_to_sck_map_lock[idx]);

    log_trace(ctx->log, "idx:%d serial:%lu sck_serial:%lu", idx, serial, flow->serial);

    free(flow);
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_serial_to_sck_map_query
 **功    能: 查找流水->SCK的映射
 **输入参数:
 **     ctx: 全局对象
 **     serial: 流水号
 **输出参数:
 **     flow: 流水->SCK映射
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
int agent_serial_to_sck_map_query(agent_cntx_t *ctx, uint64_t serial, agent_flow_t *flow)
{
    int idx;
    avl_node_t *node;

    idx = serial % ctx->serial_to_sck_map_len;

    spin_lock(&ctx->serial_to_sck_map_lock[idx]);

    node = avl_query(ctx->serial_to_sck_map[idx], &serial, sizeof(serial)); 
    if (NULL == node)
    {
        spin_unlock(&ctx->serial_to_sck_map_lock[idx]);
        log_error(ctx->log, "Query serial to sck map failed! idx:%d serial:%lu", idx, serial);
        return AGENT_ERR;
    }

    memcpy(flow, node->data, sizeof(agent_flow_t));

    spin_unlock(&ctx->serial_to_sck_map_lock[idx]);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_send
 **功    能: 发送数据(外部接口)
 **输入参数:
 **     ctx: 全局对象
 **     type: 数据类型
 **     serial: 流水号
 **     data: 数据内容
 **     len: 数据长度
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 将数据放入发送队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-04 #
 ******************************************************************************/
int agent_send(agent_cntx_t *ctx, int type, uint64_t serial, void *data, int len)
{
    int size;
    void *addr;
    queue_t *sendq;
    agent_flow_t flow;
    agent_header_t *head;

    /* > 查找SERIAL->SCK映射 */
    if (agent_serial_to_sck_map_query(ctx, serial, &flow))
    {
        log_error(ctx->log, "Query serial->sck map failed! serial:%lu", serial);
        return AGENT_ERR;
    }

    sendq = ctx->sendq[flow.agt_idx];
    size = sizeof(flow) + sizeof(agent_header_t) + len;
    if (size > queue_size(sendq))
    {
        log_error(ctx->log, "Queue size is too small! size:%d/%d", size, queue_size(sendq));
        return AGENT_ERR;
    }

    /* > 放入指定发送队列 */
    addr = queue_malloc(sendq);
    if (NULL == addr)
    {
        return AGENT_ERR;
    }

    memcpy(addr, &flow, sizeof(flow));
    head = (agent_header_t *)(addr + sizeof(flow));

    head->type = type;
    head->flag = AGENT_MSG_FLAG_USR;
    head->length = len;
    head->mark = AGENT_MSG_MARK_KEY;

    memcpy(head+1, data, len);

    queue_push(sendq, addr);

    return AGENT_OK;
}

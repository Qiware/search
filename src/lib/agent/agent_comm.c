#include "sck.h"
#include "agent.h"
#include "command.h"
#include "syscall.h"
#include "agent_mesg.h"

static int agent_cmd_send_dist_req(agent_cntx_t *ctx, int idx);

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
#define SERIAL_TO_SCK_MAP_LEN  (33)
    int i;
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    ctx->serial_to_sck_map_len = SERIAL_TO_SCK_MAP_LEN;
    ctx->serial_to_sck_map = (avl_tree_t **)slab_alloc(ctx->slab,
                                ctx->serial_to_sck_map_len*sizeof(avl_tree_t *));
    if (NULL == ctx->serial_to_sck_map)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    ctx->serial_to_sck_map_lock = (spinlock_t *)slab_alloc(ctx->slab,
                                       ctx->serial_to_sck_map_len*sizeof(spinlock_t));
    if (NULL == ctx->serial_to_sck_map_lock)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        slab_dealloc(ctx->slab, ctx->serial_to_sck_map);
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
            slab_dealloc(ctx->slab, ctx->serial_to_sck_map);
            slab_dealloc(ctx->slab, ctx->serial_to_sck_map_lock);
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
    flow->create_tm = _flow->create_tm;
    flow->agt_idx = _flow->agt_idx;
    flow->sck_seq = _flow->sck_seq;

    idx = flow->serial % ctx->serial_to_sck_map_len;

    /* > 插入流水->SCK映射 */
    spin_lock(&ctx->serial_to_sck_map_lock[idx]);

    if (avl_insert(ctx->serial_to_sck_map[idx], &flow->serial, sizeof(flow->serial), flow))
    {
        spin_unlock(&ctx->serial_to_sck_map_lock[idx]);
        free(flow);
        log_error(ctx->log, "Insert into avl failed! idx:%d serial:%lu sck_seq:%lu agt_idx:%d",
                idx, flow->serial, flow->sck_seq, flow->agt_idx);
        return AGENT_ERR;
    }

    spin_unlock(&ctx->serial_to_sck_map_lock[idx]);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: _agent_serial_to_sck_map_delete
 **功    能: 删除流水->SCK的映射
 **输入参数:
 **     ctx: 全局对象
 **     serial: 流水号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 在多线程操作的情况下，请在外部加锁
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
int _agent_serial_to_sck_map_delete(agent_cntx_t *ctx, uint64_t serial)
{
    int idx;
    agent_flow_t *flow;

    idx = serial % ctx->serial_to_sck_map_len;

    avl_delete(ctx->serial_to_sck_map[idx], &serial, sizeof(serial), (void **)&flow); 
    if (NULL == flow)
    {
        log_error(ctx->log, "Delete serial to sck map failed! idx:%d serial:%lu", idx, serial);
        return AGENT_ERR;
    }

    log_trace(ctx->log, "idx:%d serial:%lu sck_seq:%lu", idx, serial, flow->serial);

    free(flow);
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

    log_trace(ctx->log, "idx:%d serial:%lu sck_seq:%lu", idx, serial, flow->sck_seq);

    free(flow);
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_serial_get_timeout_list
 **功    能: 将超时连接加入链表
 **输入参数: 
 **     node: 平衡二叉树结点
 **     timeout: 超时链表
 **输出参数: NONE
 **返    回: 代理对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.11 15:36:36 #
 ******************************************************************************/
static int agent_serial_get_timeout_list(agent_flow_t *flow, agent_conn_timeout_list_t *timeout)
{
#define AGENT_SERIAL_TIMEOUT_SEC (30)

    /* 判断是否超时，则加入到timeout链表中 */
    if (timeout->ctm - flow->create_tm <= AGENT_SERIAL_TIMEOUT_SEC)
    {
        return AGENT_OK; /* 未超时 */
    }

    return list_lpush(timeout->list, flow);
}

/******************************************************************************
 **函数名称: agent_serial_to_sck_map_timeout
 **功    能: 删除超时流水->SCK的映射
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.11 #
 ******************************************************************************/
int agent_serial_to_sck_map_timeout(agent_cntx_t *ctx)
{
    int idx;
    list_opt_t opt;
    agent_flow_t *flow;
    time_t ctm = time(NULL);
    agent_conn_timeout_list_t timeout;

    for (idx=0; idx<ctx->serial_to_sck_map_len; ++idx)
    {
        if (avl_isempty(ctx->serial_to_sck_map[idx]))
        {
            continue;
        }

        memset(&timeout, 0, sizeof(timeout));

        /* > 创建内存池 */
        timeout.pool = mem_pool_creat(1 * KB);
        if (NULL == timeout.pool)
        {
            log_error(ctx->log, "Create memory pool failed!");
            return AGENT_ERR;
        }

        timeout.ctm = ctm;

        /* > 创建链表 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)timeout.pool;
        opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

        timeout.list = list_creat(&opt);
        if (NULL == timeout.list)
        {
            log_error(ctx->log, "Create list failed!");
            mem_pool_destroy(timeout.pool);
            break;
        }

        /* > 获取超时流水 */
        spin_lock(&ctx->serial_to_sck_map_lock[idx]);

        avl_trav(ctx->serial_to_sck_map[idx],
             (avl_trav_cb_t)agent_serial_get_timeout_list, &timeout);

        /* > 删除超时连接 */
        for (;;)
        {
            flow = (agent_flow_t *)list_lpop(timeout.list);
            if (NULL == flow)
            {
                break;
            }

            log_trace(ctx->log, "Delete timeout serial:%lu!", flow->serial);

            _agent_serial_to_sck_map_delete(ctx, flow->serial);
        }

        spin_unlock(&ctx->serial_to_sck_map_lock[idx]);

        /* > 释放内存空间 */
        mem_pool_destroy(timeout.pool);
    }

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
 **返    回: 发送队列的索引
 **实现描述: 将数据放入发送队列
 **注意事项: 内存结构: 流水信息 + 消息头 + 消息体
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

    size = sizeof(flow) + sizeof(agent_header_t) + len;

    /* > 放入指定发送队列 */
    sendq = ctx->sendq[flow.agt_idx];

    addr = queue_malloc(sendq, size);
    if (NULL == addr)
    {
        log_error(ctx->log, "Alloc from queue failed! size:%d/%d", size, queue_size(sendq));
        return AGENT_ERR;
    }

    memcpy(addr, &flow, sizeof(flow));
    head = (agent_header_t *)(addr + sizeof(flow));

    head->type = type;
    head->flag = AGENT_MSG_FLAG_USR;
    head->length = len;
    head->mark = AGENT_MSG_MARK_KEY;

    memcpy(head+1, data, len);

    queue_push(sendq, addr); /* 放入队列 */

    agent_cmd_send_dist_req(ctx, flow.agt_idx); /* 发送分发命令 */

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_cmd_send_dist_req
 **功    能: 发送分发命令给指定的代理服务
 **输入参数:
 **     ctx: 全局对象
 **     idx: 代理服务的索引
 **输出参数:
 **返    回: >0:成功 <=0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-24 23:55:45 #
 ******************************************************************************/
static int agent_cmd_send_dist_req(agent_cntx_t *ctx, int idx)
{
    cmd_data_t cmd;
    char path[FILE_PATH_MAX_LEN];
    agent_conf_t *conf = ctx->conf;

    cmd.type = CMD_DIST_DATA;
    agent_rsvr_cmd_usck_path(conf, idx, path, sizeof(path));

    return unix_udp_send(ctx->cli.cmd_sck_id, path, (void *)&cmd, sizeof(cmd));
}

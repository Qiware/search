#include "sck.h"
#include "redo.h"
#include "mesg.h"
#include "agent.h"
#include "command.h"
#include "syscall.h"
#include "agent_rsvr.h"

static int agent_cmd_send_dist_req(agent_cntx_t *ctx, int idx);

/******************************************************************************
 **函数名称: agent_send
 **功    能: 发送数据(外部接口)
 **输入参数:
 **     ctx: 全局对象
 **     type: 数据类型
 **     sid: 会话ID
 **     data: 数据内容
 **     len: 数据长度
 **输出参数:
 **返    回: 发送队列的索引
 **实现描述: 将数据放入发送队列
 **注意事项: 内存结构: 流水信息 + 消息头 + 消息体
 **作    者: # Qifeng.zou # 2015-06-04 #
 ******************************************************************************/
int agent_send(agent_cntx_t *ctx, int type, uint64_t sid, void *data, int len)
{
    int aid; // aid: 代理服务ID
    void *addr;
    queue_t *sendq;
    mesg_header_t *head;
    socket_t *sck;
    agent_socket_extra_t *extra;

    /* > 通过sid获取服务ID */
    spin_lock(&ctx->connections.lock);
    sck = rbt_query(ctx->connections.list, &sid, sizeof(sid));
    if (NULL == sck) {
        log_error(ctx->log, "Query socket by sid failed! sid:%lu", sid);
        return -1;
    }
    extra = (agent_socket_extra_t *)sck->extra;
    aid = extra->id;
    spin_unlock(&ctx->connections.lock);

    /* > 放入指定发送队列 */
    sendq = ctx->sendq[aid];

    addr = queue_malloc(sendq, sizeof(mesg_header_t) + len);
    if (NULL == addr) {
        log_error(ctx->log, "Alloc from queue failed! size:%d/%d",
                sizeof(mesg_header_t) + len, queue_size(sendq));
        return AGENT_ERR;
    }

    head = (mesg_header_t *)addr;

    head->type = type;
    head->flag = MSG_FLAG_USR;
    head->length = len;
    head->chksum = MSG_CHKSUM_VAL;
    head->sid = sid;
    head->serial = 0;//serial;

    memcpy(head+1, data, len);

    queue_push(sendq, addr); /* 放入队列 */

    agent_cmd_send_dist_req(ctx, aid); /* 发送分发命令 */

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

    return unix_udp_send(ctx->cmd_sck_id, path, (void *)&cmd, sizeof(cmd));
}

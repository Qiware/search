/******************************************************************************
 ** Coypright(C) 2014-2024 Toushi technology Co., Ltd
 **
 ** 文件名: frwd_mesg.c
 ** 版本号: 1.0
 ** 描  述: 消息处理函数定义
 ** 作  者: # Qifeng.zou # Tue 14 Jul 2015 02:52:16 PM CST #
 ******************************************************************************/
#include "frwd.h"
#include "mesg.h"
#include "agent.h"
#include "command.h"

static int frwd_search_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args);
static int frwd_insert_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

/******************************************************************************
 **函数名称: frwd_set_reg
 **功    能: 注册处理回调
 **输入参数:
 **     frwd: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_set_reg(frwd_cntx_t *frwd)
{
#define FRWD_REG_CB(frwd, type, proc, args) \
    if (rtsd_register((frwd)->rtmq, type, (rtmq_reg_cb_t)proc, (void *)args)) \
    { \
        log_error((frwd)->log, "Register type [%d] failed!", type); \
        return FRWD_ERR; \
    }

    FRWD_REG_CB(frwd, MSG_SEARCH_WORD_RSP, frwd_search_word_rsp_hdl, frwd);
    FRWD_REG_CB(frwd, MSG_INSERT_WORD_RSP, frwd_insert_word_rsp_hdl, frwd);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_shmq_push
 **功    能: 将数据转发到指定SHMQ队列
 **输入参数:
 **     shmq: SHM队列
 **     serial: 流水号
 **     type: 数据类型
 **     orig: 源结点ID
 **     data: 需要转发的数据
 **     len: 数据长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 内存结构: 流水信息 + 消息头 + 消息体
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
static int frwd_shmq_push(shm_queue_t *shmq,
        uint64_t serial, int type, int orig, char *data, size_t len)
{
    void *addr;
    size_t size;
    agent_flow_t *flow;
    rtmq_header_t *head;

    size = sizeof(agent_flow_t) + sizeof(rtmq_header_t) + len;

    /* > 申请队列空间 */
    addr = shm_queue_malloc(shmq, size);
    if (NULL == addr)
    {
        return FRWD_ERR;
    }

    /* > 设置应答数据 */
    flow = (agent_flow_t *)addr;
    head = (rtmq_header_t *)(flow + 1);

    flow->serial = serial;
    head->type = type;
    head->nodeid = orig;
    head->length = len;
    head->flag = RTMQ_EXP_MESG;
    head->checksum = RTMQ_CHECK_SUM;

    memcpy(head+1, data, len);

    if (shm_queue_push(shmq, addr))
    {
        shm_queue_dealloc(shmq, addr);
        return FRWD_ERR;
    }

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_cmd_send_to_lsnd
 **功    能: 将数据发送至侦听服务
 **输入参数:
 **     ctx: 全局对象
 **     serial: 流水号
 **     type: 数据类型
 **     orig: 源结点ID
 **     data: 需要转发的数据
 **     len: 数据长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 内存结构: 流水信息 + 消息头 + 消息体
 **作    者: # Qifeng.zou # 2015-06-22 09:07:01 #
 ******************************************************************************/
static int frwd_cmd_send_to_lsnd(frwd_cntx_t *ctx,
     uint64_t serial, int type, int orig, char *data, size_t len)
{
    cmd_data_t cmd;
    cmd_dist_data_t *dist = (cmd_dist_data_t *)&cmd.param;

    dist->qid = rand()%ctx->lsnd.distq_num;

    if (frwd_shmq_push(ctx->lsnd.distq[dist->qid], serial, type, orig, data, len))
    {
        log_error(ctx->log, "Push into SHMQ failed!");
        return FRWD_ERR;
    }

    cmd.type = CMD_DIST_DATA;
    unix_udp_send(ctx->cmd_sck_id, ctx->lsnd.dist_cmd_path, &cmd, sizeof(cmd));

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_search_word_rsp_hdl
 **功    能: 搜索关键字应答处理
 **输入参数:
 **     type: 数据类型
 **     orig: 源结点ID
 **     data: 需要转发的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
static int frwd_search_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args)
{
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_search_word_rsp_t *rsp = (mesg_search_word_rsp_t *)data;

    log_trace(ctx->log, "Call %s()", __func__);

    return frwd_cmd_send_to_lsnd(ctx, ntoh64(rsp->serial), type, orig, data, len);
}

/******************************************************************************
 **函数名称: frwd_search_word_rsp_hdl
 **功    能: 插入关键字的应答
 **输入参数:
 **     type: 数据类型
 **     orig: 源结点ID
 **     data: 需要转发的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
static int frwd_insert_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args)
{
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_insert_word_rsp_t *rsp = (mesg_insert_word_rsp_t *)data;

    log_trace(ctx->log, "Call %s()", __func__);

    return frwd_cmd_send_to_lsnd(ctx, ntoh64(rsp->serial), type, orig, data, len);
}

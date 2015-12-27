/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: lsnd_mesg.c
 ** 版本号: 1.0
 ** 描  述: 侦听相关的消息处理函数的定义
 ** 作  者: # Qifeng.zou # Thu 16 Jul 2015 01:08:20 AM CST #
 ******************************************************************************/

#include "mesg.h"
#include "agent.h"
#include "listend.h"
#include "lsnd_mesg.h"
#include "agent_mesg.h"

/******************************************************************************
 **函数名称: lsnd_search_word_req_hdl
 **功    能: 搜索请求的处理函数
 **输入参数:
 **     type: 全局对象
 **     data: 数据内容
 **     length: 数据长度(报头 + 报体)
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 请求数据的内存结构: 流水信息 + 消息头 + 消息体
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
int lsnd_search_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    const char *str;
    agent_header_t *head;
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)args;

    head = (agent_header_t *)data;      // 消息头
    str = (const char *)(head + 1);     // 消息体

    head->type = htonl(head->type);
    head->flag = htonl(head->flag);
    head->length = htonl(head->length);
    head->mark = htonl(head->mark);
    head->serial = hton64(head->serial);

    log_debug(ctx->log, "Call %s() serial:%lu length:%d body:%s!",
            __func__, ntoh64(head->serial), length, str);

    /* > 转发搜索请求 */
    return rtsd_cli_send(ctx->rtmq_to_invtd, type, data, length);
}

/******************************************************************************
 **函数名称: lsnd_insert_word_req_hdl
 **功    能: 插入关键字的处理函数
 **输入参数:
 **     type: 全局对象
 **     data: 数据内容
 **     length: 数据长度
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 请求数据的内存结构: 流水信息 + 消息头 + 消息体
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.17 21:34:49 #
 ******************************************************************************/
int lsnd_insert_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    agent_header_t *head;
    mesg_insert_word_req_t *req;
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)args;

    log_debug(ctx->log, "Call %s()!", __func__);

    head = (agent_header_t *)data; // 消息头
    req = (mesg_insert_word_req_t *)(head + 1); // 消息体

    /* > 转发搜索请求 */
    req->serial = head->serial;

    return rtsd_cli_send(ctx->rtmq_to_invtd, type, req, sizeof(mesg_insert_word_req_t));
}

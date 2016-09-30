/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: lwsd_mesg.c
 ** 版本号: 1.0
 ** 描  述: 侦听相关的消息处理函数的定义
 ** 作  者: # Qifeng.zou # Thu 16 Jul 2015 01:08:20 AM CST #
 ******************************************************************************/

#include "mesg.h"
#include "lwsd.h"
#include "lwsd_mesg.h"
#include "lwsd_search.h"

/******************************************************************************
 **函数名称: lwsd_search_req_hdl
 **功    能: 搜索请求的处理函数
 **输入参数:
 **     type: 全局对象
 **     data: 数据内容
 **     length: 数据长度(报头 + 报体)
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 请求数据的内存结构: 流水信息 + 消息头 + 消息体
 **注意事项: 需要将协议头转换为网络字节序
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
int lwsd_search_req_hdl(unsigned int type, void *data, int length, void *args)
{
    lwsd_cntx_t *ctx = (lwsd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data; /* 消息头 */

    log_debug(ctx->log, "serial:%lu length:%d body:%s!", head->serial, length, head->body);

    /* > 转换字节序 */
    MESG_HEAD_HTON(head, head);

    /* > 转发搜索请求 */
    return rtmq_proxy_async_send(ctx->frwder, type, data, length);
}

/******************************************************************************
 **函数名称: lwsd_search_rsp_hdl
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
int lwsd_search_rsp_hdl(int type, int orig, char *data, size_t len, void *args)
{
    lwsd_cntx_t *ctx = (lwsd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;

    /* > 转化字节序 */
    MESG_HEAD_NTOH(head, head);

    log_trace(ctx->log, "body:%s", head->body);

    return lwsd_search_async_send(ctx, head->sid, data, len);
}

/******************************************************************************
 **函数名称: lwsd_insert_word_req_hdl
 **功    能: 插入关键字的处理函数
 **输入参数:
 **     type: 全局对象
 **     data: 数据内容
 **     length: 数据长度
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 请求数据的内存结构: 流水信息 + 消息头 + 消息体
 **注意事项: 需要将协议头转换为网络字节序
 **作    者: # Qifeng.zou # 2015.06.17 21:34:49 #
 ******************************************************************************/
int lwsd_insert_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    mesg_header_t *head;
    mesg_insert_word_req_t *req;
    lwsd_cntx_t *ctx = (lwsd_cntx_t *)args;

    head = (mesg_header_t *)data; // 消息头
    req = (mesg_insert_word_req_t *)(head + 1);

    log_debug(ctx->log, "serial:%lu word:%s url:%s freq:%d",
            head->serial, req->word, req->url, ntohl(req->freq));

    /* > 转换字节序 */
    MESG_HEAD_HTON(head, head);

    return rtmq_proxy_async_send(ctx->frwder, type, data, length);
}

/******************************************************************************
 **函数名称: lwsd_search_rsp_hdl
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
int lwsd_insert_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args)
{
    lwsd_cntx_t *ctx = (lwsd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;
    mesg_insert_word_rsp_t *rsp = (mesg_insert_word_rsp_t *)(head + 1);

    log_debug(ctx->log, "type:%d len:%d word:%s", type, len, rsp->word);

    /* > 转换字节序 */
    MESG_HEAD_NTOH(head, head);

    /* > 放入发送队列 */
    return lwsd_search_async_send(ctx, head->sid, data, len);
}

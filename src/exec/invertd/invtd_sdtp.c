/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invtd_sdtp.c
 ** 版本号: 1.0
 ** 描  述: 倒排服务中与SDTP相关内容的处理
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 10:37:21 PM CST #
 ******************************************************************************/

#include "mesg.h"
#include "invertd.h"
#include "sdtp_recv.h"

/******************************************************************************
 **函数名称: invtd_search_req_hdl
 **功    能: 处理搜索请求
 **输入参数:
 **     type: 消息类型
 **     orig: 源设备ID
 **     buff: 搜索请求的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: TODO: 将搜索结果返回给客户端
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_search_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    int idx;
    list_node_t *node;
    invt_word_doc_t *doc;
    invt_dic_word_t *word;
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;
    mesg_search_req_t *req = (mesg_search_req_t *)buff; /* 请求 */
    mesg_search_rep_t rep; /* 应答 */

    /* > 搜索倒排表 */
    word = invert_tab_query(ctx->tab, req->body.words);
    if (NULL == word
        || NULL == word->doc_list)
    {
        log_debug(ctx->log, "Didn't find anything!");
        return 0;
    }

    /* > 打印搜索结果 */
    idx = 0;
    node = word->doc_list->head;
    for (; NULL!=node && idx < MSG_SRCH_REP_URL_NUM; node=node->next, ++idx)
    {
        doc = (invt_word_doc_t *)node->data;

        log_trace(ctx->log, "[%d]: url:%s freq:%d", idx+1, doc->url.str, doc->freq);

        snprintf(rep.url[idx], sizeof(rep.url[idx]), "%s:%d", doc->url.str, doc->freq);
    }

    /* > 应答搜索结果 */
    rep.serial = req->serial;
    if (sdtp_rcli_send(ctx->sdtp_rcli, MSG_SEARCH_REP, orig, (void *)&rep, sizeof(rep)))
    {
        log_error(ctx->log, "Send response failed! serial:%ld words:%s",
                req->serial, req->body.words);
    }

    return 0;
}

/******************************************************************************
 **函数名称: invtd_print_invt_tab_req_hdl
 **功    能: 处理打印倒排表的请求
 **输入参数:
 **     type: 消息类型
 **     orig: 源设备ID
 **     buff: 搜索请求的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_print_invt_tab_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    return 0;
}

/******************************************************************************
 **函数名称: invtd_sdtp_reg
 **功    能: 注册SDTP回调
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_sdtp_reg(invtd_cntx_t *ctx)
{
#define INVTD_SDTP_REG(sdtp, type, proc, args) \
    if (sdtp_recv_register(sdtp, type, proc, args)) \
    { \
        log_error(ctx->log, "Register callback failed!"); \
        return INVT_ERR; \
    }

   INVTD_SDTP_REG(ctx->sdtp, MSG_SEARCH_REQ, invtd_search_req_hdl, ctx);
   INVTD_SDTP_REG(ctx->sdtp, MSG_PRINT_INVT_TAB_REQ, invtd_print_invt_tab_req_hdl, ctx);

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_start_sdtp
 **功    能: 启动SDTP服务
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册, 并启动SDTP服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_start_sdtp(invtd_cntx_t *ctx)
{
    if (invtd_sdtp_reg(ctx))
    {
        return INVT_ERR;
    }

    return sdtp_recv_startup(ctx->sdtp);
}

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
#include "rtrd_recv.h"

/******************************************************************************
 **函数名称: invtd_search_word_req_hdl
 **功    能: 处理搜索请求
 **输入参数:
 **     type: 消息类型
 **     orig: 源设备ID
 **     buff: 搜索关键字-请求数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从倒排表中查询结果，并将结果返回给客户端
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_search_word_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    int idx;
    list_node_t *node;
    invt_word_doc_t *doc;
    invt_dic_word_t *word;
    mesg_search_word_rep_t rep; /* 应答 */
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;
    mesg_search_word_req_t *req = (mesg_search_word_req_t *)buff; /* 请求 */

    log_trace(ctx->log, "Call %s()! serial:%ld words:%s",
            __func__, req->serial, req->words);

    memset(&rep, 0, sizeof(rep));

    /* > 搜索倒排表 */
    word = invtab_query(ctx->tab, req->words);
    if (NULL == word
        || NULL == word->doc_list)
    {
        log_error(ctx->log, "Didn't search anything! words:%s", req->words);
        goto INVTD_SRCH_REP;
    }

    /* > 打印搜索结果 */
    idx = 0;
    node = word->doc_list->head;
    for (; NULL!=node && idx < MSG_SRCH_REP_URL_NUM; node=node->next, ++idx)
    {
        doc = (invt_word_doc_t *)node->data;

        log_trace(ctx->log, "[%d]: url:%s freq:%d", idx+1, doc->url.str, doc->freq);

        ++rep.url_num;
        snprintf(rep.url[idx], sizeof(rep.url[idx]), "%s:%d", doc->url.str, doc->freq);
    }

INVTD_SRCH_REP:
    /* > 应答搜索结果 */
    rep.serial = req->serial;
    if (rtrd_cli_send(ctx->rtrd_cli, MSG_SEARCH_WORD_REP, orig, (void *)&rep, sizeof(rep)))
    {
        log_error(ctx->log, "Send response failed! serial:%ld words:%s",
                req->serial, req->words);
    }

    return 0;
}

/******************************************************************************
 **函数名称: invtd_search_word_req_hdl
 **功    能: 插入关键字的处理
 **输入参数:
 **     type: 消息类型
 **     orig: 源节点ID
 **     buff: 插入关键字-请求数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 源节点ID(orig)将成为应答消息的目的节点ID(dest)
    **作    者: # Qifeng.zou # 2015-06-17 21:37:55 #
 ******************************************************************************/
static int invtd_insert_word_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    mesg_insert_word_rep_t rep; /* 应答 */
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;
    mesg_insert_word_req_t *req = (mesg_insert_word_req_t *)buff; /* 请求 */

    log_trace(ctx->log, "Call %s()! serial:%ld word:%s url:%s",
            __func__, req->serial, req->word, req->url);

    memset(&rep, 0, sizeof(rep));

    req->serial = req->serial;
    req->freq = req->freq;

    /* > 插入倒排表 */
    if (invtab_insert(ctx->tab, req->word, req->url, req->freq))
    {
        log_error(ctx->log, "Insert invert table failed! serial:%s word:%s url:%s freq:%d",
                req->serial, req->word, req->url, req->freq);
        /* > 设置应答信息 */
        rep.code = 0; // 失败
        snprintf(rep.word, sizeof(rep.word), "%s", req->word);
        goto INVTD_INSERT_WORD_REP;
    }

    /* > 设置应答信息 */
    rep.code = 1; // 成功
    snprintf(rep.word, sizeof(rep.word), "%s", req->word);

INVTD_INSERT_WORD_REP:
    /* > 发送应答信息 */
    rep.serial = req->serial;
    if (rtrd_cli_send(ctx->rtrd_cli, MSG_INSERT_WORD_REP, orig, (void *)&rep, sizeof(rep)))
    {
        log_error(ctx->log, "Send response failed! serial:%s word:%s url:%s freq:%d",
                req->serial, req->word, req->url, req->freq);
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
 **函数名称: invtd_rttp_reg
 **功    能: 注册RTTP回调
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_rttp_reg(invtd_cntx_t *ctx)
{
#define INVTD_RTTP_REG(ctx, type, proc, args) \
    if (rtrd_register((ctx)->rtrd, type, proc, args)) \
    { \
        log_error(ctx->log, "Register callback failed!"); \
        return INVT_ERR; \
    }

   INVTD_RTTP_REG(ctx, MSG_SEARCH_WORD_REQ, invtd_search_word_req_hdl, ctx);
   INVTD_RTTP_REG(ctx, MSG_INSERT_WORD_REQ, invtd_insert_word_req_hdl, ctx);
   INVTD_RTTP_REG(ctx, MSG_PRINT_INVT_TAB_REQ, invtd_print_invt_tab_req_hdl, ctx);

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_start_rttp
 **功    能: 启动RTTP服务
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册, 并启动SDTP服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_start_rttp(invtd_cntx_t *ctx)
{
    if (invtd_rttp_reg(ctx))
    {
        return INVT_ERR;
    }

    return rtrd_startup(ctx->rtrd);
}

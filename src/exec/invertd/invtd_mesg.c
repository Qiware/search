/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invtd_mesg.c
 ** 版本号: 1.0
 ** 描  述: 倒排服务与消息处理相关内容
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 10:37:21 PM CST #
 ******************************************************************************/
#include "mesg.h"
#include "invertd.h"
#include "xml_tree.h"
#include "rtrd_recv.h"
#include "agent_mesg.h"

/******************************************************************************
 **函数名称: invtd_search_word_parse
 **功    能: 解析搜索请求
 **输入参数:
 **     ctx: 全局对象
 **     buff: 搜索请求(报头+报体)
 **     len: 数据长度
 **输出参数:
 **     req: 搜索请求
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.12.27 03:39:00 #
 ******************************************************************************/
static int invtd_search_word_parse(invtd_cntx_t *ctx,
        const char *buf, size_t len, mesg_search_word_req_t *req)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;
    const agent_header_t *head = (const agent_header_t *)buf;
    const char *str = (const char *)(head + 1);

    memset(&opt, 0, sizeof(opt));

    /* > 字节序转换 */
    req->serial = ntoh64(head->serial);

    log_trace(ctx->log, "len:%d body:%s", head->length, str);

    /* > 构建XML树 */
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_screat(str, head->length, &opt);
    if (NULL == xml)
    {
        return -1;
    }

    /* > 解析XML树 */
    node = xml_query(xml, ".SEARCH.WORDS");
    if (NULL != node)
    {
        snprintf(req->words, sizeof(req->words), "%s", node->value.str);
    }

    xml_destroy(xml);

    return 0;
}

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
static int invtd_search_word_req_hdl(int type, int dev_orig, char *buff, size_t len, void *args)
{
    void *addr = NULL;
    char freq[32];
    int idx, body_len, total_len;
    xml_opt_t opt;
    xml_tree_t *xml = NULL;
    xml_node_t *root, *item;
    list_node_t *node;
    invt_word_doc_t *doc;
    invt_dic_word_t *word;
    mesg_search_word_req_t req;     /* 请求 */
    mesg_data_t *rsp;    /* 应答 */
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;

    memset(&req, 0, sizeof(req));
    memset(&opt, 0, sizeof(opt));

    /* > 解析搜索信息 */
    if (invtd_search_word_parse(ctx, buff, len, &req)) {
        log_error(ctx->log, "Parse search request failed! words:%s", req.words);
        goto INVTD_SRCH_RSP;
    }

    /* > 构建XML树 */
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_creat_empty(&opt);
    if (NULL == xml) {
        log_error(ctx->log, "Create xml failed!");
        return -1;
    }

    root = xml_set_root(xml, "SEARCH");
    if (NULL == root) {
        log_error(ctx->log, "Set xml root failed!");
        goto GOTO_FREE;
    }

    xml_add_attr(xml, root, "CMD", "rsp");

    /* > 搜索倒排表 */
    pthread_rwlock_rdlock(&ctx->invtab_lock);

    word = invtab_query(ctx->invtab, req.words);
    if (NULL == word
        || NULL == word->doc_list)
    {
        pthread_rwlock_unlock(&ctx->invtab_lock);
        log_error(ctx->log, "Didn't search anything! words:%s", req.words);
        goto INVTD_SRCH_RSP;
    }

    /* > 打印搜索结果 */
    idx = 0;
    node = word->doc_list->head;
    for (; NULL!=node; node=node->next, ++idx) {
        doc = (invt_word_doc_t *)node->data;

        snprintf(freq, sizeof(freq), "%d", doc->freq);

        item = xml_add_child(xml, root, "ITEM", NULL); 
        xml_add_attr(xml, item, "URL", doc->url.str);
        xml_add_attr(xml, item, "FREQ", freq);
    }
    pthread_rwlock_unlock(&ctx->invtab_lock);

INVTD_SRCH_RSP:
    /* > 应答搜索结果 */
    body_len = xml_pack_len(xml);
    total_len = mesg_data_total(body_len);

    addr = (char *)calloc(1, total_len);
    if (NULL == addr) {
        goto GOTO_FREE;
    }

    rsp = (mesg_data_t *)addr;

    xml_spack(xml, rsp->body);
    rsp->serial = hton64(req.serial);

    if (rtrd_send(ctx->rtrd, MSG_SEARCH_WORD_RSP, dev_orig, addr, total_len)) {
        log_error(ctx->log, "Send response failed! serial:%ld words:%s",
                req.serial, req.words);
    }

GOTO_FREE:
    if (NULL != addr) { free(addr); }
    if (NULL != xml) { xml_destroy(xml) };

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_insert_word_req_hdl
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
    mesg_insert_word_rsp_t rsp; /* 应答 */
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;
    mesg_insert_word_req_t *req = (mesg_insert_word_req_t *)buff; /* 请求 */

    memset(&rsp, 0, sizeof(rsp));

    req->serial = ntoh64(req->serial);
    req->freq = ntohl(req->freq);

    /* > 插入倒排表 */
    pthread_rwlock_wrlock(&ctx->invtab_lock);
    if (invtab_insert(ctx->invtab, req->word, req->url, req->freq))
    {
        pthread_rwlock_unlock(&ctx->invtab_lock);
        log_error(ctx->log, "Insert invert table failed! serial:%s word:%s url:%s freq:%d",
                req->serial, req->word, req->url, req->freq);
        /* > 设置应答信息 */
        rsp.code = htonl(MESG_INSERT_WORD_FAIL); // 失败
        snprintf(rsp.word, sizeof(rsp.word), "%s", req->word);
        goto INVTD_INSERT_WORD_RSP;
    }
    pthread_rwlock_unlock(&ctx->invtab_lock);

    /* > 设置应答信息 */
    rsp.code = htonl(MESG_INSERT_WORD_SUCC); // 成功
    snprintf(rsp.word, sizeof(rsp.word), "%s", req->word);

INVTD_INSERT_WORD_RSP:
    /* > 发送应答信息 */
    rsp.serial = hton64(req->serial);
    if (rtrd_send(ctx->rtrd, MSG_INSERT_WORD_RSP, orig, (void *)&rsp, sizeof(rsp)))
    {
        log_error(ctx->log, "Send response failed! serial:%s word:%s url:%s freq:%d",
                req->serial, req->word, req->url, req->freq);
    }

    return INVT_OK;
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
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_rtmq_reg
 **功    能: 注册RTMQ回调
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_rtmq_reg(invtd_cntx_t *ctx)
{
#define INVTD_RTMQ_REG(ctx, type, proc, args) \
    if (rtrd_register((ctx)->rtrd, type, proc, args)) \
    { \
        log_error(ctx->log, "Register callback failed!"); \
        return INVT_ERR; \
    }

   INVTD_RTMQ_REG(ctx, MSG_SEARCH_WORD_REQ, invtd_search_word_req_hdl, ctx);
   INVTD_RTMQ_REG(ctx, MSG_INSERT_WORD_REQ, invtd_insert_word_req_hdl, ctx);
   INVTD_RTMQ_REG(ctx, MSG_PRINT_INVT_TAB_REQ, invtd_print_invt_tab_req_hdl, ctx);

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_start_rtmq
 **功    能: 启动RTMQ服务
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册, 并启动SDTP服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_start_rtmq(invtd_cntx_t *ctx)
{
    if (invtd_rtmq_reg(ctx))
    {
        return INVT_ERR;
    }

    return rtrd_launch(ctx->rtrd);
}

/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invtd_search.c
 ** 版本号: 1.0
 ** 描  述: 搜索处理流程
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 10:37:21 PM CST #
 ******************************************************************************/
#include "mesg.h"
#include "invertd.h"
#include "xml_tree.h"
#include "rtmq_recv.h"

#define SRCH_SEG_FREQ_LEN           (32)    /* 字段FREQ长度 */

#define SRCH_CODE_OK                "0000"  /* 返回OK */
#define SRCH_CODE_ERR               "0001"  /* 异常错误 */
#define SRCH_CODE_NO_DATA           "0002"  /* 无数据 */

/* 静态函数 */
static int invtd_search_rsp_item_add(invt_word_doc_t *doc, xml_tree_t *xml);

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
        char *buf, size_t len, mesg_search_word_req_t *req)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;
    mesg_header_t *head = (mesg_header_t *)buf;
    const char *xml_str = (const char *)(head + 1);


    /* > 字节序转换 */
    MESG_HEAD_NTOH(head, head);

    /* > 校验合法性 */
    if (!MESG_CHKSUM_ISVALID(head)
        ||  (len != MESG_TOTAL_LEN(head->length)))
    {
        log_error(ctx->log, "serial:%lu type:%u flag:%u chksum:0x%X len:%d body:%s",
                head->serial, head->type, head->flag, head->chksum, head->length, xml_str);
        return -1;
    }

    log_trace(ctx->log, "serial:%lu type:%u flag:%u chksum:0x%X len:%d body:%s",
            head->serial, head->type, head->flag, head->chksum, head->length, xml_str);

    /* > 构建XML树 */
    memset(&opt, 0, sizeof(opt));

    opt.log = ctx->log;
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_screat(xml_str, head->length, &opt);
    if (NULL == xml) {
        log_error(ctx->log, "Parse xml failed!");
        return -1;
    }

    do {
        /* > 提取搜索关键字 */
        node = xml_query(xml, ".SEARCH.WORDS");
        if (NULL == node) {
            log_error(ctx->log, "Get search words failed!");
            break;
        }

        snprintf(req->words, sizeof(req->words), "%s", node->value.str);

        log_trace(ctx->log, "words:%s", req->words);

        /* > 释放内存空间 */
        xml_destroy(xml);
        return 0;
    } while (0);

    xml_destroy(xml);
    return -1;
}

/******************************************************************************
 **函数名称: invtd_search_word_query
 **功    能: 从倒排表中搜索关键字
 **输入参数:
 **     ctx: 上下文
 **     req: 搜索请求信息
 **输出参数: NONE
 **返    回: 搜索结果(以XML树组织)
 **实现描述: 从倒排表中查询结果，并将结果以XML树组织.
 **注意事项: 完成发送后, 必须记得释放XML树的所有内存
 **作    者: # Qifeng.zou # 2016.01.04 17:35:35 #
 ******************************************************************************/
static xml_tree_t *invtd_search_word_query(invtd_cntx_t *ctx, mesg_search_word_req_t *req)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *root, *item;
    invt_dic_word_t *word;
    char freq[SRCH_SEG_FREQ_LEN];

    memset(&opt, 0, sizeof(opt));

    /* > 构建XML树 */
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;
    opt.log = ctx->log;

    xml = xml_empty(&opt);
    if (NULL == xml) {
        log_error(ctx->log, "Create xml failed!");
        return NULL;
    }

    root = xml_set_root(xml, "SEARCH-RSP");
    if (NULL == root) {
        log_error(ctx->log, "Set xml root failed!");
        goto GOTO_SEARCH_ERR;
    }

    /* > 搜索倒排表 */
    pthread_rwlock_rdlock(&ctx->invtab_lock);

    word = (invt_dic_word_t *)invtab_query(ctx->invtab, req->words);
    if (NULL == word
        || NULL == word->doc_list)
    {
        pthread_rwlock_unlock(&ctx->invtab_lock);
        log_warn(ctx->log, "Didn't search anything! words:%s", req->words);
        goto GOTO_SEARCH_NO_DATA;
    }

    /* > 构建搜索结果 */
    if (list_trav(word->doc_list, (trav_cb_t)invtd_search_rsp_item_add, (void *)xml)) {
        pthread_rwlock_unlock(&ctx->invtab_lock);
        log_error(ctx->log, "Contribute respone list failed! words:%s", req->words);
        goto GOTO_SEARCH_ERR;
    }
    pthread_rwlock_unlock(&ctx->invtab_lock);

    xml_add_attr(xml, root, "CODE", SRCH_CODE_OK); /* 设置返回码 */

    return xml;

GOTO_SEARCH_ERR:        /* 异常错误处理 */
    xml_destroy(xml);
    return NULL;

GOTO_SEARCH_NO_DATA:    /* 搜索结果为空的处理 */
    xml_add_attr(xml, root, "CODE", SRCH_CODE_NO_DATA); /* 无数据 */

    snprintf(freq, sizeof(freq), "%d", 0);

    item = xml_add_child(xml, root, "ITEM", NULL); 
    if (NULL == item) {
        goto GOTO_SEARCH_ERR;
    }
    xml_add_attr(xml, item, "URL", "Sorry, Didn't search anything!");
    xml_add_attr(xml, item, "FREQ", freq);
    return xml;
}

/******************************************************************************
 **函数名称: invtd_search_rsp_item_add
 **功    能: 构建搜索应答项
 **输入参数:
 **     doc: 搜索列表
 **     xml: 应答结果树
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.05.04 01:33:38 #
 ******************************************************************************/
static int invtd_search_rsp_item_add(invt_word_doc_t *doc, xml_tree_t *xml)
{
    xml_node_t *item;
    char freq[SRCH_SEG_FREQ_LEN];

    snprintf(freq, sizeof(freq), "%d", doc->freq);

    item = xml_add_child(xml, xml->root, "ITEM", NULL); 
    if (NULL == item) {
        log_error(xml->log, "Add child failed! url:%s freq:%d", doc->url.str, doc->freq);
        return -1;
    }
    xml_add_attr(xml, item, "URL", doc->url.str);
    xml_add_attr(xml, item, "FREQ", freq);
    return 0;
}

/******************************************************************************
 **函数名称: invtd_search_send_and_free
 **功    能: 从发送搜索结果并释放内存
 **输入参数:
 **     ctx: 上下文
 **     xml: 搜索结果信息
 **     req: 搜索请求信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从倒排表中查询结果，并将结果以XML树组织.
 **注意事项:
 **作    者: # Qifeng.zou # 2016.01.04 17:35:35 #
 ******************************************************************************/
static int invtd_search_send_and_free(invtd_cntx_t *ctx, xml_tree_t *xml,
        mesg_header_t *head, mesg_search_word_req_t *req, int orig)
{
    void *addr = NULL;
    mesg_header_t *rsp; /* 应答 */
    int body_len, total_len;

    if (NULL == xml) { return 0; }

    /* > 发送搜索应答 */
    body_len = xml_pack_len(xml);
    total_len = MESG_TOTAL_LEN(body_len);

    do {
        /* 申请内存空间 */
        addr = (char *)calloc(1, total_len);
        if (NULL == addr) {
            break;
        }

        /* 设置发送内容 */
        rsp = (mesg_header_t *)addr;

        MESG_HEAD_SET(rsp, MSG_SEARCH_WORD_RSP, head->serial, sizeof(mesg_search_word_req_t));
        MESG_HEAD_HTON(rsp, rsp);

        xml_spack(xml, rsp->body);

        /* 放入发送队列 */
        if (rtmq_proxy_async_send(ctx->frwder, MSG_SEARCH_WORD_RSP, addr, total_len)) {
            log_error(ctx->log, "Send response failed! serial:%ld words:%s",
                    head->serial, req->words);
        }
        free(addr);
    } while(0);

    xml_destroy(xml);

    return INVT_OK;
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
int invtd_search_word_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    xml_tree_t *xml;
    mesg_search_word_req_t req; /* 请求 */
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)buff;

    /* > 解析搜索信息 */
    if (invtd_search_word_parse(ctx, buff, len, &req)) {
        log_error(ctx->log, "Parse search request failed! words:%s", req.words);
        return INVT_ERR;
    }

    /* > 从倒排表中搜索关键字 */
    xml = invtd_search_word_query(ctx, &req); 
    if (NULL == xml) {
        log_error(ctx->log, "Search word form table failed! words:%s", req.words);
        return INVT_ERR;
    }

    /* > 发送搜索结果&释放内存 */
    if (invtd_search_send_and_free(ctx, xml, head, &req, orig)) {
        log_error(ctx->log, "Search word form table failed! words:%s", req.words);
        return INVT_ERR;
    }

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
int invtd_insert_word_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    mesg_insert_word_rsp_t *rsp;
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;
    mesg_header_t *rsp_head, *head = (mesg_header_t *)buff;
    mesg_insert_word_req_t *req = (mesg_insert_word_req_t *)(head + 1); /* 请求 */
    char addr[sizeof(mesg_header_t) + sizeof(mesg_insert_word_rsp_t)];

    /* > 转换字节序 */
    MESG_HEAD_NTOH(head, head);
    req->freq = ntohl(req->freq);

    rsp_head = (mesg_header_t *)addr;
    rsp = (mesg_insert_word_rsp_t *)(rsp_head + 1);

    /* > 插入倒排表 */
    pthread_rwlock_wrlock(&ctx->invtab_lock);
    if (invtab_insert(ctx->invtab, req->word, req->url, req->freq)) {
        pthread_rwlock_unlock(&ctx->invtab_lock);
        log_error(ctx->log, "Insert invert table failed! serial:%s word:%s url:%s freq:%d",
                head->serial, req->word, req->url, req->freq);
        /* > 设置应答信息 */
        rsp->code = MESG_INSERT_WORD_FAIL; // 失败
        snprintf(rsp->word, sizeof(rsp->word), "%s", req->word);
        goto INVTD_INSERT_WORD_RSP;
    }
    pthread_rwlock_unlock(&ctx->invtab_lock);

    /* > 设置应答信息 */
    rsp->code = MESG_INSERT_WORD_SUCC; // 成功
    snprintf(rsp->word, sizeof(rsp->word), "%s", req->word);

INVTD_INSERT_WORD_RSP:
    /* > 发送应答信息 */
    MESG_HEAD_SET(rsp_head, MSG_INSERT_WORD_RSP, head->serial, sizeof(mesg_insert_word_rsp_t));
    MESG_HEAD_HTON(rsp_head, rsp_head);
    mesg_insert_word_resp_hton(rsp);

    if (rtmq_proxy_async_send(ctx->frwder, MSG_INSERT_WORD_RSP, (void *)addr, sizeof(addr))) {
        log_error(ctx->log, "Send response failed! serial:%s word:%s url:%s freq:%d",
                head->serial, req->word, req->url, req->freq);
    }

    return INVT_OK;
}

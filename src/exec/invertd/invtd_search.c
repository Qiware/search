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
        char *buf, size_t len, mesg_search_word_req_t *req)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;
    agent_header_t *head = (agent_header_t *)buf;
    const char *str = (const char *)(head + 1);

    memset(&opt, 0, sizeof(opt));

    /* > 字节序转换 */
    head->type = ntohl(head->type);
    head->flag = ntohl(head->flag);
    head->length = ntohl(head->length);
    head->mark = ntohl(head->mark);
    head->serial = ntoh64(head->serial);

    req->serial = head->serial;

    log_trace(ctx->log, "serial:%lu type:%u flag:%u mark:0X%x len:%d body:%s",
            head->serial, head->type, head->flag, head->mark, head->length, str);

    /* > 构建XML树 */
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_screat(str, head->length, &opt);
    if (NULL == xml) {
        log_error(ctx->log, "Parse xml failed!");
        return -1;
    }

    /* > 解析XML树 */
    node = xml_query(xml, ".SEARCH.WORDS");
    if (NULL == node) {
        log_error(ctx->log, "Get search words failed!");
    }
    else {
        snprintf(req->words, sizeof(req->words), "%s", node->value.str);
    }

    xml_destroy(xml);

    log_trace(ctx->log, "words:%s", req->words);

    return 0;
}

/******************************************************************************
 **函数名称: intvd_search_word_from_tab
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
static xml_tree_t *intvd_search_word_from_tab(invtd_cntx_t *ctx, mesg_search_word_req_t *req)
{
    int idx;
    char freq[32];
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *root, *item;
    list_node_t *node;
    invt_word_doc_t *doc;
    invt_dic_word_t *word;

    memset(&opt, 0, sizeof(opt));

    /* > 构建XML树 */
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_creat_empty(&opt);
    if (NULL == xml) {
        log_error(ctx->log, "Create xml failed!");
        return NULL;
    }

    root = xml_set_root(xml, "SEARCH");
    if (NULL == root) {
        log_error(ctx->log, "Set xml root failed!");
        goto GOTO_SEARCH_ERR;
    }

    xml_add_attr(xml, root, "CMD", "rsp");

    /* > 搜索倒排表 */
    pthread_rwlock_rdlock(&ctx->invtab_lock);

    word = invtab_query(ctx->invtab, req->words);
    if (NULL == word
        || NULL == word->doc_list)
    {
        pthread_rwlock_unlock(&ctx->invtab_lock);
        log_warn(ctx->log, "Didn't search anything! words:%s", req->words);
        goto GOTO_SEARCH_NO_DATA;
    }

    /* > 构建搜索结果 */
    idx = 0;
    node = word->doc_list->head;
    for (; NULL!=node; node=node->next, ++idx) {
        doc = (invt_word_doc_t *)node->data;

        snprintf(freq, sizeof(freq), "%d", doc->freq);

        item = xml_add_child(xml, root, "ITEM", NULL); 
        if (NULL == item) {
            goto GOTO_SEARCH_ERR;
        }
        xml_add_attr(xml, item, "URL", doc->url.str);
        xml_add_attr(xml, item, "FREQ", freq);
    }
    pthread_rwlock_unlock(&ctx->invtab_lock);

    return xml;

GOTO_SEARCH_ERR:
    xml_destroy(xml);
    return NULL;

GOTO_SEARCH_NO_DATA:
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
 **函数名称: intvd_search_send_and_free
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
static int intvd_search_send_and_free(invtd_cntx_t *ctx,
        xml_tree_t *xml, mesg_search_word_req_t *req, int dev_orig)
{
    void *addr = NULL;
    mesg_data_t *rsp; /* 应答 */
    int body_len, total_len;

    if (NULL == xml) { return 0; }

    /* > 发送搜索应答 */
    body_len = xml_pack_len(xml);
    total_len = mesg_data_total(body_len);

    do
    {
        addr = (char *)calloc(1, total_len);
        if (NULL == addr) {
            break;
        }

        rsp = (mesg_data_t *)addr;

        xml_spack(xml, rsp->body);
        rsp->serial = hton64(req->serial);

        if (rtrd_send(ctx->rtrd, MSG_SEARCH_WORD_RSP, dev_orig, addr, total_len)) {
            log_error(ctx->log, "Send response failed! serial:%ld words:%s",
                    req->serial, req->words);
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
 **     dev_orig: 源设备ID
 **     buff: 搜索关键字-请求数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从倒排表中查询结果，并将结果返回给客户端
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_search_word_req_hdl(int type, int dev_orig, char *buff, size_t len, void *args)
{
    xml_tree_t *xml;
    mesg_search_word_req_t req; /* 请求 */
    invtd_cntx_t *ctx = (invtd_cntx_t *)args;

    /* > 解析搜索信息 */
    if (invtd_search_word_parse(ctx, buff, len, &req)) {
        log_error(ctx->log, "Parse search request failed! words:%s", req.words);
        return INVT_ERR;
    }

    /* > 从倒排表中搜索关键字 */
    xml = intvd_search_word_from_tab(ctx, &req); 
    if (NULL == xml) {
        log_error(ctx->log, "Search word form table failed! words:%s", req.words);
        return INVT_ERR;
    }

    /* > 发送搜索结果&释放内存 */
    if (intvd_search_send_and_free(ctx, xml, &req, dev_orig)) {
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
 **     dev_orig: 源节点ID
 **     buff: 插入关键字-请求数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 源节点ID(dev_orig)将成为应答消息的目的节点ID(dest)
    **作    者: # Qifeng.zou # 2015-06-17 21:37:55 #
 ******************************************************************************/
int invtd_insert_word_req_hdl(int type, int dev_orig, char *buff, size_t len, void *args)
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
    if (rtrd_send(ctx->rtrd, MSG_INSERT_WORD_RSP, dev_orig, (void *)&rsp, sizeof(rsp)))
    {
        log_error(ctx->log, "Send response failed! serial:%s word:%s url:%s freq:%d",
                req->serial, req->word, req->url, req->freq);
    }

    return INVT_OK;
}

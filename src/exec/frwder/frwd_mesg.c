/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: frwd_mesg.c
 ** 版本号: 1.0
 ** 描  述: 消息处理函数定义
 ** 作  者: # Qifeng.zou # Tue 14 Jul 2015 02:52:16 PM CST #
 ******************************************************************************/
#include "frwd.h"
#include "mesg.h"
#include "vector.h"
#include "command.h"

/* 静态函数 */
static int frwd_reg_req_cb(frwd_cntx_t *frwd);
static int frwd_reg_rsp_cb(frwd_cntx_t *frwd);

static int frwd_sub_req_hdl(int type, int orig, char *data, size_t len, void *args);
static int frwd_sub_find_or_add(frwd_cntx_t *ctx, mesg_header_t *head, mesg_sub_req_t *req);

static int frwd_search_word_req_hdl(int type, int orig, char *data, size_t len, void *args);
static int frwd_search_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

static int frwd_insert_word_req_hdl(int type, int orig, char *data, size_t len, void *args);
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
    frwd_reg_req_cb(frwd);
    frwd_reg_rsp_cb(frwd);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_reg_req_cb
 **功    能: 注册请求回调
 **输入参数:
 **     frwd: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.24 14:22:25 #
 ******************************************************************************/
static int frwd_reg_req_cb(frwd_cntx_t *frwd)
{
#define FRWD_REG_REQ_CB(frwd, type, proc, args) \
    if (rtmq_register((frwd)->downstrm, type, (rtmq_reg_cb_t)proc, (void *)args)) { \
        log_error((frwd)->log, "Register type [%d] failed!", type); \
        return FRWD_ERR; \
    }

    FRWD_REG_REQ_CB(frwd, MSG_SEARCH_WORD_REQ, frwd_search_word_req_hdl, frwd);
    FRWD_REG_REQ_CB(frwd, MSG_INSERT_WORD_REQ, frwd_insert_word_req_hdl, frwd);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_reg_rsp_cb
 **功    能: 注册应答回调
 **输入参数:
 **     frwd: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.24 14:24:06 #
 ******************************************************************************/
static int frwd_reg_rsp_cb(frwd_cntx_t *frwd)
{
#define FRWD_REG_RSP_CB(frwd, type, proc, args) \
    if (rtmq_register((frwd)->upstrm, type, (rtmq_reg_cb_t)proc, (void *)args)) { \
        log_error((frwd)->log, "Register type [%d] failed!", type); \
        return FRWD_ERR; \
    }

    FRWD_REG_RSP_CB(frwd, MSG_SUB_REQ, frwd_sub_req_hdl, frwd);
    FRWD_REG_RSP_CB(frwd, MSG_SEARCH_WORD_RSP, frwd_search_word_rsp_hdl, frwd);
    FRWD_REG_RSP_CB(frwd, MSG_INSERT_WORD_RSP, frwd_insert_word_rsp_hdl, frwd);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_sub_req_hdl
 **功    能: 订阅请求
 **输入参数:
 **     type: 数据类型
 **     orig: 源结点ID
 **     data: 需要转发的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将收到的请求转发给倒排服务
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.23 20:25:53 #
 ******************************************************************************/
static int frwd_sub_req_hdl(int type, int orig, char *data, size_t len, void *args)
{
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;
    mesg_sub_req_t *req = (mesg_sub_req_t *)(head + 1);

    /* > 转换字节序 */
    mesg_head_ntoh(head, head);
    mesg_sub_req_ntoh(req);

    /* > 订阅处理 */
    return frwd_sub_find_or_add(ctx, head, req);
}

/******************************************************************************
 **函数名称: frwd_sub_list_creat
 **功    能: 创建订阅列表
 **输入参数:
 **     type: 消息类型
 **输出参数: NONE
 **返    回: 订阅列表
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 07:07:26 #
 ******************************************************************************/
static frwd_sub_list_t *frwd_sub_list_creat(mesg_type_e type)
{
    frwd_sub_list_t *sl;

    sl = (frwd_sub_list_t *)calloc(1, sizeof(frwd_sub_list_t));
    if (NULL == sl) {
        return NULL;
    }

    sl->type = type;

    sl->list = (vector_t *)vector_creat(FRWD_SUB_VEC_CAP, FRWD_SUB_VEC_INCR);
    if (NULL == sl->list) {
        free(sl);
        return NULL;
    }

    return sl;
}

/* 查找订阅列表的回调 */
static bool frwd_sub_list_find_cb(frwd_sub_item_t *item, int *nodeid)
{
    return (item->nodeid == *nodeid)? true : false;
}

/******************************************************************************
 **函数名称: frwd_sub_find_or_add
 **功    能: 订阅请求处理
 **输入参数:
 **     ctx: 全局对象
 **     head: 协议头
 **     req: 订阅请求
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 06:56:17 #
 ******************************************************************************/
static int frwd_sub_find_or_add(frwd_cntx_t *ctx, mesg_header_t *head, mesg_sub_req_t *req)
{
    int ret = FRWD_ERR;
    frwd_sub_list_t *sl;
    frwd_sub_item_t *item;
    frwd_sub_mgr_t *sub_mgr = &ctx->upstrm_sub_mgr;

    /* > 查找是否已经订阅 */
    pthread_rwlock_wrlock(&sub_mgr->lock);
    do {
        sl = avl_query(sub_mgr->list, &req->type, sizeof(req->type));
        if (NULL == sl) {
            sl = frwd_sub_list_creat(req->type);
            if (NULL == sl) {
                log_error(ctx->log, "Create sub list failed!");
                break;
            }

            if (avl_insert(sub_mgr->list, &req->type, sizeof(req->type), (void *)sl)) {
                log_error(ctx->log, "Insert avl failed!");
                break;
            }
        }

        item = vector_find(sl->list, (find_cb_t)frwd_sub_list_find_cb, (void *)&req->nodeid);
        if (NULL != item) {
            ret = FRWD_OK;
            break;
        }

        /* > 申请新的结点空间 */
        item = (frwd_sub_item_t *)calloc(1, sizeof(frwd_sub_item_t));
        if (NULL == item) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        item->nodeid = req->nodeid;

        if (vector_insert(sl->list, (void *)item)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            free(item);
            break;
        }
        ret = FRWD_OK;
    } while(0);

    pthread_rwlock_unlock(&sub_mgr->lock);
    return ret;
}

/******************************************************************************
 **函数名称: frwd_search_word_req_hdl
 **功    能: 搜索关键字请求处理
 **输入参数:
 **     type: 数据类型
 **     orig: 源结点ID
 **     data: 需要转发的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将收到的请求转发给倒排服务
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.23 20:25:53 #
 ******************************************************************************/
static int frwd_search_word_req_hdl(int type, int orig, char *data, size_t len, void *args)
{
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;

    /* > 转换字节序 */
    mesg_head_ntoh(head, head);

    log_trace(ctx->log, "Call %s()! serial:%lu type:%d len:%d flag:%d mark:[0x%X/0x%X]",
            __func__, head->serial, head->type,
            head->length, head->flag, head->mark, MSG_MARK_KEY);

    mesg_head_hton(head, head);

    /* > 发送数据 */
    if (rtmq_send(ctx->upstrm, type, 30001, data, len)) {
        log_error(ctx->log, "Push data into send queue failed! type:%u", type);
        return -1;
    }

    return 0;
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
 **实现描述: 将收到的搜索应答转发至帧听层
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
static int frwd_search_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args)
{
    serial_t serial;
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;

    log_trace(ctx->log, "Call %s()", __func__);

    serial.serial = ntoh64(head->serial);

    /* > 发送数据 */
    if (rtmq_send(ctx->downstrm, type, serial.nid, data, len)) {
        log_error(ctx->log, "Push data into send queue failed! type:%u", type);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: frwd_insert_word_req_hdl
 **功    能: 插入关键字的请求
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
 **作    者: # Qifeng.zou # 2016.02.23 20:26:55 #
 ******************************************************************************/
static int frwd_insert_word_req_hdl(int type, int orig, char *data, size_t len, void *args)
{
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;

    log_trace(ctx->log, "Call %s()", __func__);

    /* > 发送数据 */
    if (rtmq_send(ctx->upstrm, type, 30001, data, len)) {
        log_error(ctx->log, "Push data into send queue failed! type:%u", type);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: frwd_insert_word_rsp_hdl
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
    serial_t serial;
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;

    mesg_head_ntoh(head, head);

    serial.serial = head->serial;
    log_trace(ctx->log, "Call %s()! serial:%lu", __func__, head->serial);

    mesg_head_hton(head, head);

    /* > 发送数据 */
    if (rtmq_send(ctx->downstrm, type, serial.nid, data, len)) {
        log_error(ctx->log, "Push data into send queue failed! type:%u", type);
        return -1;
    }

    return 0;
}

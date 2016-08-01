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

static int frwd_search_req_hdl(int type, int orig, char *data, size_t len, void *args);
static int frwd_search_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

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

    FRWD_REG_REQ_CB(frwd, MSG_SEARCH_REQ, frwd_search_req_hdl, frwd);
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

    FRWD_REG_RSP_CB(frwd, MSG_SEARCH_RSP, frwd_search_rsp_hdl, frwd);
    FRWD_REG_RSP_CB(frwd, MSG_INSERT_WORD_RSP, frwd_insert_word_rsp_hdl, frwd);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_search_req_hdl
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
static int frwd_search_req_hdl(int type, int orig, char *data, size_t len, void *args)
{
    int nid;
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;

    /* > 转换字节序 */
    MESG_HEAD_NTOH(head, head);

    log_trace(ctx->log, "sid:%lu serial:%lu type:%d len:%d flag:%d chksum:[0x%X/0x%X]",
            head->sid, head->serial, head->type,
            head->length, head->flag, head->chksum, MSG_CHKSUM_VAL);

    MESG_HEAD_HTON(head, head);

    /* > 发送数据 */
    nid = rtmq_sub_query(ctx->upstrm, type);
    if (-1 == nid) {
        log_error(ctx->log, "No module sub type! type:%u", type);
        return 0;
    }

    if (rtmq_async_send(ctx->upstrm, type, nid, data, len)) {
        log_error(ctx->log, "Push data into send queue failed! type:%u", type);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: frwd_search_rsp_hdl
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
static int frwd_search_rsp_hdl(int type, int orig, char *data, size_t len, void *args)
{
    serial_t serial;
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;
    mesg_header_t *head = (mesg_header_t *)data;

    log_trace(ctx->log, "sid:%lu serial:%lu", ntoh64(head->sid), ntoh64(head->serial));

    serial.serial = ntoh64(head->serial);

    /* > 发送数据 */
    if (rtmq_async_send(ctx->downstrm, type, serial.nid, data, len)) {
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

    /* > 发送数据 */
    if (rtmq_async_send(ctx->upstrm, type, 30001, data, len)) {
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

    MESG_HEAD_NTOH(head, head);

    serial.serial = head->serial;
    log_trace(ctx->log, "serial:%lu", head->serial);

    MESG_HEAD_HTON(head, head);

    /* > 发送数据 */
    if (rtmq_async_send(ctx->downstrm, type, serial.nid, data, len)) {
        log_error(ctx->log, "Push data into send queue failed! type:%u", type);
        return -1;
    }

    return 0;
}

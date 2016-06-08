#include "lwsd_lws.h"

/* Static function */
static int lwsd_search_wsi_user_init(lwsd_cntx_t *ctx,
        struct libwebsocket *wsi, lwsd_search_user_data_t *user);
static int lwsd_search_cmd_hdl(
        lwsd_cntx_t *ctx, struct libwebsocket_context *lws,
        struct libwebsocket *wsi, void *user, void *in, size_t len);
static int lwsd_search_wsi_send_data(lwsd_cntx_t *ctx,
        struct libwebsocket_context *lws,
        struct libwebsocket *wsi, lwsd_search_user_data_t *user);

/******************************************************************************
 **函数名称: lwsd_lws_reg_add
 **功    能: 添加注册处理
 **输入参数:
 **     ctx: 全局对象
 **     type: 命令类型
 **     proc: 处理回调
 **     args: 附加数据
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.06 23:43:02 #
 ******************************************************************************/
int lwsd_lws_reg_add(lwsd_cntx_t *ctx, int type, lws_reg_cb_t proc, void *args)
{
    lws_reg_t *item;

    /* > 判断是否冲突 */
    item = (lws_reg_t *)avl_query(ctx->lws_reg, &type, sizeof(type));
    if (NULL != item) {
        log_error(ctx->log, "Type [%d] was registered at before!", type);
        return -1; /* Was registered */
    }

    /* > 新建注册项 */
    item = (lws_reg_t *)calloc(1, sizeof(lws_reg_t));
    if (NULL == item) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    item->type = type;
    item->proc = proc;
    item->args = (void *)ctx;

    /* > 插入注册表 */
    if (avl_insert(ctx->lws_reg, &type, (void *)item)) {
        log_error(ctx->log, "Insert lws reg table failed!");
        free(item);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: lwsd_callback_search_hdl
 **功    能: SEARCH协议的处理回调
 **输入参数:
 **     ctx: 全局对象
 **     wsi: 连接对象
 **     reason: 回调原因
 **     user: 用户数据信息
 **     in: 输入数据
 **     len: 输入数据长度
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 连接上线/下线时, 触发上线状态的更新
 **     2. 收到完整BODY后, 将数据放入上报队列中
 **     3. 当存在广播数据时, 将数据放入各发送队列
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.06 21:40:51 #
 ******************************************************************************/
int lwsd_callback_search_hdl(struct libwebsocket_context *lws,
        struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason,
        void *_user, void *in, size_t len)
{
    lwsd_cntx_t *ctx = LWSD_GET_CTX();
    lwsd_search_user_data_t *user = (lwsd_search_user_data_t *)_user;

    switch (reason) {
        case LWS_CALLBACK_WSI_CREATE:                   /* 创建WSI实例 */
            return 0;                                   /* 注意: 此时还未创建user对象 */
        case LWS_CALLBACK_WSI_DESTROY:                  /* 销毁WSI实例 */
            return lwsd_search_wsi_destroy(ctx, user);
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:    /* 满足协议的链接 */
            return lwsd_search_wsi_user_init(ctx, wsi, user);
        case LWS_CALLBACK_CLOSED:
            return 0;
        case LWS_CALLBACK_RECEIVE:                       /* 接收数据 */
            return lwsd_search_cmd_hdl(ctx, lws, wsi, user, in, len);
        case LWS_CALLBACK_SERVER_WRITEABLE:              /* 可写事件 */
            return lwsd_search_wsi_send_data(ctx, lws, wsi, user);
        case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
        case LWS_CALLBACK_LOCK_POLL:
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
        case LWS_CALLBACK_UNLOCK_POLL:
            return 0;
        case LWS_CALLBACK_HTTP:
        case LWS_CALLBACK_HTTP_BODY:
        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        case LWS_CALLBACK_HTTP_FILE_COMPLETION:
        case LWS_CALLBACK_HTTP_WRITEABLE:
            return 0;
        default:
            return 0;
    }

    return 0;
}

/******************************************************************************
 **函数名称: lwsd_search_wsi_user_init
 **功    能: 初始化WSI用户数据
 **输入参数:
 **     ctx: 全局对象
 **     wsi: 连接对象
 **     user: WS实例附加数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.07 20:55:06 #
 ******************************************************************************/
static int lwsd_search_wsi_user_init(lwsd_cntx_t *ctx,
        struct libwebsocket *wsi, lwsd_search_user_data_t *user)
{
    /* > 初始化数据 */
    user->ctm = time(NULL);
    user->rtm = user->ctm;
    user->sid = LWSD_GEN_SEQ(ctx);
    user->wsi = wsi;
    snprintf(user->mark, sizeof(user->mark), "SEARCH");

    /* > 创建发送队列 */
    user->send_list = list_creat(NULL);
    if (NULL == user->send_list) {
        log_error("Create send list failed!");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: lwsd_search_wsi_user_free
 **功    能: 释放WS USER数据
 **输入参数:
 **     ctx: 全局对象
 **     user: WS实例附加数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.07 08:56:57 #
 ******************************************************************************/
static int lwsd_search_wsi_user_free(lwsd_cntx_t *ctx, lwsd_search_user_data_t *user)
{
    list_destroy(user->send_list, mem_dealloc, NULL);
    return 0;
}

/******************************************************************************
 **函数名称: lwsd_search_wsi_destroy
 **功    能: 初始化WS实例
 **输入参数:
 **     ctx: 全局对象
 **     user: WS实例附加数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.06 23:58:03 #
 ******************************************************************************/
static int lwsd_search_wsi_destroy(lwsd_cntx_t *ctx, lwsd_search_user_data_t *user)
{
    void *item;

    if (NULL == user) {
        return 0;
    }
    else if (NULL != user->pl) {
        mem_dealloc(NULL, user->pl); /* 释放空间 */
        user->pl = NULL;
    }

    rbt_delete(ctx->wsi_list, &user->sid, sizeof(user->sid), &item); 
    if (NULL != item) {
        mem_dealloc(NULL, item);
    }

    return lwsd_search_wsi_user_free(ctx, user);
}

/******************************************************************************
 **函数名称: lwsd_search_cmd_hdl
 **功    能: 对SEARCH协议命令的处理
 **输入参数:
 **     ctx: 全局对象
 **     wsi: 连接对象
 **     user: 用户数据信息
 **     in: 输入数据
 **     len: 输入数据长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 对收到的SEARCH数据进行解析处理
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.06 22:41:00 #
 ******************************************************************************/
static int lwsd_search_cmd_hdl(lwsd_cntx_t *ctx,
        struct libwebsocket_context *lws, struct libwebsocket *wsi,
        lwsd_search_user_data_t *user, void *in, size_t len)
{
    lws_reg_t *reg;
    mesg_header_t *head = (mesg_header_t *)in;

    user->rtm = time(NULL);

    /* > 字节序转换 */
    MESG_HEAD_NTOH(head, head);

    if (len < sizeof(mesg_header_t)
       || MESG_CHKSUM_ISVALID(head))
    {
        return -1; /* 长度异常 */
    }

    /* > 进行消息处理 */
    reg = (lws_reg_t *)avl_query(ctx->lws_reg, &head->type);
    if (NULL == reg) {
        return -1;
    }

    return reg->proc(head->type, in, len, reg->args);
}

/******************************************************************************
 **函数名称: lwsd_search_wsi_send_data
 **功    能: 发送数据
 **输入参数:
 **     ctx: CTX对象
 **     lws: LWS对象
 **     wsi: WSI对象
 **     user: 扩展数据
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.08 07:46:45 #
 ******************************************************************************/
static int lwsd_search_wsi_send_data(lwsd_cntx_t *ctx,
        struct libwebsocket_context *lws,
        struct libwebsocket *wsi, lwsd_search_user_data_t *user)
{
    int n;
    size_t left, m;
    time_t tm = time(NULL), diff;
    enum lws_write_protocol protocol;
    lws_conf_t *conf = &ctx->conf.lws;

    diff = tm - user->rtm;
    if (diff > conf->connections.timeout) {
        log_error("Connection is timeout! sid:%u ctm:%lu diff:%lu mark:%s",
                user->sid, user->ctm, diff, user->mark);
        return -1; /* 强制踢下线 */
    }

    do {
        /* > 获取发送长度限制(返回-1: 长度无限制) */
        m = lws_get_peer_write_allowance(wsi);
        if (0 == m) {
            break; /* Not allowed send data */
        }

        /* > 获取发送数据 */
        if (NULL == user->pl) {
            user->pl = (lwsd_mesg_payload_t *)list_lpop(user->send_list);
            if (NULL == user->pl) {
                return 0; /* No data */
            }
        }

        left = user->pl->len - user->pl->offset;

        /* > 发送数据 */
        protocol = (left != user->pl->len)? LWS_WRITE_HTTP : LWS_WRITE_BINARY;

        n = lws_write(wsi, (unsigned char *)user->pl->addr
                + user->pl->offset + LWS_SEND_BUFFER_PRE_PADDING, left, protocol);
        if (n < 0) {
            log_error("Send data failed! n:%d", n);
            mem_dealloc(NULL, user->pl); /* 释放空间 */
            user->pl = NULL;
            return -1;
        }
        else if (n != (int)left) {
            user->pl->offset += n;
        }
        else {
            mem_dealloc(NULL, user->pl); /* 释放空间 */
            user->pl = NULL;
        }
    } while(!lws_send_pipe_choked(wsi));

    lws_callback_on_writable(lws, wsi); /* 等待下次事件通知 */

    return 0;
}

#if !defined(__LWSD_LWS_H__)
#define __LWSD_LWS_H__

#define LWSD_MARK_STR_LEN   (64)

typedef int (*lws_reg_cb_t)(int type, int orig, char *data, size_t len, void *param);

/* 注册对象 */
typedef struct
{
    unsigned int type;                      /* 数据类型 */
    lws_reg_cb_t proc;                      /* 对应数据类型的处理函数 */
    void *args;                             /* 附加参数 */
} lws_reg_t;

/* 发送数据 */
typedef struct
{
    void *addr;                             /* 起始地址 */
    size_t len;                             /* 总字节数 */
    size_t offset;                          /* 发送字节数 */
} lwsd_mesg_payload_t;

/* 会话附加信息 */
typedef struct
{
    uint64_t            sid;                /* 会话ID */
    time_t              ctm;                /* 上线时间 */
    time_t              rtm;                /* 最新接收数据的时间 */
    struct libwebsocket *wsi;               /* 所属WSI */

#define LWSD_EXTRA_FLAG_CREATED  (0xA1B2D3E4) /* 已创建 */
#define LWSD_EXTRA_FLAG_DESTROY  (0x4E3D2B1A) /* 已销毁 */
    uint32_t            flag;               /* 创建/销毁标志 */

    char mark[LWSD_MARK_STR_LEN];           /* 备注信息 */
    list_t *send_list;                      /* 发送链表 */
    lwsd_mesg_payload_t *pl;                /* 当前正在发送的数据...(注意: 连接断开时, 记得释放该空间) */
} lwsd_search_user_data_t;

int lwsd_callback_search_hdl(struct libwebsocket_context *lws,
        struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason,
        void *user, void *in, size_t len);

/* 全局对象 */
extern lwsd_cntx_t *g_lwsd_ctx = NULL;
#define LWSD_GET_CTX() (g_lwsd_ctx)
#define LWSD_SET_CTX(ctx) (g_lwsd_ctx = (ctx))

#endif /*__LWSD_LWS_H__*/

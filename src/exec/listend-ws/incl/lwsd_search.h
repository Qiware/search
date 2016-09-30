#if !defined(__LWSD_SEARCH_H__)
#define __LWSD_SEARCH_H__

#include "list.h"
#include "lwsd.h"
#include "libwebsockets.h"

#define LWSD_MARK_STR_LEN   (64)

typedef int (*lws_reg_cb_t)(int type, char *data, size_t len, void *param);

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
    uint64_t            sid;                /* 会话ID(每个连接的会话ID都不一样) */
    time_t              ctm;                /* 上线时间 */
    time_t              rtm;                /* 最新接收数据的时间 */
    struct libwebsocket *wsi;               /* 所属WSI */

    char mark[LWSD_MARK_STR_LEN];           /* 备注信息 */
    list_t *send_list;                      /* 发送链表 */
    lwsd_mesg_payload_t *pl;                /* 当前正在发送的数据...(注意: 连接断开时, 记得释放该空间) */
} lwsd_search_user_data_t;

int lwsd_search_reg_add(lwsd_cntx_t *ctx, int type, lws_reg_cb_t proc, void *args);
int lwsd_search_async_send(lwsd_cntx_t *ctx, uint64_t sid, const void *addr, size_t len);
int lwsd_callback_search_hdl(struct libwebsocket_context *lws,
        struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason,
        void *user, void *in, size_t len);

/* 全局对象 */
extern lwsd_cntx_t *g_lwsd_ctx;

#define LWSD_GET_CTX() (g_lwsd_ctx)
#define LWSD_SET_CTX(ctx) (g_lwsd_ctx = (ctx))

#endif /*__LWSD_SEARCH_H__*/

#if !defined(__LWSD_H__)
#define __LWSD_H__

#include "log.h"
#include "comm.h"
#include "lwsd.h"
#include "rb_tree.h"
#include "avl_tree.h"
#include "lwsd_conf.h"

#include <libwebsockets.h>

#define LWSD_DEF_CONF_PATH      "../conf/listend-ws.xml"     /* 默认配置路径 */

/* 错误码 */
typedef enum
{
    LWSD_OK = 0                             /* 正常 */
    , LWSD_SHOW_HELP                        /* 显示帮助信息 */

    , LWSD_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} lwsd_err_code_e;

/* 输入参数 */
typedef struct
{
    int log_level;                          /* 日志级别 */
    bool isdaemon;                          /* 是否后台运行 */
    char *conf_path;                        /* 配置路径 */
} lwsd_opt_t;

/* WSI项 */
typedef struct
{
    uint64_t sid;                           /* 会话ID */
    struct libwebsocket *wsi;               /* WSI对象 */
} lwsd_wsi_item_t;

/* 全局对象 */
typedef struct
{
    lwsd_conf_t conf;                       /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */

    avl_tree_t *lws_reg;                    /* LWS注册表 */
    rbt_tree_t *wsi_map;                    /* WSI管理表(lwsd_wsi_item_t) */

    uint32_t wsi_seq;                       /* WSI序列号(递增) */
    uint32_t req_seq;                       /* REQ序列号(递增) */
    struct libwebsocket_context *lws;       /* LWS上下文 */
    rtmq_proxy_t *frwder;                   /* FRWDER服务 */
} lwsd_cntx_t;

#define LWSD_WSI_SEQ(ctx) (atomic32_inc(&(ctx)->wsi_seq))
#define LWSD_REQ_SEQ(ctx) (atomic32_inc(&(ctx)->req_seq))

int lwsd_getopt(int argc, char **argv, lwsd_opt_t *opt);
int lwsd_usage(const char *exec);

#endif /*__LWSD_H__*/

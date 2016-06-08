#if !defined(__LWSD_CONF_H__)
#define __LWSD_CONF_H__

#include "comm.h"
#include "rtmq_proxy.h"

/* LWS配置 */
typedef struct
{
    bool is_use_ssl;                        /* 是否使用SSL */
    char iface[IFACE_MAX_LEN];              /* 网卡名称 */

    struct {
        int max;                            /* 帧听端口 */
        int port;                           /* 帧听端口 */
        int timeout;                        /* 超时时间 */
    } connections;                          /* 连接配置 */

    queue_conf_t sendq;                     /* 发送对列 */

    char key_path[FILE_PATH_MAX_LEN];       /* 键值路径 */
    char cert_path[FILE_PATH_MAX_LEN];      /* 鉴权路径 */
    char resource_path[FILE_PATH_MAX_LEN];  /* 资源路径 */
} lws_conf_t;

/* LWSD配置 */
typedef struct
{
    int nid;                                /* 结点ID */
    char name[NODE_MAX_LEN];                /* 结点名 */
    char wdir[FILE_PATH_MAX_LEN];           /* 工作路径 */

    struct {
        int num;                            /* 队列数 */
        int max;                            /* 队列长度 */
        int size;                           /* 单元大小 */
    } distq;                                /* 分发队列 */

    lws_conf_t lws;                         /* LWS配置 */
    rtmq_proxy_conf_t frwder;               /* FRWDER配置 */
} lwsd_conf_t;

int lwsd_load_conf(const char *path, lwsd_conf_t *conf, log_cycle_t *log);

#endif /*__LWSD_CONF_H__*/

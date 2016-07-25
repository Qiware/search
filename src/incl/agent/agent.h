#if !defined(__AGENT_H__)
#define __AGENT_H__

#include "sck.h"
#include "slot.h"
#include "queue.h"
#include "rb_tree.h"
#include "spinlock.h"
#include "avl_tree.h"
#include "shm_queue.h"
#include "agent_comm.h"
#include "agent_listen.h"
#include "thread_pool.h"

/* 宏定义 */
#define AGENT_TMOUT_SCAN_SEC    (15)        /* 超时扫描间隔 */
#define AGENT_MSG_TYPE_MAX      (0xFF)      /* 消息最大类型 */

/* 命令路径 */
#define AGENT_LSVR_CMD_PATH     "lsvr-cmd-%02d.usck" /* 侦听服务 */
#define AGENT_RSVR_CMD_PATH     "rsvr-cmd-%02d.usck" /* 接收服务 */
#define AGENT_WSVR_CMD_PATH     "wsvr-cmd-%02d.usck" /* 工作服务 */
#define AGENT_CLI_CMD_PATH      "cli_cmd.usck"       /* 客户端 */

#define agent_cli_cmd_usck_path(conf, path, size)       /* 客户端命令路径 */\
    snprintf(path, size, "%s"AGENT_CLI_CMD_PATH, (conf)->path)
#define agent_lsvr_cmd_usck_path(conf, idx, path, size)      /* 侦听服务命令路径 */\
    snprintf(path, size, "%s"AGENT_LSVR_CMD_PATH, (conf)->path, idx)
#define agent_rsvr_cmd_usck_path(conf, rid, path, size) /* 接收服务命令路径 */\
    snprintf(path, size, "%s"AGENT_RSVR_CMD_PATH, (conf)->path, rid)
#define agent_wsvr_cmd_usck_path(conf, wid, path, size) /* 工作服务命令路径 */\
    snprintf(path, size, "%s"AGENT_WSVR_CMD_PATH, (conf)->path, wid)

/* 配置信息 */
typedef struct
{
    int nid;                                /* 结点ID */
    char path[FILE_NAME_MAX_LEN];           /* 工作路径 */

    struct {
        int max;                            /* 最大并发数 */
        int timeout;                        /* 连接超时时间 */
        int port;                           /* 侦听端口 */
    } connections;

    int lsn_num;                            /* Listen线程数 */
    int agent_num;                          /* Agent线程数 */
    int worker_num;                         /* Worker线程数 */

    queue_conf_t connq;                     /* 连接队列 */
    queue_conf_t recvq;                     /* 接收队列 */
    queue_conf_t sendq;                     /* 发送队列 */
} agent_conf_t;

/* SID列表 */
typedef struct
{
    spinlock_t lock;                        /* 锁 */
    rbt_tree_t *sids;                       /* SID列表 */
} agent_sid_list_t;

/* 代理对象 */
typedef struct
{
    agent_conf_t *conf;                     /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    int cmd_sck_id;                         /* 命令套接字 */

    /* 侦听信息 */
    struct {
        int lsn_sck_id;                     /* 侦听套接字 */
        spinlock_t accept_lock;             /* 侦听锁 */
        uint64_t sid;                       /* Session ID */
        agent_lsvr_t *lsvr;                 /* 侦听对象 */
    } listen;

    thread_pool_t *agents;                  /* Agent线程池 */
    thread_pool_t *listens;                 /* Listen线程池 */
    thread_pool_t *workers;                 /* Worker线程池 */
    agent_reg_t reg[AGENT_MSG_TYPE_MAX];    /* 消息注册 */

    agent_sid_list_t *connections;          /* SID集合(注:数组长度与Agent相等) */

    queue_t **connq;                        /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                        /* 接收队列(注:数组长度与Agent相等) */
    ring_t **sendq;                         /* 发送队列(注:数组长度与Agent相等) */
} agent_cntx_t;

#define AGENT_GET_NODE_ID(ctx) ((ctx)->conf->nid)

/* 内部接口 */
int agent_listen_init(agent_cntx_t *ctx, agent_lsvr_t *lsn, int idx);

int agent_sid_item_add(agent_cntx_t *ctx, uint64_t sid, socket_t *sck);
socket_t *agent_sid_item_del(agent_cntx_t *ctx, uint64_t sid);
int agent_get_aid_by_sid(agent_cntx_t *ctx, uint64_t sid);

/* 外部接口 */
agent_cntx_t *agent_init(agent_conf_t *conf, log_cycle_t *log);
int agent_launch(agent_cntx_t *ctx);
int agent_reg_add(agent_cntx_t *ctx, unsigned int type, agent_reg_cb_t proc, void *args);
void agent_destroy(agent_cntx_t *ctx);

int agent_async_send(agent_cntx_t *ctx, int type, uint64_t sid, void *data, int len);

#endif /*__AGENT_H__*/

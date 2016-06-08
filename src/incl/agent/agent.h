#if !defined(__AGENT_H__)
#define __AGENT_H__

#include "slot.h"
#include "queue.h"
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

/* 流水号->套接字的映射表 */
typedef struct
{
    int len;                                /* 流水号->SCK映射表数组长度 */
    spinlock_t *lock;                       /* 流水号->SCK映射表锁 */
    avl_tree_t **map;                       /* 流水号->SCK映射表 */

    slot_t **slot;                          /* 内存池: 专门用于存储映射表 */
} agent_serial_to_sck_map_t;

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

    queue_t **connq;                        /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                        /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                        /* 发送队列(注:数组长度与Agent相等) */

    agent_serial_to_sck_map_t *serial_to_sck_map; /* 流水号->套接字的映射表 */
} agent_cntx_t;

#define AGENT_GET_NODE_ID(ctx) ((ctx)->conf->nid)

/* 内部接口 */
int agent_listen_init(agent_cntx_t *ctx, agent_lsvr_t *lsn, int idx);

/* 外部接口 */
agent_cntx_t *agent_init(agent_conf_t *conf, log_cycle_t *log);
int agent_launch(agent_cntx_t *ctx);
int agent_reg_add(agent_cntx_t *ctx, unsigned int type, agent_reg_cb_t proc, void *args);
void agent_destroy(agent_cntx_t *ctx);

int agent_send(agent_cntx_t *ctx, int type, uint64_t serial, void *data, int len);

uint64_t agent_gen_sys_serail(uint16_t nid, uint16_t sid, uint32_t seq);

agent_serial_to_sck_map_t *agent_serial_to_sck_map_init(agent_cntx_t *ctx);
int agent_serial_to_sck_map_insert(agent_cntx_t *ctx, agent_flow_t *_flow);
int agent_serial_to_sck_map_query(agent_cntx_t *ctx, uint64_t serial, agent_flow_t *flow);
int agent_serial_to_sck_map_delete(agent_cntx_t *ctx, uint64_t serial);
int _agent_serial_to_sck_map_delete(agent_cntx_t *ctx, uint64_t serial);
int agent_serial_to_sck_map_timeout(agent_cntx_t *ctx);

#endif /*__AGENT_H__*/

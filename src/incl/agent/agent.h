#if !defined(__AGENT_H__)
#define __AGENT_H__

#include "slab.h"
#include "queue.h"
#include "spinlock.h"
#include "avl_tree.h"
#include "shm_queue.h"
#include "agent_comm.h"
#include "thread_pool.h"

/* 宏定义 */
#define AGENT_TMOUT_SCAN_SEC    (15)        /* 超时扫描间隔 */

#define AGENT_SLAB_SIZE         (30 * MB)   /* SLAB内存池大小 */
#define AGENT_RECV_SIZE         (8 * KB)    /* 缓存SIZE(接收缓存) */
#define AGENT_SYNC_SIZE         (AGENT_RECV_SIZE >> 1)  /* 缓存同步SIZE */

#define AGENT_MSG_TYPE_MAX      (0xFF)      /* 消息最大类型 */

/* 命令路径 */
#define AGENT_LSN_CMD_PATH "../temp/agent/lsn_cmd.usck"       /* 侦听线程 */
#define AGENT_RCV_CMD_PATH "../temp/agent/rcv_cmd_%02d.usck"  /* 接收线程 */
#define AGENT_WRK_CMD_PATH "../temp/agent/wrk_cmd_%02d.usck"  /* 工作线程 */

/* 配置信息 */
typedef struct
{
    struct
    {
        int max;                            /* 最大并发数 */
        int timeout;                        /* 连接超时时间 */
        int port;                           /* 侦听端口 */
    } connections;

    int agent_num;                          /* Agent线程数 */
    int worker_num;                         /* Worker线程数 */

    queue_conf_t connq;                     /* 连接队列 */
    queue_conf_t recvq;                     /* 接收队列 */
    queue_conf_t sendq;                     /* 发送队列 */
} agent_conf_t;

/* 代理对象 */
typedef struct
{
    agent_conf_t *conf;                     /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    slab_pool_t *slab;                      /* 内存池 */

    pthread_t lsn_tid;                      /* Listen线程 */
    thread_pool_t *agent_pool;              /* Agent线程池 */
    thread_pool_t *worker_pool;             /* Worker线程池 */
    agent_reg_t reg[AGENT_MSG_TYPE_MAX];    /* 消息注册 */

    queue_t **connq;                        /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                        /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                        /* 发送队列(注:数组长度与Agent相等) */

    int serial_to_sck_map_len;              /* 流水号->SCK映射表数组长度 */
    spinlock_t *serial_to_sck_map_lock;     /* 流水号->SCK映射表锁 */
    avl_tree_t **serial_to_sck_map;         /* 流水号->SCK映射表 */
} agent_cntx_t;

#define agent_connq_used(ctx, idx) queue_used(ctx->connq[idx]) /* 连接队列已用空间 */
#define agent_recvq_used(ctx, idx) queue_used(ctx->recvq[idx]) /* 接收队列已用空间 */
#define agent_sendq_used(ctx, idx) queue_used(ctx->sendq[idx]) /* 发送队列已用空间 */

agent_cntx_t *agent_init(agent_conf_t *conf, log_cycle_t *log);
void agent_destroy(agent_cntx_t *ctx);

int agent_startup(agent_cntx_t *ctx);
int agent_register(agent_cntx_t *ctx, unsigned int type, agent_reg_cb_t proc, void *args);

int agent_send(agent_cntx_t *ctx, int type, uint64_t serial, void *data, int len);

int agent_serial_to_sck_map_init(agent_cntx_t *ctx);
int agent_serial_to_sck_map_insert(agent_cntx_t *ctx, agent_flow_t *_flow);
int agent_serial_to_sck_map_query(agent_cntx_t *ctx, uint64_t serial, agent_flow_t *flow);
int agent_serial_to_sck_map_delete(agent_cntx_t *ctx, uint64_t serial);
int agent_serial_to_sck_map_timeout(agent_cntx_t *ctx);

#endif /*__AGENT_H__*/

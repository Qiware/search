#if !defined(__AGENT_H__)
#define __AGENT_H__

#include "slab.h"
#include "queue.h"
#include "agent_conf.h"
#include "agent_comm.h"
#include "thread_pool.h"

/* 宏定义 */
#define AGENT_TMOUT_SCAN_SEC         (15)        /* 超时扫描间隔 */

#define AGENT_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define AGENT_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define AGENT_SYNC_SIZE  (AGENT_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define AGENT_MSG_TYPE_MAX           (0xFF)      /* 消息最大类型 */

/* 命令路径 */
#define AGENT_LSN_CMD_PATH "../temp/agent/lsn_cmd.usck"       /* 侦听线程 */
#define AGENT_RCV_CMD_PATH "../temp/agent/rcv_cmd_%02d.usck"  /* 接收线程 */
#define AGENT_WRK_CMD_PATH "../temp/agent/wrk_cmd_%02d.usck"  /* 工作线程 */

/* 代理对象 */
typedef struct
{
    agent_conf_t *conf;                          /* 配置信息 */
    log_cycle_t *log;                           /* 日志对象 */
    slab_pool_t *slab;                          /* 内存池 */

    pthread_t lsn_tid;                          /* Listen线程 */
    thread_pool_t *agent_pool;                  /* Agent线程池 */
    thread_pool_t *worker_pool;                 /* Worker线程池 */
    agent_reg_t reg[AGENT_MSG_TYPE_MAX];        /* 消息注册 */

    queue_t **connq;                            /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                            /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                            /* 发送队列(注:数组长度与Agent相等) */
} agent_cntx_t;

#define agent_connq_used(ctx, idx) queue_used(ctx->connq[idx]) /* 已用连接队列空间 */
#define agent_recvq_used(ctx, idx) queue_used(ctx->recvq[idx]) /* 已用接收队列空间 */
#define agent_sendq_used(ctx, idx) queue_used(ctx->sendq[idx]) /* 已用发送队列空间 */

agent_cntx_t *agent_cntx_init(agent_conf_t *conf, log_cycle_t *log);
void agent_cntx_destroy(agent_cntx_t *ctx);

int agent_startup(agent_cntx_t *ctx);
int agent_init_register(agent_cntx_t *ctx);
int agent_register(agent_cntx_t *ctx, unsigned int type, agent_reg_cb_t proc, void *args);

#endif /*__AGENT_H__*/

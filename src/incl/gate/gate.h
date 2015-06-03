#if !defined(__GATE_H__)
#define __GATE_H__

#include "slab.h"
#include "queue.h"
#include "gate_conf.h"
#include "gate_comm.h"
#include "thread_pool.h"

/* 宏定义 */
#define GATE_TMOUT_SCAN_SEC         (15)        /* 超时扫描间隔 */

#define GATE_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define GATE_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define GATE_SYNC_SIZE  (GATE_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define GATE_MSG_TYPE_MAX           (0xFF)      /* 消息最大类型 */

/* 命令路径 */
#define GATE_LSN_CMD_PATH "../temp/gate/lsn_cmd.usck"       /* 侦听线程 */
#define GATE_RCV_CMD_PATH "../temp/gate/rcv_cmd_%02d.usck"  /* 接收线程 */
#define GATE_WRK_CMD_PATH "../temp/gate/wrk_cmd_%02d.usck"  /* 工作线程 */

/* 闸门对象 */
typedef struct
{
    gate_conf_t *conf;                          /* 配置信息 */
    log_cycle_t *log;                           /* 日志对象 */
    slab_pool_t *slab;                          /* 内存池 */

    pthread_t lsn_tid;                          /* Listen线程 */
    thread_pool_t *agent_pool;                  /* Agent线程池 */
    thread_pool_t *worker_pool;                 /* Worker线程池 */
    gate_reg_t reg[GATE_MSG_TYPE_MAX];          /* 消息注册 */

    queue_t **connq;                            /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                            /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                            /* 发送队列(注:数组长度与Agent相等) */
} gate_cntx_t;

#define gate_connq_used(ctx, idx) queue_used(ctx->connq[idx]) /* 已用连接队列空间 */
#define gate_recvq_used(ctx, idx) queue_used(ctx->recvq[idx]) /* 已用接收队列空间 */
#define gate_sendq_used(ctx, idx) queue_used(ctx->sendq[idx]) /* 已用发送队列空间 */

gate_cntx_t *gate_cntx_init(gate_conf_t *conf, log_cycle_t *log);
void gate_cntx_destroy(gate_cntx_t *ctx);

int gate_startup(gate_cntx_t *ctx);
int gate_init_register(gate_cntx_t *ctx);
int gate_register(gate_cntx_t *ctx, unsigned int type, gate_reg_cb_t proc, void *args);

#endif /*__GATE_H__*/

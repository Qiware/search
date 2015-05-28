#if !defined(__PROBE_H__)
#define __PROBE_H__

#include "slab.h"
#include "queue.h"
#include "prob_conf.h"
#include "prob_comm.h"
#include "thread_pool.h"

/* 宏定义 */
#define PROB_TMOUT_SCAN_SEC         (15)        /* 超时扫描间隔 */

#define PROB_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define PROB_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define PROB_SYNC_SIZE  (PROB_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define PROB_MSG_TYPE_MAX           (0xFF)      /* 消息最大类型 */

#define PROB_DEF_CONF_PATH  "../conf/search.xml"/* 默认配置路径 */

/* 命令路径 */
#define PROB_LSN_CMD_PATH "../temp/srch/lsn_cmd.usck"       /* 侦听线程 */
#define PROB_RCV_CMD_PATH "../temp/srch/rcv_cmd_%02d.usck"  /* 接收线程 */
#define PROB_WRK_CMD_PATH "../temp/srch/wrk_cmd_%02d.usck"  /* 工作线程 */

/* 搜索引擎对象 */
typedef struct
{
    prob_conf_t *conf;                          /* 配置信息 */
    log_cycle_t *log;                           /* 日志对象 */
    slab_pool_t *slab;                          /* 内存池 */

    pthread_t lsn_tid;                          /* Listen线程 */
    thread_pool_t *agent_pool;                  /* Agent线程池 */
    thread_pool_t *worker_pool;                 /* Worker线程池 */
    prob_reg_t reg[PROB_MSG_TYPE_MAX];          /* 消息注册 */

    queue_t **connq;                            /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                            /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                            /* 发送队列(注:数组长度与Agent相等) */
} prob_cntx_t;

#define prob_connq_used(ctx, idx) queue_used(ctx->connq[idx]) /* 已用连接队列空间 */
#define prob_recvq_used(ctx, idx) queue_used(ctx->recvq[idx]) /* 已用接收队列空间 */
#define prob_sendq_used(ctx, idx) queue_used(ctx->sendq[idx]) /* 已用发送队列空间 */

prob_cntx_t *prob_cntx_init(char *pname, const char *conf_path);
void prob_cntx_destroy(prob_cntx_t *ctx);
int prob_getopt(int argc, char **argv, prob_opt_t *opt);
int prob_usage(const char *exec);

int prob_startup(prob_cntx_t *ctx);
int prob_init_register(prob_cntx_t *ctx);
int prob_register(prob_cntx_t *ctx, unsigned int type, prob_reg_cb_t proc, void *args);

#endif /*__PROBE_H__*/

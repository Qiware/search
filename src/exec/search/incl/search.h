#if !defined(__SEARCH_H__)
#define __SEARCH_H__

#include "slab.h"
#include "queue.h"
#include "srch_conf.h"
#include "srch_comm.h"
#include "thread_pool.h"

/* 宏定义 */
#define SRCH_TMOUT_SCAN_SEC         (15)        /* 超时扫描间隔 */

#define SRCH_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define SRCH_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define SRCH_SYNC_SIZE  (SRCH_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define SRCH_MSG_TYPE_MAX           (0xFF)      /* 消息最大类型 */

#define SRCH_DEF_CONF_PATH  "../conf/search.xml"/* 默认配置路径 */

/* 命令路径 */
#define SRCH_LSN_CMD_PATH "../temp/srch/lsn_cmd.usck"       /* 侦听线程 */
#define SRCH_RCV_CMD_PATH "../temp/srch/rcv_cmd_%02d.usck"  /* 接收线程 */
#define SRCH_WRK_CMD_PATH "../temp/srch/wrk_cmd_%02d.usck"  /* 工作线程 */

/* 搜索引擎对象 */
typedef struct
{
    srch_conf_t *conf;                          /* 配置信息 */
    log_cycle_t *log;                           /* 日志对象 */
    slab_pool_t *slab;                          /* 内存池 */

    pthread_t lsn_tid;                          /* Listen线程 */
    thread_pool_t *agent_pool;                  /* Agent线程池 */
    thread_pool_t *worker_pool;                 /* Worker线程池 */
    srch_reg_t reg[SRCH_MSG_TYPE_MAX];          /* 消息注册 */

    queue_t **connq;                            /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                            /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                            /* 发送队列(注:数组长度与Agent相等) */
} srch_cntx_t;

#define srch_connq_used(ctx, idx) queue_used(ctx->connq[idx]) /* 已用连接队列空间 */
#define srch_recvq_used(ctx, idx) queue_used(ctx->recvq[idx]) /* 已用接收队列空间 */
#define srch_sendq_used(ctx, idx) queue_used(ctx->sendq[idx]) /* 已用发送队列空间 */

srch_cntx_t *srch_cntx_init(char *pname, const char *conf_path);
void srch_cntx_destroy(srch_cntx_t *ctx);
int srch_getopt(int argc, char **argv, srch_opt_t *opt);
int srch_usage(const char *exec);

int srch_startup(srch_cntx_t *ctx);
int srch_init_register(srch_cntx_t *ctx);
int srch_register(srch_cntx_t *ctx, unsigned int type, srch_reg_cb_t proc, void *args);
#endif /*__SEARCH_H__*/

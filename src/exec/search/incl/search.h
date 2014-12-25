#if !defined(__SEARCH_H__)
#define __SEARCH_H__

#include "queue.h"
#include "srch_conf.h"
#include "srch_comm.h"
#include "thread_pool.h"

/* 宏定义 */
#define SRCH_TMOUT_SCAN_SEC         (15)    /* 超时扫描间隔 */
#define SRCH_CONNECT_TMOUT_SEC      (00)    /* 连接超时时间 */

#define SRCH_THD_MAX_NUM            (64)    /* 最大线程数 */
#define SRCH_THD_DEF_NUM            (05)    /* 默认线程数 */
#define SRCH_THD_MIN_NUM            (01)    /* 最小线程数 */

#define SRCH_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define SRCH_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define SRCH_SYNC_SIZE  (SRCH_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define SRCH_CONN_MAX_NUM           (1024)  /* 最大网络连接数 */
#define SRCH_CONN_DEF_NUM           (128)   /* 默认网络连接数 */
#define SRCH_CONN_MIN_NUM           (1)     /* 最小网络连接数 */
#define SRCH_CONN_TMOUT_SEC         (15)    /* 连接超时时间(秒) */

#define SRCH_CONNQ_LEN              (10000) /* 连接队列长度 */
#define SRCH_RECVQ_LEN              (10000) /* 接收队列长度 */
#define SRCH_RECVQ_SIZE             (2 * KB)/* 接收队列SIZE */

#define SRCH_MSG_TYPE_MAX           (0xFF)  /* 消息最大类型 */

#define SRCH_DEF_CONF_PATH  "../conf/search.xml"   /* 默认配置路径 */

/* 命令路径 */
#define SRCH_LSN_CMD_PATH "../temp/srch/lsn_cmd.usck"       /* 侦听线程 */
#define SRCH_RCV_CMD_PATH "../temp/srch/rcv_cmd_%02d.usck"  /* 接收线程 */
#define SRCH_WRK_CMD_PATH "../temp/srch/wrk_cmd_%02d.usck"  /* 工作线程 */

/* 搜索引擎对象 */
typedef struct
{
    srch_conf_t *conf;                      /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    slab_pool_t *slab;                      /* 内存池 */

    pthread_t lsn_tid;                      /* Listen线程 */
    thread_pool_t *agents;                  /* Agent线程池 */
    thread_pool_t *workers;                 /* Worker线程池 */
    srch_reg_t reg[SRCH_MSG_TYPE_MAX];      /* 消息注册 */

    queue_t **connq;                        /* 连接队列(注:数组长度与Agent相等) */
    queue_t **recvq;                        /* 接收队列(注:数组长度与Agent相等) */
    queue_t **sendq;                        /* 发送队列(注:数组长度与Agent相等) */
} srch_cntx_t;

#define srch_connq_space(ctx, idx) queue_space(&ctx->connq[idx]->queue) /* 连接队列空间 */
#define srch_recvq_space(ctx, idx) queue_space(&ctx->recvq[idx]->queue) /* 接收队列空间 */
#define srch_sendq_space(ctx, idx) queue_space(&ctx->sendq[idx]->queue) /* 发送队列空间 */

srch_cntx_t *srch_cntx_init(char *pname, const char *conf_path);
void srch_cntx_destroy(srch_cntx_t *ctx);
int srch_getopt(int argc, char **argv, srch_opt_t *opt);
int srch_usage(const char *exec);

int srch_startup(srch_cntx_t *ctx);
int srch_init_register(srch_cntx_t *ctx);
int srch_register(srch_cntx_t *ctx, uint32_t type, srch_reg_cb_t cb, void *args);
#endif /*__SEARCH_H__*/

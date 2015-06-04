#if !defined(__SDTP_SEND_H__)
#define __SDTP_SEND_H__

#include "dsnd_ssvr.h"

/* 配置信息 */
typedef struct
{
    int devid;                          /* 设备ID: 唯一值 */
    char name[SDTP_NAME_MAX_LEN];       /* 发送端名称 */

    sdtp_auth_conf_t auth;              /* 鉴权信息 */

    char ipaddr[IP_ADDR_MAX_LEN];       /* 服务端IP地址 */
    int port;                           /* 服务端端口号 */

    int send_thd_num;                   /* 发送线程数 */
    int work_thd_num;                   /* 工作线程数 */

    size_t send_buff_size;              /* 发送缓存大小 */
    size_t recv_buff_size;              /* 接收缓存大小 */

    sdtp_cpu_conf_t cpu;                /* CPU亲和性配置 */

    sdtp_queue_conf_t sendq;            /* 发送队列配置 */
    queue_conf_t recvq;                 /* 接收队列配置 */
} dsnd_conf_t;

/* 全局信息 */
typedef struct
{
    dsnd_conf_t conf;              /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */
    slab_pool_t *slab;                  /* 内存池对象 */

    thread_pool_t *sendtp;              /* 发送线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */

    sdtp_reg_t reg[SDTP_TYPE_MAX];      /* 回调注册对象(TODO: 如果类型过多，可构造平衡二叉树) */
    queue_t **recvq;                    /* 接收队列 */
} dsnd_cntx_t;

/* 内部接口 */
int dsnd_ssvr_init(dsnd_cntx_t *ctx, dsnd_ssvr_t *ssvr, int tidx);
void *dsnd_ssvr_routine(void *_ctx);

int dsnd_worker_init(dsnd_cntx_t *ctx, sdtp_worker_t *wrk, int tidx);
void *dsnd_worker_routine(void *_ctx);

sdtp_worker_t *dsnd_worker_get_by_idx(dsnd_cntx_t *ctx, int idx);

/* 对外接口 */
dsnd_cntx_t *dsnd_init(const dsnd_conf_t *conf, log_cycle_t *log);
int dsnd_start(dsnd_cntx_t *ctx);
int dsnd_register(dsnd_cntx_t *ctx, int type, sdtp_reg_cb_t proc, void *args);
int dsnd_destroy(dsnd_cntx_t *ctx);

#endif /*__SDTP_SEND_H__*/

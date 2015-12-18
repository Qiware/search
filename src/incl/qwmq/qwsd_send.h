#if !defined(__QWSD_SEND_H__)
#define __QWSD_SEND_H__

#include "qwsd_ssvr.h"

/* 配置信息 */
typedef struct
{
    int nodeid;                         /* 设备ID: 唯一值 */
    char path[FILE_LINE_MAX_LEN];       /* 工作路径 */

    struct
    {
        char usr[QWMQ_USR_MAX_LEN];     /* 用户名 */
        char passwd[QWMQ_PWD_MAX_LEN];  /* 登录密码 */
    } auth;                             /* 鉴权信息 */

    char ipaddr[IP_ADDR_MAX_LEN];       /* 服务端IP地址 */
    int port;                           /* 服务端端口号 */

    int send_thd_num;                   /* 发送线程数 */
    int work_thd_num;                   /* 工作线程数 */

    size_t send_buff_size;              /* 发送缓存大小 */
    size_t recv_buff_size;              /* 接收缓存大小 */

    qwmq_cpu_conf_t cpu;                /* CPU亲和性配置 */

    queue_conf_t sendq;                 /* 发送队列配置 */
    queue_conf_t recvq;                 /* 接收队列配置 */
} qwsd_conf_t;

/* 全局信息 */
typedef struct
{
    qwsd_conf_t conf;                   /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */
    slab_pool_t *slab;                  /* 内存池对象 */

    thread_pool_t *sendtp;              /* 发送线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */

    qwmq_reg_t reg[QWMQ_TYPE_MAX];      /* 回调注册对象(TODO: 如果类型过多，可构造平衡二叉树) */
    queue_t **recvq;                    /* 接收队列(数组长度与send_thd_num一致) */
} qwsd_cntx_t;

/* 内部接口 */
int qwsd_ssvr_init(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, int tidx);
void *qwsd_ssvr_routine(void *_ctx);

int qwsd_worker_init(qwsd_cntx_t *ctx, qwmq_worker_t *wrk, int tidx);
void *qwsd_worker_routine(void *_ctx);

qwmq_worker_t *qwsd_worker_get_by_idx(qwsd_cntx_t *ctx, int idx);

/* 对外接口 */
qwsd_cntx_t *qwsd_init(const qwsd_conf_t *conf, log_cycle_t *log);
int qwsd_launch(qwsd_cntx_t *ctx);
int qwsd_register(qwsd_cntx_t *ctx, int type, qwmq_reg_cb_t proc, void *args);

#endif /*__QWSD_SEND_H__*/

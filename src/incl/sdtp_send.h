#if !defined(__SDTP_SEND_H__)
#define __SDTP_SEND_H__

#include "sdtp_ssvr.h"

/* 全局信息 */
typedef struct
{
    sdtp_ssvr_conf_t conf;              /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */
    slab_pool_t *slab;                  /* 内存池对象 */

    thread_pool_t *sendtp;              /* 发送线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */

    sdtp_reg_t reg[SDTP_TYPE_MAX];      /* 回调注册对象(TODO: 如果类型过多，可使用平衡二叉树构造) */
    queue_t **recvq;                    /* 接收队列 */
} sdtp_sctx_t;

/* 内部接口 */
int sdtp_ssvr_init(sdtp_sctx_t *ctx, sdtp_ssvr_t *ssvr, int tidx);
void *sdtp_ssvr_routine(void *_ctx);

int sdtp_swrk_init(sdtp_sctx_t *ctx, sdtp_worker_t *wrk, int tidx);
void *sdtp_swrk_routine(void *_ctx);

/* 对外接口 */
sdtp_sctx_t *sdtp_send_init(const sdtp_ssvr_conf_t *conf, log_cycle_t *log);
int sdtp_send_startup(sdtp_sctx_t *ctx);
int sdtp_send_register(sdtp_sctx_t *ctx, int type, sdtp_reg_cb_t proc, void *args);
int sdtp_send_destroy(sdtp_sctx_t *ctx);

#endif /*__SDTP_SEND_H__*/

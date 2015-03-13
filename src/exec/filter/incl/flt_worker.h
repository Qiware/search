#if !defined(__FLT_WORKER_H__)
#define __FLT_WORKER_H__

#include "redis.h"
#include "flt_priv.h"

/* 工作对象 */
typedef struct
{
    int tidx;
    log_cycle_t *log;                       /* 日志对象 */
    redis_clst_t *redis;                    /* Redis集群 */

    flt_webpage_info_t info;                /* 网页信息 */
} flt_worker_t;

void *flt_worker_routine(void *_ctx);

#endif /*__FLT_WORKER_H__*/

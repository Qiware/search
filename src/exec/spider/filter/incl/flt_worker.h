#if !defined(__FLT_WORKER_H__)
#define __FLT_WORKER_H__

#include "redis.h"
#include "flt_priv.h"

/* 工作对象 */
typedef struct
{
    int id;                                 /* 对象ID */
    log_cycle_t *log;                       /* 日志对象 */
    redisContext *redis;                    /* Redis集群 */

    flt_webpage_info_t info;                /* 网页信息 */
} flt_worker_t;

void *flt_worker_routine(void *_ctx);

/* 通过索引获取WORKER对象 */
#define flt_worker_get_by_idx(ctx, idx) (&(ctx)->worker[idx])

#endif /*__FLT_WORKER_H__*/

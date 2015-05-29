#if !defined(__PROB_WORKER_H__)
#define __PROB_WORKER_H__

#include "queue.h"
#include "probe.h"

/* Worker对象 */
typedef struct
{
    int tidx;                       /* 线程IDX */
    log_cycle_t *log;               /* 日志对象 */
} prob_worker_t;

void *prob_worker_routine(void *_ctx);

int prob_worker_init(prob_cntx_t *ctx, prob_worker_t *worker, int idx);
int prob_worker_destroy(prob_worker_t *worker);

#endif /*__PROB_WORKER_H__*/

#if !defined(__GATE_WORKER_H__)
#define __GATE_WORKER_H__

#include "gate.h"
#include "queue.h"

/* Worker对象 */
typedef struct
{
    int tidx;                       /* 线程IDX */
    log_cycle_t *log;               /* 日志对象 */
} gate_worker_t;

void *gate_worker_routine(void *_ctx);

int gate_worker_init(gate_cntx_t *ctx, gate_worker_t *worker, int idx);
int gate_worker_destroy(gate_worker_t *worker);

#endif /*__GATE_WORKER_H__*/

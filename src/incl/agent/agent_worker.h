#if !defined(__AGENT_WORKER_H__)
#define __AGENT_WORKER_H__

#include "agent.h"
#include "queue.h"

/* Worker对象 */
typedef struct
{
    int tidx;                       /* 线程IDX */
    log_cycle_t *log;               /* 日志对象 */
} agent_worker_t;

void *agent_worker_routine(void *_ctx);

int agent_worker_init(agent_cntx_t *ctx, agent_worker_t *worker, int idx);
int agent_worker_destroy(agent_worker_t *worker);

#endif /*__AGENT_WORKER_H__*/

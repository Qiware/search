#if !defined(__SRCH_WORKER_H__)
#define __SRCH_WORKER_H__

#include "queue.h"
#include "search.h"

/* Worker对象 */
typedef struct
{
    int tidx;                       /* 线程IDX */
    lqueue_t add_sck;               /* 新增连接队列 */
} srch_worker_t;

void *srch_worker_routine(void *_ctx);

int srch_worker_init(srch_cntx_t *ctx, srch_worker_t *worker);
int srch_worker_destroy(srch_worker_t *worker);

#endif /*__SRCH_WORKER_H__*/

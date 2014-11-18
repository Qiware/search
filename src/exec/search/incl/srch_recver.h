#if !defined(__SRCH_RECVER_H__)
#define __SRCH_RECVER_H__

#include "search.h"

typedef struct
{
    int tidx;                               /* 线程IDX */
} srch_recver_t;

void *srch_recver_routine(void *_ctx);

int srch_recver_init(srch_cntx_t *ctx, srch_recver_t *recver);
int srch_recver_destroy(srch_recver_t *recver);

#endif /*__SRCH_RECVER_H__*/

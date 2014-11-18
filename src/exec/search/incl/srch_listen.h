#if !defined(__SRCH_LISTEN_H__)
#define __SRCH_LISTEN_H__

#include "search.h"

/* 侦听线程 */
typedef struct
{
    int tid;                    /* 线程ID */
    int lsn_fd;                 /* 侦听FD */
} srch_listen_t;

void *srch_listen_routine(void *_ctx);

int srch_listen_init(srch_cntx_t *ctx, srch_listen_t *lsn);
int srch_listen_destroy(srch_listen_t *lsn);

#endif /*__SRCH_LISTEN_H__*/

#if !defined(__SRCH_LISTEN_H__)
#define __SRCH_LISTEN_H__

#include <stdint.h>

#include "search.h"

/* 侦听线程 */
typedef struct
{
    int tid;                    /* 线程ID */
    int lsn_sck_id;             /* 侦听套接字 */
    int cmd_sck_id;             /* 命令套接字 */
    log_cycle_t *log;           /* 日志对象 */
    uint64_t sck_serial;        /* SCK流水号 */
} srch_listen_t;

void *srch_listen_routine(void *_ctx);

int srch_listen_destroy(srch_listen_t *lsn);

#endif /*__SRCH_LISTEN_H__*/

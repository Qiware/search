#if !defined(__PROB_LISTEN_H__)
#define __PROB_LISTEN_H__

#include <stdint.h>

#include "search.h"

/* 侦听线程 */
typedef struct
{
    int tid;                    /* 线程ID */
    int lsn_sck_id;             /* 侦听套接字 */
    int cmd_sck_id;             /* 命令套接字 */
    log_cycle_t *log;           /* 日志对象 */
    unsigned long long serial;  /* SCK流水号 */
} prob_listen_t;

void *prob_listen_routine(void *_ctx);

#endif /*__PROB_LISTEN_H__*/

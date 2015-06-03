#if !defined(__GATE_LISTEN_H__)
#define __GATE_LISTEN_H__

#include "log.h"
#include <stdint.h>

/* 侦听线程 */
typedef struct
{
    int tid;                    /* 线程ID */
    int lsn_sck_id;             /* 侦听套接字 */
    int cmd_sck_id;             /* 命令套接字 */
    log_cycle_t *log;           /* 日志对象 */
    unsigned long long serial;  /* SCK流水号 */
} gate_listen_t;

void *gate_listen_routine(void *_ctx);

#endif /*__GATE_LISTEN_H__*/

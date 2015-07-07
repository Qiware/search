#if !defined(__AGENT_LISTEN_H__)
#define __AGENT_LISTEN_H__

#include "log.h"
#include <stdint.h>

/* 侦听线程 */
typedef struct
{
    int id;                                 /* 线程ID(从0开始计数) */

    int cmd_sck_id;                         /* 命令套接字 */
    log_cycle_t *log;                       /* 日志对象 */
} agent_lsvr_t;

void *agent_listen_routine(void *_ctx);

#endif /*__AGENT_LISTEN_H__*/

#if !defined(__AGENTD_CONF_H__)
#define __AGENTD_CONF_H__

#include "comm.h"
#include "agent.h"
#include "sdtp_send.h"

/* 代理配置 */
typedef struct
{
    int log_level;                  /* 日志级别 */

    agent_conf_t agent;             /* 代理配置 */
    dsnd_conf_t sdtp;               /* SDTP配置 */
} agentd_conf_t;

agentd_conf_t *agentd_conf_load(const char *path, log_cycle_t *log);

#endif /*__AGENTD_CONF_H__*/

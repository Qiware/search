#if !defined(__LSND_CONF_H__)
#define __LSND_CONF_H__

#include "comm.h"
#include "agent.h"
#include "rtsd_send.h"

/* 代理配置 */
typedef struct
{
    int log_level;                  /* 日志级别 */

    agent_conf_t agent;             /* 代理配置 */
    rtsd_conf_t to_frwd;            /* SDTP配置 */
} lsnd_conf_t;

lsnd_conf_t *lsnd_load_conf(const char *path, log_cycle_t *log);

#endif /*__AGENTD_CONF_H__*/

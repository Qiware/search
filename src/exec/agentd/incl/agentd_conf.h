#if !defined(__AGENTD_CONF_H__)
#define __AGENTD_CONF_H__

#include "comm.h"
#include "agent.h"
#if defined(__RTTP_SUPPORT__)
#include "rtsd_send.h"
#else /*__RTTP_SUPPORT__*/
#include "sdsd_send.h"
#endif /*__RTTP_SUPPORT__*/

/* 代理配置 */
typedef struct
{
    int log_level;                  /* 日志级别 */

    agent_conf_t agent;             /* 代理配置 */
#if defined(__RTTP_SUPPORT__)
    rtsd_conf_t sdtp;               /* SDTP配置 */
#else /*__RTTP_SUPPORT__*/
    sdsd_conf_t sdtp;               /* SDTP配置 */
#endif /*__RTTP_SUPPORT__*/
} agentd_conf_t;

agentd_conf_t *agentd_load_conf(const char *path, log_cycle_t *log);

#endif /*__AGENTD_CONF_H__*/

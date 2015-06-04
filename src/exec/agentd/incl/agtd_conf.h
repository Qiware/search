#if !defined(__AGTD_CONF_H__)
#define __AGTD_CONF_H__

#include "comm.h"
#include "sdtp_send.h"
#include "agent_conf.h"

/* 代理配置 */
typedef struct
{
    int log_level;                  /* 日志级别 */

    agent_conf_t gate;               /* 探针配置 */
    dsnd_conf_t sdtp;               /* SDTP配置 */
} agtd_conf_t;

agtd_conf_t *agtd_conf_load(const char *path, log_cycle_t *log);

#endif /*__AGTD_CONF_H__*/

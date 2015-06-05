#if !defined(__FRWD_H__)
#define __FRWD_H__

#include "sdsd_cli.h"
#include "sdsd_ssvr.h"

#define AGTD_SHM_SENDQ_PATH     "../temp/agentd/send.shmq"  /* 发送队列路径 */

/* 配置信息 */
typedef struct
{
    sdsd_conf_t sdtp;               /* SDTP配置 */
} frwd_conf_t;

/* 全局对象 */
typedef struct
{
    frwd_conf_t conf;               /* 配置信息 */
    log_cycle_t *log;               /* 日志对象 */
    shm_queue_t *send_to_agentd;    /* 发送至Agentd */
    sdsd_cntx_t *sdtp;              /* SDTP对象 */
} frwd_cntx_t;

#endif /*__FRWD_H__*/

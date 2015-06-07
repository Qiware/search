#if !defined(__AGENTD_H__)
#define __AGENTD_H__

#include "log.h"
#include "comm.h"
#include "agentd.h"
#if defined(__RTTP_SUPPORT__)
#include "rtsd_cli.h"
#else /*__RTTP_SUPPORT__*/
#include "sdsd_cli.h"
#endif /*__RTTP_SUPPORT__*/
#include "shm_queue.h"
#include "agentd_conf.h"

#define AGTD_DEF_CONF_PATH      "../conf/agentd.xml"        /* 默认配置路径 */
#define AGTD_SHM_SENDQ_PATH     "../temp/agentd/send.shmq"  /* 发送队列路径 */

/* 错误码 */
typedef enum
{
    AGTD_OK = 0                             /* 正常 */
    , AGTD_SHOW_HELP                        /* 显示帮助信息 */

    , AGTD_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} AGTD_err_code_e;


/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} agentd_opt_t;

typedef struct
{
    agentd_conf_t *conf;                    /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */

#if defined(__RTTP_SUPPORT__)
    rtsd_cli_t *send_to_invtd;              /* SDTP服务(发送至倒排服务) */
#else /*__RTTP_SUPPORT__*/
    sdsd_cli_t *send_to_invtd;              /* SDTP服务(发送至倒排服务) */
#endif /*__RTTP_SUPPORT__*/
    agent_cntx_t *agent;                    /* 代理服务 */
    shm_queue_t *sendq;                     /* 发送队列 */
} agentd_cntx_t;

int agentd_getopt(int argc, char **argv, agentd_opt_t *opt);
int agentd_usage(const char *exec);
log_cycle_t *agentd_init_log(char *fname);
void *agentd_dist_routine(void *_ctx);

#endif /*__AGENTD_H__*/

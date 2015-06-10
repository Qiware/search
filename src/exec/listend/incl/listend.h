#if !defined(__LISTEND_H__)
#define __LISTEND_H__

#include "log.h"
#include "comm.h"
#include "listend.h"
#include "rtsd_cli.h"
#include "shm_queue.h"
#include "lsnd_conf.h"

#define LSND_DEF_CONF_PATH      "../conf/listend.xml"     /* 默认配置路径 */
#define LSND_SHM_SENDQ_PATH     "../temp/lsnd/send.shmq"  /* 发送队列路径 */

/* 错误码 */
typedef enum
{
    LSND_OK = 0                             /* 正常 */
    , LSND_SHOW_HELP                        /* 显示帮助信息 */

    , LSND_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} lsnd_err_code_e;


/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} lsnd_opt_t;

typedef struct
{
    lsnd_conf_t *conf;                      /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */

    rtsd_cli_t *send_to_invtd;              /* SDTP服务(发送至倒排服务) */
    agent_cntx_t *agent;                    /* 代理服务 */
    shm_queue_t *sendq;                     /* 发送队列 */
} lsnd_cntx_t;

int lsnd_getopt(int argc, char **argv, lsnd_opt_t *opt);
int lsnd_usage(const char *exec);
log_cycle_t *lsnd_init_log(char *fname);
void *lsnd_dist_routine(void *_ctx);

#endif /*__LISTEND_H__*/

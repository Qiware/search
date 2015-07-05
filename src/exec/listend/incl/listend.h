#if !defined(__LISTEND_H__)
#define __LISTEND_H__

#include "log.h"
#include "comm.h"
#include "listend.h"
#include "rtsd_cli.h"
#include "shm_queue.h"
#include "lsnd_conf.h"

#define LSND_DEF_CONF_PATH      "../conf/listend.xml"     /* 默认配置路径 */

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

/* 分发对象 */
typedef struct
{
    int cmd_sck_id;                         /* 命令套接字 */
    fd_set rdset;                           /* 可读集合 */
} lsnd_dsvr_t;

/* 全局对象 */
typedef struct
{
    lsnd_conf_t conf;                       /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */

    lsnd_dsvr_t dsvr;                       /* 分发服务 */

    rtsd_cli_t *send_to_invtd;              /* SDTP服务(发送至倒排服务) */
    agent_cntx_t *agent;                    /* 代理服务 */
    shm_queue_t **distq;                    /* 分发队列 */
} lsnd_cntx_t;

int lsnd_getopt(int argc, char **argv, lsnd_opt_t *opt);
int lsnd_usage(const char *exec);
int lsnd_attach_distq(lsnd_cntx_t *ctx);

int lsnd_dsvr_init(lsnd_cntx_t *ctx);
void *lsnd_dsvr_routine(void *_ctx);

#endif /*__LISTEND_H__*/

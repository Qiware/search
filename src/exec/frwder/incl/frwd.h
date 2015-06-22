#if !defined(__FRWD_H__)
#define __FRWD_H__

#include "rtsd_cli.h"
#include "rtsd_ssvr.h"
#include "frwd_conf.h"

typedef enum
{
    FRWD_OK                                 /* 正常 */
    , FRWD_SHOW_HELP                        /* 显示帮助 */

    , FRWD_ERR = ~0x7fffffff                /* 异常 */
} frwd_err_code_e;

/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} frwd_opt_t;

/* 全局对象 */
typedef struct
{
    int cmd_sck_id;                         /* 命令套接字 */
    frwd_conf_t conf;                       /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    shm_queue_t *send_to_listend;           /* 发送至Listend */
    rtsd_cntx_t *rttp;                      /* RTTP对象 */
} frwd_cntx_t;

int frwd_getopt(int argc, char **argv, frwd_opt_t *opt);
int frwd_usage(const char *exec);
frwd_cntx_t *frwd_init(const frwd_conf_t *conf);
int frwd_startup(frwd_cntx_t *frwd);
int frwd_set_reg(frwd_cntx_t *frwd);

#endif /*__FRWD_H__*/

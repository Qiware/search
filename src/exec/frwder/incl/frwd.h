#if !defined(__FRWD_H__)
#define __FRWD_H__

#include "shm_queue.h"
#include "rtsd_send.h"
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
    int log_level;                          /* 日志级别 */
    bool isdaemon;                          /* 是否后台运行 */
    char *conf_path;                        /* 配置路径 */
    char *log_key_path;                     /* 日志键值路径 */
} frwd_opt_t;

/* 全局对象 */
typedef struct
{
    int cmd_sck_id;                         /* 命令套接字 */
    frwd_conf_t conf;                       /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    rtsd_cntx_t *rtmq;                      /* RTMQ对象 */
} frwd_cntx_t;

int frwd_getopt(int argc, char **argv, frwd_opt_t *opt);
int frwd_usage(const char *exec);
log_cycle_t *frwd_init_log(const char *pname, int log_level, const char *log_key_path);
frwd_cntx_t *frwd_init(const frwd_conf_t *conf, log_cycle_t *log);
int frwd_launch(frwd_cntx_t *frwd);
int frwd_set_reg(frwd_cntx_t *frwd);

#endif /*__FRWD_H__*/

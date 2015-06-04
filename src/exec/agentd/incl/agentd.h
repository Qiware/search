#if !defined(__AGENTD_H__)
#define __AGENTD_H__

#include "log.h"
#include "comm.h"
#include "agent.h"
#include "dsnd_cli.h"
#include "agtd_conf.h"

#define AGTD_DEF_CONF_PATH  "../conf/agentd.xml"/* 默认配置路径 */

/* 错误码 */
typedef enum
{
    AGTD_OK = 0                             /* 正常 */
    , AGTD_SHOW_HELP                        /* 显示帮助信息 */
    , AGTD_DONE                             /* 完成 */
    , AGTD_SCK_AGAIN                        /* 出现EAGAIN提示 */
    , AGTD_SCK_CLOSE                        /* 套接字关闭 */

    , AGTD_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} AGTD_err_code_e;


/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} agtd_opt_t;

typedef struct
{
    agtd_conf_t *conf;                      /* 配置信息 */

    dsnd_cli_t *sdtp;                       /* SDTP服务 */
    agent_cntx_t *agent;                    /* 代理服务 */
    log_cycle_t *log;                       /* 日志对象 */

    int len;                                /* 业务请求树长 */
    avl_tree_t **serial_to_sck_map;         /* 序列与SCK的映射 */
} agtd_cntx_t;

int agtd_getopt(int argc, char **argv, agtd_opt_t *opt);
int agtd_usage(const char *exec);
log_cycle_t *agtd_init_log(char *fname);

#endif /*__AGENTD_H__*/

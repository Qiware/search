#if !defined(__INVERTD_H__)
#define __INVERTD_H__

#include "log.h"
#include "sdtp_recv.h"
#include "invtd_conf.h"
#include "invert_tab.h"

/* 全局信息 */
typedef struct
{
    invtd_conf_t conf;                      /* 配置信息 */

    log_cycle_t *log;                       /* 日志对象 */
    invt_tab_t *tab;                        /* 倒排表 */
    sdtp_rctx_t *sdtp;                      /* SDTP服务 */
    sdtp_rcli_t *sdtp_rcli;                 /* SDTP客户端 */
} invtd_cntx_t;

/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} invtd_opt_t;

invtd_cntx_t *invtd_init(const char *conf_path);
int invtd_startup(invtd_cntx_t *ctx);

#endif /*__INVERTD_H__*/

#if !defined(__INVERTD_H__)
#define __INVERTD_H__

#include "log.h"
#include "rtrd_recv.h"
#include "invtab.h"
#include "invtd_conf.h"

/* 全局信息 */
typedef struct
{
    log_cycle_t *log;                       /* 日志对象 */
    invtd_conf_t conf;                      /* 配置信息 */

    invt_tab_t *invtab;                     /* 倒排表 */
    pthread_rwlock_t invtab_lock;           /* 倒排表锁 */

    rtrd_cntx_t *rtrd;                      /* RTRD服务 */
    rtrd_cli_t *rtrd_cli;                   /* RTRD客户端 */
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

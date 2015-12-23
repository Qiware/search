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
} invtd_cntx_t;

/* 输入参数 */
typedef struct
{
    int log_level;                          /* 日志级别 */
    bool isdaemon;                          /* 是否后台运行 */
    char *conf_path;                        /* 配置文件路径 */
    char *log_key_path;                     /* 日志键值路径 */
} invtd_opt_t;

invtd_cntx_t *invtd_init(const invtd_conf_t *conf, log_cycle_t *log);
int invtd_launch(invtd_cntx_t *ctx);

#endif /*__INVERTD_H__*/

#if !defined(__INVTD_CONF_H__)
#define __INVTD_CONF_H__

#include "log.h"
#include "rtmq_proxy.h"

/* 配置信息 */
typedef struct
{
    int nid;                            /* 结点ID */
    int gid;                            /* 分组ID */
    char path[FILE_LINE_MAX_LEN];       /* 工作路径 */
    int invt_tab_max;                   /* 倒排表长度 */
    rtmq_proxy_conf_t frwder;           /* FRWDER配置 */
} invtd_conf_t;

int invtd_conf_load(const char *path, invtd_conf_t *conf, log_cycle_t *log);

#endif /*__INVTD_CONF_H__*/

#if !defined(__FRWD_CONF_H__)
#define __FRWD_CONF_H__

#include "sck.h"
#include "comm.h"
#include "rtsd_send.h"

/* 配置信息 */
typedef struct
{
    int log_level;                          /* 日志级别 */
    char to_listend[FILE_NAME_MAX_LEN];     /* 发送至Listend */
    rtsd_conf_t conn_invtd;                 /* RTTP配置 */
} frwd_conf_t;

int frwd_load_conf(const char *path, frwd_conf_t *conf);
#endif /*__FRWD_CONF_H__*/

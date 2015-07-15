#if !defined(__FRWD_CONF_H__)
#define __FRWD_CONF_H__

#include "sck.h"
#include "comm.h"
#include "xml_tree.h"
#include "rtsd_send.h"

/* 配置信息 */
typedef struct
{
    char name[NODE_MAX_LEN];                /* 结点名 */
    int log_level;                          /* 日志级别 */
    char lsnd_name[FILE_NAME_MAX_LEN];      /* 侦听服务名 */
    rtsd_conf_t conn_invtd;                 /* RTTP配置 */
} frwd_conf_t;

int frwd_load_conf(const char *name, const char *path, frwd_conf_t *conf);
int frwd_conf_load_frwder(xml_tree_t *xml, const char *path, rtsd_conf_t *conf);

#endif /*__FRWD_CONF_H__*/

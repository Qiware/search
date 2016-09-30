#if !defined(__FRWD_CONF_H__)
#define __FRWD_CONF_H__

#include "sck.h"
#include "comm.h"
#include "xml_tree.h"
#include "rtmq_recv.h"
#include "rtmq_proxy.h"

/* 配置信息 */
typedef struct
{
    int nid;                                /* 结点名ID */
    char name[NODE_MAX_LEN];                /* 结点名 */
    rtmq_conf_t backend;                    /* Backend配置 */
    rtmq_conf_t forward;                    /* Forward配置 */
} frwd_conf_t;

int frwd_load_conf(const char *path, frwd_conf_t *conf, log_cycle_t *log);

#endif /*__FRWD_CONF_H__*/

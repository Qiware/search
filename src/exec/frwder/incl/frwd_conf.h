#if !defined(__FRWD_CONF_H__)
#define __FRWD_CONF_H__

#include "sck.h"
#include "comm.h"
#include "xml_tree.h"
#include "rtsd_send.h"
#include "rtrd_recv.h"

/* 配置信息 */
typedef struct
{
    int nid;                                /* 结点名ID */
    char name[NODE_MAX_LEN];                /* 结点名 */
    char lsnd_name[FILE_NAME_MAX_LEN];      /* 侦听服务名 */
    rtsd_conf_t upload;                     /* upload配置 */
    rtrd_conf_t download;                   /* download配置 */
} frwd_conf_t;

int frwd_load_conf(const char *path, frwd_conf_t *conf, log_cycle_t *log);

#endif /*__FRWD_CONF_H__*/

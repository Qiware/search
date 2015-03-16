#if !defined(__MON_CONF_H__)
#define __MON_CONF_H__

#include "common.h"

#define MON_DEF_CONF_PATH "../conf/monitor.xml"

typedef struct
{
    /* 爬虫配置 */
    struct
    {
        char ip[IP_ADDR_MAX_LEN];   /* IP地址 */
        int port;                   /* 端口号 */
    } crwl;

    /* 过滤配置 */
    struct
    {
        char ip[IP_ADDR_MAX_LEN];   /* IP地址 */
        int port;                   /* 端口号 */
    } filter;
} mon_conf_t;

mon_conf_t *mon_conf_load(const char *path);

#endif /*__MON_CONF_H__*/

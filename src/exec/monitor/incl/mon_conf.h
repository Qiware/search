#if !defined(__MON_CONF_H__)
#define __MON_CONF_H__

#include "menu.h"

#define MON_DEF_CONF_PATH "../conf/monitor.xml"

typedef struct
{
    char ip[IP_ADDR_MAX_LEN];   /* IP地址 */
    int port;                   /* 端口号 */
} ip_port_t;

typedef struct
{
    menu_conf_t menu;           /* 菜单配置 */
    ip_port_t  crwl;            /* 爬虫配置 */
    ip_port_t  filter;          /* 过滤配置 */
    ip_port_t  search;          /* 搜索引擎配置 */
} mon_conf_t;

mon_conf_t *mon_conf_load(const char *path);

#endif /*__MON_CONF_H__*/

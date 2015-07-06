#if !defined(__CONF_H__)
#define __CONF_H__

#include "comm.h"
#include "list.h"

/* 配置映射 */
typedef struct
{
    char name[NODE_MAX_LEN];            /* 结点名 */
    char path[FILE_PATH_MAX_LEN];       /* 结点对应的配置路径 */
} conf_map_t;

/* 系统配置 */
typedef struct
{
    list_t lsnds;                       /* 侦听配置 */
    list_t frwds;                       /* 转发配置 */
} sys_conf_t;

#endif /*__CONF_H__*/

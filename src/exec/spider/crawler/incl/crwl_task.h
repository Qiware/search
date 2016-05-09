#if !defined(__CRWL_TASK_H__)
#define __CRWL_TASK_H__

#include "comm.h"
#include "queue.h"

/* 任务类型 */
typedef enum
{
    CRWL_TASK_TYPE_UNKNOWN                  /* 未知任务 */
    , CRWL_TASK_DOWN_WEBPAGE                /* 下载网页 */

    , CRWL_TASK_TYPE_TOTAL                  /* 任务总数 */
} crwl_task_type_e;

/* 爬虫任务 */
typedef struct
{
    crwl_task_type_e type;                  /* 任务类型 */
    unsigned int length;                    /* 数据长度(头+体) */
} crwl_task_t;

/* 通过URL加载网页 */
typedef struct
{
    char uri[URL_MAX_LEN];                  /* URI */
    unsigned int depth;                     /* 当前URI的深度(0:表示深度未知) */
    int port;                               /* 端口号 */

    /* 查询到的信息 */
    char ip[IP_ADDR_MAX_LEN];               /* IP地址 */
    int family;                             /* 协议类型(IPv4 or IPv6) */
} crwl_task_down_webpage_t;

/* TASK空间 */
typedef union
{
    crwl_task_down_webpage_t uri;
} crwl_task_space_u;

#endif /*__CRWL_TASK_H__*/

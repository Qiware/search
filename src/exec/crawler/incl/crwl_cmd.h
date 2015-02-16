#if !defined(__CRWL_CMD_H__)
#define __CRWL_CMD_H__

#include "common.h"

/* 命令类型 */
typedef enum
{
    CRWL_CMD_UNKNOWN                /* 未知指令 */

    , CRWL_CMD_ADD_SEED_REQ         /* 请求添加种子 */
    , CRWL_CMD_ADD_SEED_RESP        /* 反馈添加种子 */

    , CRWL_CMD_QUERY_CONF_REQ       /* 查询配置信息 */
    , CRWL_CMD_QUERY_CONF_RESP      /* 反馈配置信息 */

    , CRWL_CMD_QUERY_WORKER_REQ     /* 查询爬取信息 */
    , CRWL_CMD_QUERY_WORKER_RESP    /* 反馈爬取信息 */

    , CRWL_CMD_QUERY_QUEUE_REQ      /* 查询队列信息 */
    , CRWL_CMD_QUERY_QUEUE_RESP     /* 反馈队列信息 */
        
    , CRWL_CMD_TOTAL
} crwl_cmd_e;

/* 添加种子信息 */
typedef struct
{
    char url[URL_MAX_LEN];          /* URL */
} crwl_cmd_add_seed_t;

/* 查询爬虫信息 */
typedef struct
{
} crwl_cmd_worker_resp_t;

/* 反馈配置信息 */
typedef struct
{
    int sched_num;
    int worker_num;
} crwl_cmd_conf_resp_t;

/* 各命令数据 */
typedef union
{
    crwl_cmd_add_seed_t add_seed;
    crwl_cmd_worker_resp_t worker_resp;
    crwl_cmd_conf_resp_t conf_resp;
} crwl_cmd_data_t;

/* 命令信息结构体 */
typedef struct
{
    uint16_t type;              /* 命令类型(范围:crwl_cmd_e) */
    uint16_t length;            /* 数据长度 */
} crwl_cmd_t;

#endif /*__CRWL_CMD_H__*/

#if !defined(__FLT_CMD_H__)
#define __FLT_CMD_H__

#include "comm.h"

/* 命令类型 */
typedef enum
{
    FLT_CMD_UNKNOWN                         /* 未知指令 */

    , FLT_CMD_ADD_SEED_REQ                  /* 请求添加种子 */
    , FLT_CMD_ADD_SEED_RSP                  /* 反馈添加种子 */

    , FLT_CMD_QUERY_CONF_REQ                /* 查询配置信息 */
    , FLT_CMD_QUERY_CONF_RSP                /* 反馈配置信息 */

    , FLT_CMD_QUERY_TABLE_STAT_REQ          /* 查询各表信息 */
    , FLT_CMD_QUERY_TABLE_STAT_RSP          /* 反馈各表信息 */

    , FLT_CMD_QUERY_WORKQ_STAT_REQ          /* 查询工作队列信息 */
    , FLT_CMD_QUERY_WORKQ_STAT_RSP          /* 反馈工作队列信息 */

    , FLT_CMD_STORE_DOMAIN_IP_MAP_REQ       /* 存储域名IP映射信息 */
    , FLT_CMD_STORE_DOMAIN_IP_MAP_RSP       /* 反馈存储域名IP映射信息 */

    , FLT_CMD_STORE_DOMAIN_BLACKLIST_REQ    /* 存储域名黑名单信息 */
    , FLT_CMD_STORE_DOMAIN_BLACKLIST_RSP    /* 反馈存储域名黑名单信息 */

    , FLT_CMD_TOTAL
} flt_cmd_e;

/* 添加种子信息 */
typedef struct
{
    char url[256];                          /* URL */
} flt_cmd_add_seed_req_t;

typedef struct
{
#define FLT_CMD_ADD_SEED_STAT_UNKNOWN  (1)  /* 未知 */
#define FLT_CMD_ADD_SEED_STAT_SUCC     (2)  /* 成功 */
#define FLT_CMD_ADD_SEED_STAT_FAIL     (3)  /* 失败 */
#define FLT_CMD_ADD_SEED_STAT_EXIST    (4)  /* 已存在 */
    int stat;                               /* 状态 */
    char url[256];                          /* URL */
} flt_cmd_add_seed_rsp_t;

/* 查询TABLE信息 */
typedef struct
{
} flt_cmd_table_stat_req_t;

#define FLT_CMD_TAB_MAX_NUM        (20)
typedef struct
{
    int num;                                /* Table数组有效长度 */
    struct
    {
    #define FLT_TAB_NAME_LEN (32)
        char name[FLT_TAB_NAME_LEN];
        int num;
        int max;
    } table[FLT_CMD_TAB_MAX_NUM];
} flt_cmd_table_stat_t;

/* 查询QUEUE信息 */
typedef struct
{
} flt_cmd_workq_stat_req_t;

#define FLT_CMD_QUEUE_MAX_NUM        (20)
typedef struct
{
    int num;                                /* Queue数组有效长度 */
    struct
    {
    #define FLT_QUEUE_NAME_LEN (32)
        char name[FLT_QUEUE_NAME_LEN];
        int num;
        int max;
    } queue[FLT_CMD_QUEUE_MAX_NUM];
} flt_cmd_workq_stat_t;

/* 反馈配置信息 */
typedef struct
{
    int log_level;                          /* 日志级别 */
} flt_cmd_conf_t;

/* 存储DOMAIN IP映射表 */
typedef struct
{
    char path[FILE_PATH_MAX_LEN];           /* 存储路径 */
} flt_cmd_store_domain_ip_map_rsp_t;

/* 存储DOMAIN黑名单 */
typedef struct
{
    char path[FILE_PATH_MAX_LEN];           /* 存储路径 */
} flt_cmd_store_domain_blacklist_rsp_t;

/* 各命令数据 */
typedef union
{
    flt_cmd_add_seed_req_t add_seed_req;
    flt_cmd_add_seed_rsp_t add_seed_rep;
    flt_cmd_conf_t conf;
    flt_cmd_workq_stat_t workq_stat;
    flt_cmd_table_stat_t table_stat;
    flt_cmd_store_domain_ip_map_rsp_t domain_ip_map_rep;
    flt_cmd_store_domain_blacklist_rsp_t domain_blacklist_rep;
} flt_cmd_data_t;

/* 命令信息结构体 */
typedef struct
{
    uint32_t type;                          /* 命令类型(范围:flt_cmd_e) */
    flt_cmd_data_t data;                    /* 命令内容 */
} flt_cmd_t;

#endif /*__FLT_CMD_H__*/

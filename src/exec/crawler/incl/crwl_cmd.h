#if !defined(__CRWL_CMD_H__)
#define __CRWL_CMD_H__

#include "common.h"

/* 命令类型 */
typedef enum
{
    CRWL_CMD_UNKNOWN                    /* 未知指令 */

    , CRWL_CMD_QUERY_CONF_REQ           /* 查询配置信息 */
    , CRWL_CMD_QUERY_CONF_RESP          /* 反馈配置信息 */

    , CRWL_CMD_QUERY_WORKER_STAT_REQ    /* 查询爬取信息 */
    , CRWL_CMD_QUERY_WORKER_STAT_RESP   /* 反馈爬取信息 */

    , CRWL_CMD_QUERY_WORKQ_STAT_REQ     /* 查询工作队列信息 */
    , CRWL_CMD_QUERY_WORKQ_STAT_RESP    /* 反馈工作队列信息 */

    , CRWL_CMD_SWITCH_SCHED_REQ         /* 切换调度 */
    , CRWL_CMD_SWITCH_SCHED_RESP        /* 反馈切换调度信息 */

    , CRWL_CMD_TOTAL
} crwl_cmd_e;

/* 查询爬虫信息 */
typedef struct
{
} crwl_cmd_worker_stat_req_t;

#define CRWL_CMD_WORKER_MAX_NUM     (20)
typedef struct
{
    time_t stm;                     /* 开始时间 */
    time_t ctm;                     /* 当前时间 */
    int num;                        /* WORKER数 */
    struct
    {
        uint32_t connections;       /* 连接数 */
        uint64_t down_webpage_total;/* 下载网页的计数 */
        uint64_t err_webpage_total; /* 异常网页的计数 */
    } worker[CRWL_CMD_WORKER_MAX_NUM];
} crwl_cmd_worker_stat_t;

/* 查询QUEUE信息 */
typedef struct
{
} crwl_cmd_workq_stat_req_t;

#define CRWL_CMD_QUEUE_MAX_NUM        (20)
typedef struct
{
    int num;                        /* Number */
    struct
    {
    #define CRWL_QUEUE_NAME_LEN (32)        
        char name[CRWL_QUEUE_NAME_LEN];
        int num;
        int max;
    } queue[CRWL_CMD_QUEUE_MAX_NUM];
} crwl_cmd_workq_stat_t;

/* 反馈配置信息 */
typedef struct
{
    struct
    {
        int level;                          /* 日志级别 */
        int syslevel;                       /* 系统日志级别 */
    } log;                                  /* 日志配置 */

    struct
    {
        uint32_t depth;                     /* 最大爬取深度 */
        char path[FILE_PATH_MAX_LEN];       /* 网页存储路径 */
    } download;                             /* 下载配置 */

    int workq_count;                        /* 工作队列容量 */

    struct
    {
        int num;                            /* 爬虫线程数 */
        int conn_max_num;                   /* 并发网页连接数 */
        int conn_tmout_sec;                 /* 连接超时时间 */
    } worker;
} crwl_cmd_conf_t;

/* 切换调度 */
typedef struct
{
} crwl_cmd_switch_sched_t;

typedef struct
{
    bool sched_stat;                        /* 调度状态(true:运行 false:暂停) */
} crwl_cmd_sched_stat_t;

/* 各命令数据 */
typedef union
{
    crwl_cmd_conf_t conf;
    crwl_cmd_workq_stat_t workq_stat;
    crwl_cmd_worker_stat_t worker_stat;
    crwl_cmd_switch_sched_t switch_sched;
    crwl_cmd_sched_stat_t sched_stat;
} crwl_cmd_data_t;

/* 命令信息结构体 */
typedef struct
{
    uint32_t type;              /* 命令类型(范围:crwl_cmd_e) */
    crwl_cmd_data_t data;       /* 命令内容 */
} crwl_cmd_t;

#endif /*__CRWL_CMD_H__*/

#if !defined(__CRWL_CONF_H__)
#define __CRWL_CONF_H__

#include "redis.h"
#include "common.h"
#include "xml_tree.h"

/* Worker配置信息 */
typedef struct
{
    int num;                                /* 爬虫线程数 */
    int conn_max_num;                       /* 并发网页连接数 */
    int conn_tmout_sec;                     /* 连接超时时间 */
    int taskq_count;                        /* Undo任务队列容量 */
} crwl_worker_conf_t;

/* Parser配置信息 */
typedef struct
{
    struct
    {
        char path[FILE_PATH_MAX_LEN];       /* 数据存储路径 */
        char err_path[FILE_PATH_MAX_LEN];   /* 错误数据存储路径 */
    } store;
} crwl_filter_conf_t;

/* Seed配置信息 */
typedef struct
{
    char uri[URI_MAX_LEN];                  /* 网页URI */
    uint32_t depth;                         /* 网页深度 */
} crwl_seed_conf_t;

/* Redis配置信息 */
typedef struct
{
    redis_conf_t master;                    /* Master配置 */
    char undo_taskq[QUEUE_NAME_MAX_LEN];    /* Undo任务队列名 */
    char done_tab[TABLE_NAME_MAX_LEN];      /* Done哈希表名 */
    char push_tab[TABLE_NAME_MAX_LEN];      /* Push哈希表名 */
    list_t slave_list;                      /* 副本配置信息 */
} crwl_redis_conf_t;

/* 爬虫配置信息 */
typedef struct
{
    struct
    {
        int level;                          /* 日志级别 */
        int level2;                         /* 系统日志级别 */
    } log;                                  /* 日志配置 */
    struct
    {
        uint32_t depth;                     /* 爬取最大深度 */
        char path[FILE_PATH_MAX_LEN];       /* 网页存储路径 */
    } download;                             /* 下载配置 */
    crwl_redis_conf_t redis;                /* REDIS配置信息 */
    crwl_worker_conf_t worker;              /* Worker配置信息 */
    crwl_filter_conf_t filter;              /* Filter配置信息 */
    list_t seed;                            /* 种子信息 */

    mem_pool_t *mem_pool;                   /* 内存池 */
} crwl_conf_t;

crwl_conf_t *crwl_conf_load(const char *path, log_cycle_t *log);
#define crwl_conf_destroy(conf)             /* 销毁配置对象 */\
{ \
    mem_pool_destroy(conf->mem_pool); \
    conf = NULL; \
}

#endif /*__CRWL_CONF_H__*/

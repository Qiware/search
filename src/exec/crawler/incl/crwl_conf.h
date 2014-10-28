#if !defined(__CRWL_CONF_H__)
#define __CRWL_CONF_H__

#include "common.h"
#include "xml_tree.h"

/* Worker配置信息 */
typedef struct
{
    int num;                                /* 爬虫线程数 */
    int connections;                        /* 并发网页连接数 */
    int taskq_count;                        /* Undo任务队列容量 */
} crwl_worker_conf_t;

/* Download配置信息 */
typedef struct
{
    int deep;                               /* 爬取最大深度 */
    char path[PATH_NAME_MAX_LEN];           /* 网页存储路径 */
} crwl_download_conf_t;

/* Seed配置信息 */
typedef struct
{
    char uri[URI_MAX_LEN];                  /* 网页URI */
    int deep;                               /* 网页深度 */
} crwl_seed_item_t;

/* Redis配置信息 */
typedef struct
{
    char ipaddr[IP_ADDR_MAX_LEN];           /* Redis服务IP */
    int port;                               /* Redis服务端口 */
    char undo_taskq[QUEUE_NAME_MAX_LEN];    /* Undo任务队列名 */
    char done_tab[TABLE_NAME_MAX_LEN];      /* Done哈希表名 */
    char push_tab[TABLE_NAME_MAX_LEN];      /* Push哈希表名 */
} crwl_redis_conf_t;

/* 爬虫配置信息 */
typedef struct
{
    int log_level;                          /* 日志级别 */
    int log2_level;                         /* 系统日志级别 */

    crwl_download_conf_t download;          /* Download配置信息 */
    crwl_redis_conf_t redis;                /* REDIS配置信息 */
    crwl_worker_conf_t worker;              /* Worker配置信息 */
    list_t seed;                            /* 种子信息 */
} crwl_conf_t;

int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log);

#endif /*__CRWL_CONF_H__*/

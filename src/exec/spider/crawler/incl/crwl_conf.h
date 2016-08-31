/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: crwl_conf.h
 ** 版本号: 1.0
 ** 描  述: 爬虫配置
 **         定义爬虫配置相关的结构体
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__CRWL_CONF_H__)
#define __CRWL_CONF_H__

#include "uri.h"
#include "comm.h"
#include "redis.h"
#include "xml_tree.h"
#include "mem_pool.h"

/* Worker配置信息 */
typedef struct
{
    int num;                                /* 爬虫线程数 */
    int conn_max_num;                       /* 并发网页连接数 */
    int conn_tmout_sec;                     /* 连接超时时间 */
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
    unsigned int depth;                     /* 网页深度 */
} crwl_seed_conf_t;

/* Redis配置信息 */
typedef struct
{
    redis_conf_t conf;                      /* Redis配置 */

    char taskq[QUEUE_NAME_MAX_LEN];         /* 任务队列名 */
    char done_tab[TABLE_NAME_MAX_LEN];      /* DONE哈希表名 */
    char push_tab[TABLE_NAME_MAX_LEN];      /* PUSHED哈希表名 */
} crwl_redis_conf_t;

/* 爬虫配置信息 */
typedef struct
{
    struct
    {
        unsigned int depth;                 /* 最大爬取深度 */
        char path[FILE_PATH_MAX_LEN];       /* 网页存储路径 */
    } download;                             /* 下载配置 */

    int workq_count;                        /* 工作队列容量 */
    int man_port;                           /* 管理服务侦听端口 */

    crwl_redis_conf_t redis;                /* REDIS配置 */
    crwl_worker_conf_t worker;              /* WORKER配置 */

    bool sched_stat;                        /* 调度状态(false:暂停 true:运行) */
} crwl_conf_t;

int crwl_load_conf(const char *path, crwl_conf_t *conf, log_cycle_t *log);

#endif /*__CRWL_CONF_H__*/

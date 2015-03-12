/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: flt_conf.h
 ** 版本号: 1.0
 ** 描  述: 爬虫配置
 **         定义爬虫配置相关的结构体
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__FLT_CONF_H__)
#define __FLT_CONF_H__

#include "redis.h"
#include "common.h"
#include "xml_tree.h"
#include "mem_pool.h"

/* Worker配置信息 */
typedef struct
{
    int num;                                /* 爬虫线程数 */
} flt_worker_conf_t;

/* Parser配置信息 */
typedef struct
{
    struct
    {
        char path[FILE_PATH_MAX_LEN];       /* 数据存储路径 */
        char err_path[FILE_PATH_MAX_LEN];   /* 错误数据存储路径 */
    } store;
} flt_filter_conf_t;

/* Seed配置信息 */
typedef struct
{
    char uri[URI_MAX_LEN];                  /* 网页URI */
    uint32_t depth;                         /* 网页深度 */
} flt_seed_conf_t;

/* Redis配置信息 */
typedef struct
{
    redis_conf_t master;                    /* Master配置 */
    char undo_taskq[QUEUE_NAME_MAX_LEN];    /* Undo任务队列名 */
    char done_tab[TABLE_NAME_MAX_LEN];      /* Done哈希表名 */
    char push_tab[TABLE_NAME_MAX_LEN];      /* Push哈希表名 */
#define FLT_REDIS_SLAVE_MAX_NUM  (10)      /* 最大副本数 */
    int slave_num;                          /* 副本数目 */
    redis_conf_t slaves[FLT_REDIS_SLAVE_MAX_NUM];  /* 副本配置信息 */
} flt_redis_conf_t;

/* 爬虫配置信息 */
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
    int man_port;                           /* 管理服务侦听端口 */

    flt_redis_conf_t redis;                /* REDIS配置 */
    flt_worker_conf_t worker;              /* WORKER配置 */
    flt_filter_conf_t filter;              /* FILTER配置 */

#define FLT_SEED_MAX_NUM   (100)           /* 种子最大数 */
    uint32_t seed_num;                      /* 种子实数 */
    flt_seed_conf_t seed[FLT_SEED_MAX_NUM];  /* 种子配置 */
} flt_conf_t;

flt_conf_t *flt_conf_load(const char *path, log_cycle_t *log);
#define flt_conf_destroy(conf)             /* 销毁配置对象 */\
{ \
    free(conf); \
}

#endif /*__FLT_CONF_H__*/

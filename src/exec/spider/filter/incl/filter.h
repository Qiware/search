/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: filter.h
 ** 版本号: 1.0
 ** 描  述: 网页过滤器
 **         负责网页的分析, 过滤
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__FILTER_H__)
#define __FILTER_H__

#include "log.h"
#include "queue.h"
#include "hash_tab.h"
#include "gumbo_ex.h"
#include "flt_priv.h"
#include "flt_conf.h"
#include "sig_queue.h"
#include "flt_worker.h"
#include "thread_pool.h"
#include <hiredis/hiredis.h>

#define FLT_REDIS_UNDO_LIMIT_NUM    (20000)
#define FLT_TASKQ_LEN               (1024)  /* 工作队列 */
#define FLT_CRWLQ_LEN               (1024)  /* 爬取队列 */


/* 任务对象 */
typedef struct
{
    char fname[FILE_PATH_MAX_LEN];          /* 文件名 */
    char fpath[FILE_PATH_MAX_LEN];          /* 文件路径 */
} flt_task_t;

/* 爬取对象 */
typedef struct
{
    char task_str[FLT_TASK_STR_LEN];        /* 任务字串 */
} flt_crwl_t;

/* 全局对象 */
typedef struct
{
    log_cycle_t *log;                       /* 日志对象 */
    flt_conf_t *conf;                       /* 配置信息 */

    time_t run_tm;                          /* 运行时间 */

    pthread_t sched_tid;                    /* Sched线程ID */
    thread_pool_t *workers;                 /* Worker线程池 */
    flt_worker_t *worker;                   /* 工作对象 */

    sig_queue_t *taskq;                     /* 处理队列(存放的是将要被解析的网页索引文件) */
    sig_queue_t *crwlq;                     /* 爬取队列(存放的是将被推送至REDIS's TASKQ的URL) */
    redisContext *redis;                    /* Redis对象 */

    hash_tab_t *domain_ip_map;              /* 域名IP映射表: 通过域名找到IP地址 */
    hash_tab_t *domain_blacklist;           /* 域名黑名单 */
} flt_cntx_t;

flt_cntx_t *flt_init(char *pname, flt_opt_t *opt);
int flt_launch(flt_cntx_t *ctx);
void flt_destroy(flt_cntx_t *ctx);

void *flt_push_routine(void *_ctx);

int flt_get_domain_ip_map(flt_cntx_t *ctx, const char *host, ipaddr_t *ip);

int flt_worker_init(flt_cntx_t *ctx, flt_worker_t *worker, int idx);
int flt_worker_destroy(flt_cntx_t *ctx, flt_worker_t *worker);

int flt_push_url_to_crwlq(flt_cntx_t *ctx, const char *url, const char *host, int port, int depth);
int flt_push_seed_to_crwlq(flt_cntx_t *ctx);

#endif /*__FILTER_H__*/

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.h
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__CRAWLER_H__)
#define __CRAWLER_H__

#include "log.h"
#include "list2.h"
#include "common.h"
#include "thread_pool.h"

#define CRWL_WORKER_TV_SEC          (02)    /* 超时(秒) */
#define CRWL_WORKER_TV_USEC         (00)    /* 超时(微妙) */
#define CRWL_WORKER_DEF_THD_NUM     (1)     /* 爬虫默认线程数 */
#define CRWL_WORKER_SLAB_SIZE       (16*KB) /* 爬虫SLAB内存池大小 */

typedef enum
{
    CRWL_OK = 0
    , CRWL_SHOW_HELP                        /* 显示帮助信息 */
    , CRWL_ERR = ~0x7fffffff                /* 失败、错误 */
} crwl_err_code_e;

/* 爬虫配置信息 */
typedef struct
{
    int thread_num;                         /* 爬虫线程数 */
    char svrip[IP_ADDR_MAX_LEN];            /* 任务分发服务IP */
    int port;                               /* 任务分发服务端口 */
    char log_level_str[LOG_LEVEL_MAX_LEN];  /* 日志级别 */
} crwl_conf_t;

typedef struct
{
    int fd;                                 /* 文件描述符 */
    char url[FILE_NAME_MAX_LEN];            /* URL */
} crwl_sck_t;

/* 爬虫对象信息 */
typedef struct
{
    fd_set wrset;                           /* 可写集合 */
    fd_set rdset;                           /* 可读集合 */

    eslab_pool_t slab;                      /* 内存池 */
    log_cycle_t *log;                       /* 日志对象 */

    list2_t sck_lst;                        /* 套接字列表 */
} crwl_worker_t;

/* 爬虫上下文 */
typedef struct
{
    crwl_conf_t conf;                       /* 配置信息 */
    thread_pool_t *tpool;                   /* 线程池对象 */
    log_cycle_t *log;                       /* 日志对象 */
} crwl_ctx_t;

int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log);
crwl_ctx_t *crwl_start_work(crwl_conf_t *conf, log_cycle_t *log);

#endif /*__CRAWLER_H__*/

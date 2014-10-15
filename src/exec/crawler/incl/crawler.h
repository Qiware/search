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

#include <stdint.h>

#include "log.h"
#include "slab.h"
#include "list.h"
#include "queue.h"
#include "common.h"
#include "crawler.h"
#include "crwl_task.h"
#include "thread_pool.h"

/* 错误码 */
typedef enum
{
    CRWL_OK = 0
    , CRWL_SHOW_HELP                        /* 显示帮助信息 */

    , CRWL_ERR = ~0x7fffffff                /* 失败、错误 */
} crwl_err_code_e;

/* 数据类型 */
typedef enum
{
    CRWL_DATA_TYPE_UNKNOWN = 0              /* 未知数据 */
    , CRWL_HTTP_GET_REQ                     /* HTTP GET请求 */
} crwl_data_type_e;

/* 读取/发送快照 */
typedef struct
{
    int off;                                /* 偏移量 */
    int total;                              /* 总字节 */

    char *addr;                             /* 缓存首地址 */
} snap_shot_t;

/* 发送数据的信息 */
typedef struct
{
    int type;                               /* 数据类型(crwl_data_type_e) */
    int length;                             /* 数据长度(报头+报体) */
} crwl_data_info_t;

/* Worker配置信息 */
typedef struct
{
    int thread_num;                         /* 爬虫线程数 */
    char svrip[IP_ADDR_MAX_LEN];            /* 任务分发服务IP */
    int port;                               /* 任务分发服务端口 */
    int load_web_page_num;                  /* 同时加载网页的数目 */
    queue_conf_t task_queue;                /* 任务队列配置 */
} crwl_worker_conf_t;

/* 爬虫配置信息 */
typedef struct
{
    crwl_worker_conf_t worker;              /* Worker配置信息 */
} crwl_conf_t;

/* 爬虫全局信息 */
typedef struct
{
    crwl_conf_t conf;                       /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */

    thread_pool_t *workers;                 /* 线程池对象 */

    pthread_rwlock_t slab_lock;             /* 内存池锁 */
    eslab_pool_t slab;                      /* 内存池 */
} crwl_cntx_t;

crwl_cntx_t *crwl_cntx_init(const crwl_conf_t *conf, log_cycle_t *log);
int crwl_cntx_startup(crwl_cntx_t *ctx);

int crwl_slab_init(crwl_cntx_t *ctx);
void *crwl_slab_alloc(crwl_cntx_t *ctx, int size);
void crwl_slab_free(crwl_cntx_t *ctx, void *p);
void crwl_slab_destroy(crwl_cntx_t *ctx);

#endif /*__CRAWLER_H__*/

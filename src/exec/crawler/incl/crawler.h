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

/* 宏定义 */
#define CRWL_TMOUT_SEC              (02)        /* 超时(秒) */
#define CRWL_TMOUT_USEC             (00)        /* 超时(微妙) */
#define CRWL_DEF_THD_NUM            (01)        /* 默认线程数 */
#define CRWL_SLAB_SIZE              (1 * MB)    /* SLAB内存池大小 */
#define CRWL_RECV_SIZE              (128 * KB)  /* 缓存SIZE(接收缓存) */
#define CRWL_SYNC_SIZE              (64 * KB)   /* 同步SIZE */
#define CRWL_DOWN_WEBPAGE_NUM       (1)         /* 默认同时下载的网页数 */
#define CRWL_CONNECT_TMOUT_SEC      (00)        /* 连接超时时间 */
#define CRWL_WEB_SVR_PORT           (80)        /* WEB服务器侦听端口 */
#define CRWL_SCK_TMOUT_SEC          (05)        /* 套接字超时时间(秒) */

#define CRWL_TASK_QUEUE_MAX_NUM     (10000)     /* 任务队列单元数 */
#define CRWL_DEF_CONF_PATH  "../conf/crawler.xml"   /* 默认配置路径 */



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
    int down_webpage_num;                   /* 同时加载网页的数目 */
    queue_conf_t task_queue;                /* 任务队列配置 */
} crwl_worker_conf_t;

/* Redis配置信息 */
typedef struct
{
    char ipaddr[IP_ADDR_MAX_LEN];           /* Redis服务IP */
    int port;                               /* Redis服务端口 */
    char undo_taskq[QUEUE_NAME_MAX_LEN];    /* Undo任务队列名 */
} crwl_redis_conf_t;

/* 爬虫配置信息 */
typedef struct
{
    int log_level;                          /* 日志级别 */
    int log2_level;                         /* 系统日志级别 */

    crwl_redis_conf_t redis;                /* REDIS配置信息 */
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
void crwl_slab_dealloc(crwl_cntx_t *ctx, void *p);
void crwl_slab_destroy(crwl_cntx_t *ctx);

#endif /*__CRAWLER_H__*/

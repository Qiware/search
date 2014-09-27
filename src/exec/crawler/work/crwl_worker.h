#if !defined(__CRWL_WORKER_H__)
#define __CRWL_WORKER_H__

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
#define CRWL_WRK_TV_SEC             (02)        /* 超时(秒) */
#define CRWL_WRK_TV_USEC            (00)        /* 超时(微妙) */
#define CRWL_WRK_DEF_THD_NUM        (1)         /* 爬虫默认线程数 */
#define CRWL_WRK_SLAB_SIZE          (16 * KB)   /* 爬虫SLAB内存池大小 */
#define CRWL_WRK_BUFF_SIZE          (16 * KB)   /* 接收SIZE */
#define CRWL_WRK_READ_SIZE          (12 * KB)   /* 读取SIZE */
#define CRWL_WRK_SYNC_SIZE          (12 * KB)   /* 同步SIZE */
#define CRWL_WRK_LOAD_WEB_PAGE_NUM  (1)         /* 默认同时下载的网页数 */
#define CRWL_WRK_CONNECT_TMOUT      (30)        /* 连接超时时间 */
#define CRWL_WRK_WEB_SVR_PORT       (80)        /* WEB服务器侦听端口 */
#define CRWL_WRK_TMOUT_SEC          (30)        /* 超时时间(秒) */

#define CRWL_TASK_QUEUE_MAX_NUM     (10000)     /* 任务队列单元数 */
#define CRWL_TASK_QUEUE_MAX_SIZE    (4 * KB)    /* 任务队列单元SIZE */

/* 爬虫配置信息 */
typedef struct
{
    int thread_num;                         /* 爬虫线程数 */
    char svrip[IP_ADDR_MAX_LEN];            /* 任务分发服务IP */
    int port;                               /* 任务分发服务端口 */
    int load_web_page_num;                  /* 同时加载网页的数目 */
    queue_conf_t task_queue;                /* 任务队列配置 */
} crwl_worker_conf_t;

/* 网页加载套接字信息 */
typedef struct
{
    int sckid;                              /* 套接字ID */
    time_t wrtm;                            /* 最近写入时间 */
    time_t rdtm;                            /* 最近读取时间 */

    char url[URL_MAX_LEN];                  /* 原始URL(未转义) */
    char base64_url[URL_MAX_LEN];           /* 转义URL(中文转为BASE64编码) */

    char ipaddr[IP_ADDR_MAX_LEN];           /* IP地址 */
    int port;                               /* 端口号 */

    snap_shot_t read;                       /* 读取快照 */
    snap_shot_t send;                       /* 发送快照 */

    char recv_buff[CRWL_WRK_BUFF_SIZE];     /* 接收缓存 */
    list_t send_list;                       /* 发送链表 */
} crwl_worker_socket_t;

/* 爬虫对象信息 */
typedef struct
{
    fd_set wrset;                           /* 可写集合 */
    fd_set rdset;                           /* 可读集合 */

    eslab_pool_t slab;                      /* 内存池 */
    log_cycle_t *log;                       /* 日志对象 */

    list_t sock_list;                       /* 套接字列表
                                               结点数据指针指向crwl_worker_socket_t */
    crwl_task_queue_t task;                 /* 任务对象 */
} crwl_worker_t;

/* 爬虫全局信息 */
typedef struct
{
    crwl_worker_conf_t conf;                /* 配置信息 */
    thread_pool_t *tpool;                   /* 线程池对象 */
    log_cycle_t *log;                       /* 日志对象 */
} crwl_worker_ctx_t;

int crwl_worker_add_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);
int crwl_worker_task_load_webpage_by_url(
        crwl_worker_t *worker, const crwl_task_load_webpage_by_url_t *args);
int crwl_worker_task_load_webpage_by_ip(
        crwl_worker_t *worker, const crwl_task_load_webpage_by_ip_t *args);

int crwl_worker_load_conf(crwl_worker_conf_t *conf, const char *path, log_cycle_t *log);
crwl_worker_ctx_t *crwl_worker_init_cntx(crwl_worker_conf_t *conf, log_cycle_t *log);
int crwl_worker_startup(crwl_worker_ctx_t *ctx);

#endif /*__CRWL_WORKER_H__*/

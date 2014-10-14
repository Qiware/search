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
#define CRWL_WRK_BUFF_SIZE          (64 * KB)   /* 接收SIZE */
#define CRWL_WRK_READ_SIZE          (64 * KB)   /* 读取SIZE */
#define CRWL_WRK_SYNC_SIZE          (16 * KB)   /* 同步SIZE */
#define CRWL_WRK_LOAD_WEB_PAGE_NUM  (1)         /* 默认同时下载的网页数 */
#define CRWL_WRK_CONNECT_TMOUT      (00)        /* 连接超时时间 */
#define CRWL_WRK_WEB_SVR_PORT       (80)        /* WEB服务器侦听端口 */
#define CRWL_WRK_TMOUT_SEC          (05)        /* 超时时间(秒) */

#define CRWL_TASK_QUEUE_MAX_NUM     (10000)     /* 任务队列单元数 */

/* 网页加载套接字信息 */
typedef struct
{
    int sckid;                              /* 套接字ID */
    time_t wrtm;                            /* 最近写入时间 */
    time_t rdtm;                            /* 最近读取时间 */

    char uri[URL_MAX_LEN];                  /* 原始URL(未转义) */
    char base64_uri[URL_MAX_LEN];           /* 转义URL(中文转为BASE64编码) */

    char ipaddr[IP_ADDR_MAX_LEN];           /* IP地址 */
    int port;                               /* 端口号 */

    snap_shot_t read;                       /* 读取快照 */
    snap_shot_t send;                       /* 发送快照 */

    char recv_buff[CRWL_WRK_BUFF_SIZE+1];   /* 接收缓存 */
    list_t send_list;                       /* 发送链表 */
} crwl_worker_socket_t;

/* 爬虫对象信息 */
typedef struct
{
    int tidx;                               /* 线程索引 */
    crwl_cntx_t *ctx;                       /* 全局信息 */

    fd_set wrset;                           /* 可写集合 */
    fd_set rdset;                           /* 可读集合 */

    eslab_pool_t slab;                      /* 内存池 */
    log_cycle_t *log;                       /* 日志对象 */

    list_t sock_list;                       /* 套接字列表
                                               结点数据指针指向crwl_worker_socket_t */
    crwl_task_queue_t task;                 /* 任务对象(注意: 该队列结点和数据的空间来自CTX) */
} crwl_worker_t;

/* 函数声明 */
int crwl_worker_add_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);
int crwl_worker_remove_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);

int crwl_worker_add_http_get_req(
        crwl_worker_t *worker, crwl_worker_socket_t *sck, const char *uri);
int crwl_task_load_webpage_by_uri(
        crwl_worker_t *worker, const crwl_task_load_webpage_by_uri_t *args);
int crwl_task_load_webpage_by_ip(
        crwl_worker_t *worker, const crwl_task_load_webpage_by_ip_t *args);

int crwl_worker_load_conf(crwl_worker_conf_t *conf, const char *path, log_cycle_t *log);

int crwl_worker_init(crwl_cntx_t *ctx, crwl_worker_t *worker);
void *crwl_worker_routine(void *_ctx);

int crwl_init_workers(crwl_cntx_t *ctx);
int crwl_workers_destroy(crwl_cntx_t *ctx);

#endif /*__CRWL_WORKER_H__*/

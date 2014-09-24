#if !defined(__CRWL_WORKER_H__)
#define __CRWL_WORKER_H__

#include "log.h"
#include "slab.h"
#include "list.h"
#include "crawler.h"
#include "thread_pool.h"

/* 宏定义 */
#define CRWL_WRK_TV_SEC          (02)       /* 超时(秒) */
#define CRWL_WRK_TV_USEC         (00)       /* 超时(微妙) */
#define CRWL_WRK_DEF_THD_NUM     (1)        /* 爬虫默认线程数 */
#define CRWL_WRK_SLAB_SIZE       (16 * KB)  /* 爬虫SLAB内存池大小 */
#define CRWL_WRK_BUFF_SIZE       (16 * KB)  /* 接收SIZE */
#define CRWL_WRK_READ_SIZE       (12 * KB)  /* 读取SIZE */
#define CRWL_WRK_SYNC_SIZE       (12 * KB)  /* 同步SIZE */
#define CRWL_WRK_LOAD_WEB_PAGE_NUM (1)      /* 默认同时下载的网页数 */

/* 爬虫配置信息 */
typedef struct
{
    int thread_num;                         /* 爬虫线程数 */
    char svrip[IP_ADDR_MAX_LEN];            /* 任务分发服务IP */
    int port;                               /* 任务分发服务端口 */
    int load_web_page_num;                  /* 同时加载网页的数目 */
} crwl_worker_conf_t;

/* 爬虫对象信息 */
typedef struct
{
    int sckid;                              /* 套接字ID */

    char url[URL_MAX_LEN];                  /* 原始URL(未转义) */
    char base64_url[URL_MAX_LEN];           /* 转义URL(中文转为BASE64编码) */

    char ipaddr[IP_ADDR_MAX_LEN];           /* IP地址 */
    int port;                               /* 端口号 */

    snap_shot_t read;                       /* 读取快照 */
    snap_shot_t send;                       /* 发送快照 */

    char recv_buff[CRWL_WRK_BUFF_SIZE];     /* 接收缓存 */
    list_t send_list;                       /* 发送链表 */
} crwl_worker_sck_t;

/* 爬虫对象信息 */
typedef struct
{
    fd_set wrset;                           /* 可写集合 */
    fd_set rdset;                           /* 可读集合 */

    eslab_pool_t slab;                      /* 内存池 */
    log_cycle_t *log;                       /* 日志对象 */

    list_t sck_list;                        /* 套接字列表
                                               结点数据指针指向crwl_worker_sck_t */
} crwl_worker_t;

/* 爬虫上下文 */
typedef struct
{
    crwl_worker_conf_t conf;                /* 配置信息 */
    thread_pool_t *tpool;                   /* 线程池对象 */
    log_cycle_t *log;                       /* 日志对象 */
} crwl_worker_ctx_t;

int crwl_worker_load_conf(crwl_worker_conf_t *conf, const char *path, log_cycle_t *log);
crwl_worker_ctx_t *crwl_worker_startup(crwl_worker_conf_t *conf, log_cycle_t *log);

#endif /*__CRWL_WORKER_H__*/

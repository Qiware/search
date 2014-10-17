#if !defined(__CRWL_WORKER_H__)
#define __CRWL_WORKER_H__

#include <stdint.h>

#include "log.h"
#include "slab.h"
#include "list.h"
#include "queue.h"
#include "common.h"
#include "crawler.h"
#include "xml_tree.h"
#include "crwl_task.h"
#include "thread_pool.h"


/* 网页加载套接字信息 */
typedef struct
{
    int sckid;                      /* 套接字ID */
    time_t wrtm;                    /* 最近写入时间 */
    time_t rdtm;                    /* 最近读取时间 */

    char uri[URL_MAX_LEN];          /* 原始URL(未转义) */
    uint32_t deep;                  /* 网页深度 */

    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
    int port;                       /* 端口号 */

    snap_shot_t read;               /* 读取快照 */
    snap_shot_t send;               /* 发送快照 */

    char recv[CRWL_RECV_SIZE + 1];  /* 接收缓存 */
    list_t send_list;               /* 发送链表 */

    /* 网页存储信息 */
    FILE *fp;                       /* 文件指针 */
    uint64_t webpage_idx;           /* 网页编号 */
} crwl_worker_socket_t;

/* 爬虫对象信息 */
typedef struct
{
    int tidx;                       /* 线程索引 */
    crwl_cntx_t *ctx;               /* 全局信息 */

    fd_set wrset;                   /* 可写集合 */
    fd_set rdset;                   /* 可读集合 */

    eslab_pool_t slab;              /* 内存池 */
    log_cycle_t *log;               /* 日志对象 */

    list_t sock_list;               /* 套接字列表
                                       结点数据指针指向crwl_worker_socket_t */
    crwl_task_queue_t undo_taskq;   /* 任务对象(注意: 该队列结点和数据的空间来自CTX) */

    uint64_t down_webpage_total;    /* 下载网页的总数 */
} crwl_worker_t;

/* 获取队列剩余空间 */
#define crwl_worker_undo_taskq_space(worker) queue_space(&(worker)->undo_taskq.queue)

/* 函数声明 */
int crwl_worker_add_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);
int crwl_worker_remove_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);

int crwl_worker_add_http_get_req(
        crwl_worker_t *worker, crwl_worker_socket_t *sck, const char *uri);
int crwl_task_down_webpage_by_uri(
        crwl_worker_t *worker, const crwl_task_down_webpage_by_uri_t *args);
int crwl_task_down_webpage_by_ip(
        crwl_worker_t *worker, const crwl_task_down_webpage_by_ip_t *args);

int crwl_worker_webpage_fopen(crwl_worker_t *worker, crwl_worker_socket_t *sck);
int crwl_worker_webpage_fsync(crwl_worker_t *worker, crwl_worker_socket_t *sck);
int crwl_worker_webpage_fcheck(crwl_worker_t *worker, crwl_worker_socket_t *sck);

int crwl_worker_parse_conf(xml_tree_t *xml, crwl_worker_conf_t *conf, log_cycle_t *log);

int crwl_worker_init(crwl_cntx_t *ctx, crwl_worker_t *worker);
int crwl_worker_destroy(crwl_worker_t *worker);
void *crwl_worker_routine(void *_ctx);
#endif /*__CRWL_WORKER_H__*/

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
#if defined(__EVENT_EPOLL__)
#include <sys/epoll.h>    
#endif /*__EVENT_EPOLL__*/

/* 网页信息 */
typedef struct
{
    char uri[URL_MAX_LEN];          /* 原始URL */
    uint32_t depth;                 /* 网页深度 */

    char ip[IP_ADDR_MAX_LEN];       /* IP地址 */
    int port;                       /* 端口号 */

    /* 网页存储信息 */
    char fname[FILE_NAME_MAX_LEN];  /* 文件名 - 无后缀 */
    uint64_t idx;                   /* 网页编号 */
    FILE *fp;                       /* 文件指针 */
    size_t size;                    /* 网页总字节数 */
} crwl_webpage_t;

/* 网页加载套接字信息 */
typedef struct
{
    int sckid;                      /* 套接字ID */
    struct timeb crtm;              /* 创建时间 */
    time_t wrtm;                    /* 最近写入时间 */
    time_t rdtm;                    /* 最近读取时间 */

    crwl_webpage_t webpage;         /* 网页信息 */

    snap_shot_t read;               /* 读取快照 */
    snap_shot_t send;               /* 发送快照 */

    char recv[CRWL_RECV_SIZE + 1];  /* 接收缓存 */
    list_t send_list;               /* 发送链表 */
} crwl_worker_socket_t;

/* 爬虫对象信息 */
typedef struct
{
    int tidx;                       /* 线程索引 */
    crwl_cntx_t *ctx;               /* 全局配置 */

#if defined(__EVENT_EPOLL__)
    int ep_fd;                      /* epoll文件描述符 */
    int ep_fds;                     /* 处于激活状态的套接字数 */
    struct epoll_event *events;     /* Event最大数 */
#else /*!__EVENT_EPOLL__*/
    fd_set wrset;                   /* 可写集合 */
    fd_set rdset;                   /* 可读集合 */
#endif /*!__EVENT_EPOLL__*/

    eslab_pool_t slab;              /* 内存池 */
    log_cycle_t *log;               /* 日志对象 */

    list_t sock_list;               /* 套接字列表
                                       结点数据指针指向crwl_worker_socket_t */
    lqueue_t undo_taskq;            /* 任务队列(注意: 该队列结点和数据的空间来自CTX) */

    uint64_t down_webpage_total;    /* 下载网页的计数 */
} crwl_worker_t;

/* 获取队列剩余空间 */
#define crwl_worker_undo_taskq_space(worker) queue_space(&(worker)->undo_taskq.queue)

/* 函数声明 */
int crwl_worker_add_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);
crwl_worker_socket_t *crwl_worker_query_sock(crwl_worker_t *worker, int sckid);
int crwl_worker_remove_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);

int crwl_worker_add_http_get_req(
        crwl_worker_t *worker, crwl_worker_socket_t *sck, const char *uri);
int crwl_task_down_webpage_by_uri(
        crwl_worker_t *worker, const crwl_task_down_webpage_by_uri_t *args);
int crwl_task_down_webpage_by_ip(
        crwl_worker_t *worker, const crwl_task_down_webpage_by_ip_t *args);

int crwl_worker_webpage_creat(crwl_worker_t *worker, crwl_worker_socket_t *sck);
/* 将接收的数据同步到文件
 *  worker: 对应crwl_worker_t数据类型
 *  sck: 对应crwl_worker_socket_t数据类型
 * */
#define crwl_worker_webpage_fsync(worker, sck) \
{ \
    fwrite(sck->read.addr, sck->read.off, 1, sck->webpage.fp); \
 \
    sck->read.off = 0; \
    sck->read.total = CRWL_RECV_SIZE; \
}
int crwl_worker_webpage_finfo(crwl_worker_t *worker, crwl_worker_socket_t *sck);

int crwl_worker_init(crwl_cntx_t *ctx, crwl_worker_t *worker);
int crwl_worker_destroy(crwl_worker_t *worker);
void *crwl_worker_routine(void *_ctx);
#endif /*__CRWL_WORKER_H__*/

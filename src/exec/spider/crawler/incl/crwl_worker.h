#if !defined(__CRWL_WORKER_H__)
#define __CRWL_WORKER_H__

#include "log.h"
#include "comm.h"
#include "list.h"
#include "http.h"
#include "queue.h"
#include "crwl_priv.h"
#include "crwl_task.h"
#include "thread_pool.h"

/* 网页信息 */
typedef struct
{
    char uri[URL_MAX_LEN];          /* 原始URL */
    unsigned int depth;             /* 网页深度 */

    char ip[IP_ADDR_MAX_LEN];       /* IP地址 */
    int port;                       /* 端口号 */

    /* 网页存储信息 */
    char fname[FILE_NAME_MAX_LEN];  /* 文件名 - 无后缀 */
    unsigned long long idx;         /* 网页编号 */
    FILE *fp;                       /* 文件指针 */
    size_t size;                    /* 网页总字节数 */

    bool has_filter_http_header;    /* 是否已过滤HTTP头 */
} crwl_webpage_t;

/* 爬虫对象信息 */
typedef struct
{
    int id;                         /* 线程索引 */

    int epid;                       /* epoll文件描述符 */
    int fds;                        /* 处于激活状态的套接字数 */
    struct epoll_event *events;     /* EVENT数组 */

    slot_t *slot_for_sck_extra;     /* 内存池(固定大小)(for crwl_worker_socket_extra_t) */
    log_cycle_t *log;               /* 日志对象 */

    time_t scan_tm;                 /* 超时扫描时间 */
    list_t *sock_list;              /* 套接字列表
                                       结点数据指针指向socket_t(TODO: 可使用红黑树) */

    unsigned long long total;               /* 计数 */
    unsigned long long err_webpage_total;   /* 异常网页的计数 */
    unsigned long long down_webpage_total;  /* 已爬网页的计数 */
} crwl_worker_t;

/* 网页加载套接字信息 */
typedef struct
{
    crwl_webpage_t webpage;         /* 网页信息 */

    http_response_t response;       /* HTTP应答 */

    char recv[CRWL_RECV_SIZE + 1];  /* 接收缓存 */
    list_t *send_list;              /* 发送链表 */
} crwl_worker_socket_extra_t;

/* 函数声明 */
int crwl_worker_add_sock(crwl_worker_t *worker, socket_t *sck);
socket_t *crwl_worker_query_sock(crwl_worker_t *worker, int sckid);
int crwl_worker_remove_sock(crwl_worker_t *worker, socket_t *sck);

int crwl_worker_recv_data(crwl_cntx_t *ctx, crwl_worker_t *worker, socket_t *sck);
int crwl_worker_send_data(crwl_cntx_t *ctx, crwl_worker_t *worker, socket_t *sck);

int crwl_worker_add_http_get_req(
        crwl_worker_t *worker, socket_t *sck, const char *uri);

int crwl_worker_webpage_creat(crwl_cntx_t *ctx, crwl_worker_t *worker, socket_t *sck);
/* 将接收的数据同步到文件
 *  worker: 对应crwl_worker_t数据类型
 *  sck: 对应socket_t数据类型
 * */
#define crwl_worker_webpage_fsync(worker, sck) \
{ \
    crwl_worker_socket_extra_t *_ex = sck->extra; \
 \
    if (!_ex->webpage.has_filter_http_header) \
    { \
        fwrite(sck->recv.addr + _ex->response.header_len, \
                sck->recv.off - _ex->response.header_len, 1, _ex->webpage.fp); \
        _ex->webpage.has_filter_http_header = true; \
    } \
    else \
    { \
        fwrite(sck->recv.addr, sck->recv.off, 1, _ex->webpage.fp); \
    } \
 \
    sck->recv.off = 0; \
    sck->recv.total = CRWL_RECV_SIZE; \
}
int crwl_worker_webpage_finfo(crwl_cntx_t *ctx, crwl_worker_t *worker, socket_t *sck);

int crwl_worker_init(crwl_cntx_t *ctx, crwl_worker_t *worker, int id);
int crwl_worker_destroy(crwl_cntx_t *ctx, crwl_worker_t *worker);
void *crwl_worker_routine(void *_ctx);

crwl_worker_t *crwl_worker_get_by_idx(crwl_cntx_t *ctx, int idx);

#endif /*__CRWL_WORKER_H__*/

#if !defined(__SRCH_RECVER_H__)
#define __SRCH_RECVER_H__

#include "list.h"
#include "queue.h"
#include "search.h"
#include  "hash_tab.h"

typedef struct
{
    int tidx;                       /* 线程索引 */

    srch_cntx_t *ctx;               /* 全局对象 */
    srch_conf_t *conf;              /* 全局配置信息 */
    slab_pool_t *slab;              /* 内存池 */
    log_cycle_t *log;               /* 日志对象 */

    int ep_fd;                      /* epoll描述符 */
    int fds;                        /* 处于激活状态的套接字数 */
    struct epoll_event *events;     /* Event最大数 */

    int cmd_sck_id;                 /* 命令套接字 */
    hash_tab_t *sock_tab;           /* 套接字表(挂载数据socket_t) */
    lqueue_t *conn_sckq;            /* 套接字队列 */

    time_t scan_tm;                 /* 前一次超时扫描的时间 */
} srch_recver_t;

/* 套接字信息 */
typedef struct
{
    uint64_t sck_serial;            /* 序列号 */

    char recv[SRCH_RECV_SIZE + 1];  /* 接收缓存 */
    list_t send_list;               /* 发送链表 */
} srch_recver_socket_data_t;

void *srch_recver_routine(void *_ctx);

int srch_recver_init(srch_cntx_t *ctx, srch_recver_t *recver);
int srch_recver_destroy(srch_recver_t *recver);

#endif /*__SRCH_RECVER_H__*/

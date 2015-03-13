#if !defined(__SRCH_AGENT_H__)
#define __SRCH_AGENT_H__

#include "list.h"
#include "queue.h"
#include "search.h"
#include "rb_tree.h"
#include "srch_mesg.h"

#define SRCH_AGENT_TMOUT_MSEC       (500)   /* 超时(豪秒) */

#define SRCH_AGENT_EVENT_MAX_NUM    (4096)  /* 事件最大数 */
#define SRCH_AGENT_SCK_HASH_MOD     (7)     /* 套接字哈希长度 */

typedef struct
{
    int tidx;                       /* 线程索引 */

    slab_pool_t *slab;              /* 内存池 */
    log_cycle_t *log;               /* 日志对象 */

    int epid;                       /* epoll描述符 */
    int fds;                        /* 处于激活状态的套接字数 */
    struct epoll_event *events;     /* Event最大数 */

    int cmd_sck_id;                 /* 命令套接字 */
    rbt_tree_t *connections;        /* 套接字表(挂载数据socket_t) */

    time_t ctm;                     /* 当前时间 */
    time_t scan_tm;                 /* 前一次超时扫描的时间 */
} srch_agent_t;

/* 套接字信息 */
typedef struct
{
    uint64_t serial;                /* 序列号(主键) */

    srch_mesg_header_t *head;       /* 报头起始地址 */
    void *body;                     /* Body */
    list_t *send_list;              /* 发送链表 */
} srch_agent_socket_extra_t;

void *srch_agent_routine(void *_ctx);

int srch_agent_init(srch_cntx_t *ctx, srch_agent_t *agent, int idx);
int srch_agent_destroy(srch_agent_t *agent);

#endif /*__SRCH_AGENT_H__*/

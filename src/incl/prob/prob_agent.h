#if !defined(__PROB_AGENT_H__)
#define __PROB_AGENT_H__

#include "list.h"
#include "queue.h"
#include "probe.h"
#include "rb_tree.h"
#include "prob_mesg.h"

#define PROB_AGENT_TMOUT_MSEC       (500)   /* 超时(豪秒) */

#define PROB_AGENT_EVENT_MAX_NUM    (4096)  /* 事件最大数 */
#define PROB_AGENT_SCK_HASH_MOD     (7)     /* 套接字哈希长度 */

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
    unsigned int conn_total;        /* 当前连接数 */

    time_t ctm;                     /* 当前时间 */
    time_t scan_tm;                 /* 前一次超时扫描的时间 */
} prob_agent_t;

/* 套接字信息 */
typedef struct
{
    uint64_t serial;                /* 序列号(主键) */

    prob_flow_t *flow;              /* 流水信息 */
    prob_mesg_header_t *head;       /* 报头起始地址 */
    void *body;                     /* Body */
    list_t *send_list;              /* 发送链表 */
} prob_agent_socket_extra_t;

void *prob_agent_routine(void *_ctx);

int prob_agent_init(prob_cntx_t *ctx, prob_agent_t *agent, int idx);
int prob_agent_destroy(prob_agent_t *agent);

#endif /*__PROB_AGENT_H__*/

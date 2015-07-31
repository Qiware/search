#if !defined(__AGENT_RSVR_H__)
#define __AGENT_RSVR_H__

#include "list.h"
#include "agent.h"
#include "queue.h"
#include "rb_tree.h"
#include "agent_mesg.h"

#define AGENT_TMOUT_MSEC       (1000)  /* 超时(豪秒) */

#define AGENT_EVENT_MAX_NUM    (8192)  /* 事件最大数 */
#define AGENT_SCK_HASH_MOD     (7)     /* 套接字哈希长度 */

typedef struct
{
    int id;                         /* 对象ID */

    slab_pool_t *slab;              /* 内存池 */
    log_cycle_t *log;               /* 日志对象 */

    int epid;                       /* epoll描述符 */
    int fds;                        /* 处于激活状态的套接字数 */
    struct epoll_event *events;     /* Event最大数 */

    socket_t cmd_sck;               /* 命令套接字 */
    rbt_tree_t *connections;        /* 套接字表(挂载数据socket_t) */
    unsigned int conn_total;        /* 当前连接数 */

    time_t ctm;                     /* 当前时间 */
    time_t scan_tm;                 /* 前一次超时扫描的时间 */
    uint32_t recv_seq;              /* 业务接收序列号(此值将用于生成系统流水号) */
} agent_rsvr_t;

/* 套接字信息 */
typedef struct
{
    uint64_t seq;                   /* SCK序列号(主键) */
    bool is_cmd_sck;                /* 是否是命令套接字(false:否 true:是) */

    agent_flow_t *flow;             /* 流水信息 */
    agent_header_t *head;           /* 报头起始地址 */
    void *body;                     /* Body */
    list_t *send_list;              /* 发送链表 */
} agent_socket_extra_t;

void *agent_rsvr_routine(void *_ctx);

int agent_rsvr_init(agent_cntx_t *ctx, agent_rsvr_t *agent, int idx);
int agent_rsvr_destroy(agent_rsvr_t *agent);

#endif /*__AGENT_RSVR_H__*/

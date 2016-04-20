#if !defined(__RTRD_RECV_H__)
#define __RTRD_RECV_H__

#include "log.h"
#include "sck.h"
#include "list.h"
#include "comm.h"
#include "iovec.h"
#include "list2.h"
#include "queue.h"
#include "shm_opt.h"
#include "spinlock.h"
#include "avl_tree.h"
#include "rtmq_cmd.h"
#include "rtmq_comm.h"
#include "shm_queue.h"
#include "thread_pool.h"

/* 宏定义 */
#define RTMQ_CTX_POOL_SIZE          (5 * MB)/* 全局内存池空间 */

/* Recv线程的UNIX-UDP路径 */
#define rtrd_rsvr_usck_path(conf, _path, tidx) \
    snprintf(_path, sizeof(_path), "%s/%d_rsvr_%d.usck", (conf)->path, (conf)->nodeid, tidx+1)
/* Worker线程的UNIX-UDP路径 */
#define rtrd_worker_usck_path(conf, _path, tidx) \
    snprintf(_path, sizeof(_path), "%s/%d_wsvr_%d.usck", (conf)->path, (conf)->nodeid, tidx+1)
/* Listen线程的UNIX-UDP路径 */
#define rtrd_lsn_usck_path(conf, _path) \
    snprintf(_path, sizeof(_path), "%s/%d_lsn.usck", (conf)->path, (conf)->nodeid)
/* 发送队列的共享内存KEY路径 */
#define rtrd_shm_distq_path(conf, _path, idx) \
    snprintf(_path, sizeof(_path), "%s/%d_shm-%d.sq", (conf)->path, (conf)->nodeid, idx)
/* 分发线程的UNIX-UDP路径 */
#define rtrd_dsvr_usck_path(conf, _path) \
    snprintf(_path, sizeof(_path), "%s/%d_dsvr.usck", (conf)->path, (conf)->nodeid)
/* 客户端的通信路径 */
#define rtrd_cli_unix_path(conf, _path) \
    snprintf(_path, sizeof(_path), "%s/%d_cli.usck", (conf)->path, (conf)->nodeid)

/* 配置信息 */
typedef struct
{
    int nodeid;                         /* 节点ID(唯一值: 不允许重复) */
    char path[FILE_NAME_MAX_LEN];       /* 工作路径 */

    struct {
        char usr[RTMQ_USR_MAX_LEN];     /* 用户名 */
        char passwd[RTMQ_PWD_MAX_LEN];  /* 登录密码 */
    } auth;

    int port;                           /* 侦听端口 */
    int recv_thd_num;                   /* 接收线程数 */
    int work_thd_num;                   /* 工作线程数 */
    int recvq_num;                      /* 接收队列数 */
    int distq_num;                      /* 分发队列数 */

    queue_conf_t recvq;                 /* 接收队列配置 */
    queue_conf_t sendq;                 /* 发送队列配置 */
    queue_conf_t distq;                 /* 分发队列配置 */
} rtrd_conf_t;

/* 侦听对象 */
typedef struct
{
    pthread_t tid;                      /* 侦听线程ID */
    log_cycle_t *log;                   /* 日志对象 */
    int cmd_sck_id;                     /* 命令套接字 */
    int lsn_sck_id;                     /* 侦听套接字 */

    uint64_t sid;                       /* Session ID */
} rtrd_listen_t;

/* 套接字信息 */
typedef struct _rtrd_sck_t
{
    int fd;                             /* 套接字ID */
    int nodeid;                         /* 结点ID */
    uint64_t sid;                       /* Session ID */

    time_t ctm;                         /* 创建时间 */
    time_t rdtm;                        /* 最近读取时间 */
    time_t wrtm;                        /* 最近写入时间 */
    char ipaddr[IP_ADDR_MAX_LEN];       /* IP地址 */

    int auth_succ;                      /* 鉴权成功(1:成功 0:失败)  */

    rtmq_snap_t recv;                   /* 接收快照 */
    wiov_t send;                        /* 发送缓存 */

    list_t *mesg_list;                  /* 发送消息链表 */

    uint64_t recv_total;                /* 接收的数据条数 */
} rtrd_sck_t;

/* DEV->SVR的映射表 */
typedef struct
{
    int nodeid;                         /* 结点ID */

    int num;                            /* 当前实际长度 */
#define RTRD_NODE_TO_SVR_MAX_LEN    (32)
    int rsvr_id[RTRD_NODE_TO_SVR_MAX_LEN]; /* 结点ID对应的接收服务ID */
} rtrd_node_to_svr_map_t;

/* 接收对象 */
typedef struct
{
    int id;                             /* 对象ID */
    log_cycle_t *log;                   /* 日志对象 */

    int cmd_sck_id;                     /* 命令套接字 */

    int max;                            /* 最大套接字 */
    time_t ctm;                         /* 当前时间 */
    fd_set rdset;                       /* 可读集合 */
    fd_set wrset;                       /* 可写集合 */
    list2_t *conn_list;                 /* 套接字链表 */

    /* 统计信息 */
    uint32_t connections;               /* TCP连接数 */
    uint64_t recv_total;                /* 获取的数据总条数 */
    uint64_t err_total;                 /* 错误的数据条数 */
    uint64_t drop_total;                /* 丢弃的数据条数 */
} rtrd_rsvr_t;

/* 全局对象 */
typedef struct
{
    rtrd_conf_t conf;                   /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */

    avl_tree_t *reg;                    /* 回调注册对象(注: 存储rtmq_reg_t数据) */

    rtrd_listen_t listen;               /* 侦听对象 */
    thread_pool_t *recvtp;              /* 接收线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */

    int cmd_sck_id;                     /* 命令套接字(注: 用于给各线程发送命令) */
    spinlock_t cmd_sck_lock;            /* 命令套接字锁 */

    queue_t **recvq;                    /* 接收队列(内部队列) */
    queue_t **sendq;                    /* 发送队列(内部队列) */
    queue_t **distq;                    /* 分发队列(外部队列)
                                           注: 外部接口首先将要发送的数据放入
                                           此队列, 再从此队列分发到不同的线程队列 */

    pthread_rwlock_t node_to_svr_map_lock;  /* 读写锁: NODE->SVR映射表 */
    avl_tree_t *node_to_svr_map;         /* NODE->SVR的映射表(以nodeid为主键) */
} rtrd_cntx_t;

/* 外部接口 */
rtrd_cntx_t *rtrd_init(const rtrd_conf_t *conf, log_cycle_t *log);
int rtrd_register(rtrd_cntx_t *ctx, int type, rtmq_reg_cb_t proc, void *args);
int rtrd_launch(rtrd_cntx_t *ctx);

int rtrd_send(rtrd_cntx_t *ctx, int type, int dest, void *data, size_t len);

/* 内部接口 */
int rtrd_lsn_init(rtrd_cntx_t *ctx);
void *rtrd_lsn_routine(void *_ctx);
int rtrd_lsn_destroy(rtrd_listen_t *lsn);

void *rtrd_dsvr_routine(void *_ctx);

void *rtrd_rsvr_routine(void *_ctx);
int rtrd_rsvr_init(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, int tidx);

void *rtrd_worker_routine(void *_ctx);
int rtrd_worker_init(rtrd_cntx_t *ctx, rtmq_worker_t *worker, int tidx);

void rtrd_rsvr_del_all_conn_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);

int rtrd_cmd_to_rsvr(rtrd_cntx_t *ctx, int cmd_sck_id, const rtmq_cmd_t *cmd, int idx);
int rtrd_link_auth_check(rtrd_cntx_t *ctx, rtmq_link_auth_req_t *link_auth_req);

shm_queue_t *rtrd_shm_distq_creat(const rtrd_conf_t *conf, int idx);
shm_queue_t *rtrd_shm_distq_attach(const rtrd_conf_t *conf, int idx);

int rtrd_node_to_svr_map_init(rtrd_cntx_t *ctx);
int rtrd_node_to_svr_map_add(rtrd_cntx_t *ctx, int nodeid, int rsvr_idx);
int rtrd_node_to_svr_map_rand(rtrd_cntx_t *ctx, int nodeid);
int rtrd_node_to_svr_map_del(rtrd_cntx_t *ctx, int nodeid, int rsvr_idx);

#endif /*__RTRD_RECV_H__*/

#if !defined(__SDTP_RECV_H__)
#define __SDTP_RECV_H__

#include "log.h"
#include "sck.h"
#include "list.h"
#include "comm.h"
#include "list2.h"
#include "queue.h"
#include "shm_opt.h"
#include "avl_tree.h"
#include "sdtp_comm.h"
#include "shm_queue.h"
#include "thread_pool.h"

/* 宏定义 */
#define SDTP_CTX_POOL_SIZE      (5 * MB)/* 全局内存池空间 */

/* Recv线程的UNIX-UDP路径 */
#define sdrd_rsvr_usck_path(conf, path, id) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/usck/%s_rsvr_%d.usck", conf->name, conf->name, id+1)
/* Worker线程的UNIX-UDP路径 */
#define sdrd_worker_usck_path(conf, path, id) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/usck/%s_wsvr_%d.usck", conf->name, conf->name, id+1)
/* Listen线程的UNIX-UDP路径 */
#define sdrd_lsn_usck_path(conf, path) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/usck/%s_listen.usck", conf->name, conf->name)
/* 分发队列的共享内存KEY路径 */
#define sdrd_shm_distq_path(conf, path) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/%s_shm_sendq", conf->name, conf->name)

/* 配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];       /* 服务名: 不允许重复出现 */

    sdtp_auth_conf_t auth;              /* 鉴权配置 */

    int port;                           /* 侦听端口 */
    int recv_thd_num;                   /* 接收线程数 */
    int work_thd_num;                   /* 工作线程数 */
    int recvq_num;                      /* 接收队列数 */

    queue_conf_t recvq;                 /* 接收队列配置 */
    queue_conf_t sendq;                 /* 发送队列配置 */
} sdrd_conf_t;

/* 侦听对象 */
typedef struct
{
    pthread_t tid;                      /* 侦听线程ID */
    log_cycle_t *log;                   /* 日志对象 */
    int cmd_sck_id;                     /* 命令套接字 */
    int lsn_sck_id;                     /* 侦听套接字 */

    uint64_t sid;                       /* Session ID */
} sdrd_lsn_t;

/* 套接字信息 */
typedef struct _sdrd_sck_t
{
    int fd;                             /* 套接字ID */
    int nid;                            /* 结点ID */
    uint64_t sid;                       /* Session ID */

    time_t ctm;                         /* 创建时间 */
    time_t rdtm;                        /* 最近读取时间 */
    time_t wrtm;                        /* 最近写入时间 */
    char ipaddr[IP_ADDR_MAX_LEN];       /* IP地址 */

    int auth_succ;                      /* 鉴权成功(1:成功 0:失败)  */

    sdtp_snap_t recv;                   /* 接收快照 */
    sdtp_snap_t send;                   /* 发送快照 */

    list_t *mesg_list;                  /* 发送消息链表 */

    uint64_t recv_total;                /* 接收的数据条数 */
} sdrd_sck_t;

/* DEV->SVR的映射 */
typedef struct
{
    int rsvr_idx;                       /* 接收服务的索引(链表主键) */
    int count;                          /* 引用计数 */
} sdrd_node_to_svr_item_t;

/* DEV->SVR列表 */
typedef struct
{
    int nid;                            /* 结点ID */
    list_t *list;                       /* 结点对应的RSVR列表 */
} sdrd_dev_to_rsvr_map_t;

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

    /* 队列缓存 */
    struct
    {
        int rqid;
        void *start;
        void *addr;
        void *end;
        sdtp_group_t *group;
        int size;
        time_t alloc_tm;
    } queue;

    /* 统计信息 */
    uint32_t connections;               /* TCP连接数 */
    uint64_t recv_total;                /* 获取的数据总条数 */
    uint64_t err_total;                 /* 错误的数据条数 */
    uint64_t drop_total;                /* 丢弃的数据条数 */
} sdrd_rsvr_t;

/* 服务端外部对象 */
typedef struct
{
    shm_queue_t *sendq;                 /* 发送队列 */
} sdrd_cli_t;

/* 全局对象 */
typedef struct
{
    sdrd_conf_t conf;                   /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */

    sdtp_reg_t reg[SDTP_TYPE_MAX];      /* 回调注册对象 */

    sdrd_lsn_t listen;                 /* 侦听对象 */
    thread_pool_t *recvtp;              /* 接收线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */

    queue_t **recvq;                    /* 接收队列(内部队列) */
    queue_t **sendq;                    /* 发送队列(内部队列) */
    shm_queue_t *distq;                 /* 分发队列(外部队列)
                                           注: 外部接口首先将要发送的数据放入
                                           此队列, 再从此队列分发到不同的线程队列 */

    pthread_rwlock_t node_to_svr_map_lock;  /* 读写锁: NODE->SVR映射表 */
    avl_tree_t *node_to_svr_map;         /* NODE->SVR的映射表(以nid为主键 sdrd_node_item_t) */
} sdrd_cntx_t;

/* 外部接口 */
sdrd_cntx_t *sdrd_init(const sdrd_conf_t *conf, log_cycle_t *log);
int sdrd_register(sdrd_cntx_t *ctx, int type, sdtp_reg_cb_t proc, void *args);
int sdrd_launch(sdrd_cntx_t *ctx);

sdrd_cli_t *sdrd_cli_init(const sdrd_conf_t *conf);
int sdrd_cli_send(sdrd_cli_t *cli, int type, int dest, void *data, size_t len);

/* 内部接口 */
void *sdrd_lsn_routine(void *_ctx);
int sdrd_lsn_destroy(sdrd_lsn_t *lsn);

void *sdrd_dist_routine(void *_ctx);

void *sdrd_rsvr_routine(void *_ctx);
int sdrd_rsvr_init(sdrd_cntx_t *ctx, sdrd_rsvr_t *rsvr, int id);

void *sdrd_worker_routine(void *_ctx);
int sdrd_worker_init(sdrd_cntx_t *ctx, sdtp_worker_t *worker, int idx);

void sdrd_rsvr_del_all_conn_hdl(sdrd_cntx_t *ctx, sdrd_rsvr_t *rsvr);

int sdrd_cmd_to_rsvr(sdrd_cntx_t *ctx, int cmd_sck_id, const sdtp_cmd_t *cmd, int idx);
int sdrd_link_auth_check(sdrd_cntx_t *ctx, sdtp_link_auth_req_t *link_auth_req);

shm_queue_t *sdrd_shm_distq_creat(const sdrd_conf_t *conf );
shm_queue_t *sdrd_shm_distq_attach(const sdrd_conf_t *conf);

int sdrd_node_to_svr_map_init(sdrd_cntx_t *ctx);
int sdrd_node_to_svr_map_add(sdrd_cntx_t *ctx, int nid, int rsvr_idx);
int sdrd_node_to_svr_map_rand(sdrd_cntx_t *ctx, int nid);
int sdrd_node_to_svr_map_del(sdrd_cntx_t *ctx, int nid, int rsvr_idx);

#endif /*__SDTP_RECV_H__*/

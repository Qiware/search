#if !defined(__SDTP_H__)
#define __SDTP_H__

#include <stdint.h>

#include "log.h"
#include "sck.h"
#include "slab.h"
#include "list.h"
#include "list2.h"
#include "queue.h"
#include "sdtp_priv.h"
#include "thread_pool.h"

/* 宏定义 */
#define SDTP_THD_DEF_NUM    (01)        /* 默认线程数 */
#define SDTP_THD_MIN_NUM    (01)        /* 最小线程数 */
#define SDTP_THD_MAX_NUM    (64)        /* 最大线程数 */
#define SDTP_CONN_MAX_NUM   (512)       /* 最大链接数 */
#define SDTP_SCK_DEF_NUM    (32)        /* Default SCK number */
#define SDTP_MSG_DEF_LEN    (512)       /* Read length at one time */
#define SDTP_CLOSE_TMOUT    (60)        /* 超时关闭时长 */
#define SDTP_CMD_RESND_TIMES (3)        /* 命令重发次数 */
#define SDTP_CTX_POOL_SIZE  (5 * MB)    /* 全局内存池空间 */

#define SDTP_WORKER_HDL_QNUM (2)        /* 各Worker线程负责的队列数 */

/* Recv线程的UNIX-UDP路径 */
#define sdtp_rsvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/usck/%s_rsvr_%d.usck", \
        conf->name, conf->name, tidx+1)
/* Worker线程的UNIX-UDP路径 */
#define sdtp_worker_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/usck/%s_wsvr_%d.usck", \
        conf->name, conf->name, tidx+1)
/* Listen线程的UNIX-UDP路径 */
#define sdtp_lsn_usck_path(conf, path) \
    snprintf(path, sizeof(path), "../temp/sdtp/recv/%s/usck/%s_listen.usck", \
        conf->name, conf->name)

/* 配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];       /* 服务名: 不允许重复出现 */
    int port;                           /* 侦听端口 */
    int recv_thd_num;                   /* 接收线程数 */
    int work_thd_num;                   /* 工作线程数 */
    int rqnum;                          /* 接收队列数 */

    queue_conf_t recvq;                 /* 队列配置信息 */
} sdtp_conf_t;

/* 回调注册 */
typedef int (*sdtp_reg_cb_t)(uint32_t type, char *buff, size_t len, void *args);
typedef struct
{
    uint32_t type;                      /* 消息类型. Range: 0~SDTP_TYPE_MAX */
#define SDTP_FLAG_UNREG     (0)         /* 0: 未注册 */
#define SDTP_FLAG_REGED     (1)         /* 1: 已注册 */
    uint32_t flag;                      /* 注册标识 
                                            - 0: 未注册
                                            - 1: 已注册 */
    sdtp_reg_cb_t proc;                 /* 回调函数指针 */
    void *args;                         /* 附加参数 */
} sdtp_reg_t;

/* 侦听对象 */
typedef struct
{
    pthread_t tid;                      /* 侦听线程ID */
    log_cycle_t *log;                   /* 日志对象 */
    int cmd_sck_id;                     /* 命令套接字 */
    int lsn_sck_id;                     /* 侦听套接字 */

    uint64_t total;                     /* 连接请求总数 */
} sdtp_lsn_t;

/* 套接字信息 */
typedef struct _sdtp_sck_t
{
    int fd;                             /* 套接字ID */
    time_t ctm;                         /* 创建时间 */
    time_t rdtm;                        /* 最近读取时间 */
    time_t wrtm;                        /* 最近写入时间 */
    char ipaddr[IP_ADDR_MAX_LEN];       /* IP地址 */

    sdtp_snap_t recv;                   /* 接收快照 */
    sdtp_snap_t send;                   /* 发送快照 */

    list_t *mesg_list;                  /* 发送消息链表 */

    uint64_t recv_total;                /* 接收的数据条数 */
} sdtp_sck_t;

/* 接收对象 */
typedef struct
{
    int tidx;                           /* 线程索引 */
    slab_pool_t *pool;                  /* 内存池 */
    log_cycle_t *log;                   /* 日志对象 */

    int cmd_sck_id;                     /* 命令套接字 */

    int max;                            /* 最大套接字 */
    time_t ctm;                         /* 当前时间 */
    fd_set rdset;                       /* 可读集合 */
    fd_set wrset;                       /* 可写集合 */
    list2_t conn_list;                  /* 套接字链表 */

    /* 队列缓存 */
    struct
    {
        int rqid;
        void *start;
        void *addr;
        void *end;
        int *num;
        int size;
        time_t alloc_tm;
    } queue;

    /* 统计信息 */
    uint32_t connections;               /* TCP连接数 */
    uint64_t recv_total;                /* 获取的数据总条数 */
    uint64_t err_total;                 /* 错误的数据条数 */
    uint64_t drop_total;                /* 丢弃的数据条数 */
} sdtp_rsvr_t;

/* 工作对象 */
typedef struct
{
    int tidx;                           /* 线程索引 */
    log_cycle_t *log;                   /* 日志对象 */

    int cmd_sck_id;                     /* 命令套接字 */

    int max;                            /* 套接字最大值 */
    fd_set rdset;                       /* 可读套接字集合 */

    uint64_t proc_total;                /* 已处理条数 */
    uint64_t drop_total;                /* 丢弃条数 */
    uint64_t err_total;                 /* 错误条数 */
} sdtp_worker_t;

/* 全局对象 */
typedef struct
{
    sdtp_conf_t conf;                   /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */ 
    slab_pool_t *pool;                  /* 内存池对象 */

    sdtp_reg_t reg[SDTP_TYPE_MAX];      /* 回调注册对象 */
    
    sdtp_lsn_t listen;                  /* 侦听对象 */
    thread_pool_t *recvtp;              /* 接收线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */ 

    queue_t **recvq;                    /* 接收队列 */
} sdtp_cntx_t;

/* 外部接口 */
sdtp_cntx_t *sdtp_init(const sdtp_conf_t *conf, log_cycle_t *log);
int sdtp_register(sdtp_cntx_t *ctx, uint32_t type, sdtp_reg_cb_t proc, void *args);
int sdtp_startup(sdtp_cntx_t *ctx);
int sdtp_destroy(sdtp_cntx_t **ctx);

/* 内部接口 */
void *sdtp_listen_routine(void *_ctx);
int sdtp_listen_destroy(sdtp_lsn_t *lsn);

void *sdtp_rsvr_routine(void *_ctx);
int sdtp_rsvr_init(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, int tidx);

void *sdtp_worker_routine(void *_ctx);
int sdtp_worker_init(sdtp_cntx_t *ctx, sdtp_worker_t *worker, int tidx);

void sdtp_rsvr_del_all_conn_hdl(sdtp_rsvr_t *rsvr);

#endif /*__SDTP_H__*/

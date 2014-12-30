#if !defined(__SMTP_H__)
#define __SMTP_H__

#include <stdint.h>

#include "log.h"
#include "slab.h"
#include "smtp_comm.h"
#include "orm_queue.h"
#include "thread_pool.h"

/* 宏定义 */
#define SMTP_THD_DEF_NUM     (01)       /* 默认线程数 */
#define SMTP_THD_MIN_NUM     (01)       /* 最小线程数 */
#define SMTP_THD_MAX_NUM     (64)       /* 最大线程数 */
#define SMTP_CONN_MAX_NUM    (512)      /* 最大链接数 */
#define SMTP_SCK_DEF_NUM     (32)       /* Default SCK number */
#define SMTP_MSG_DEF_LEN     (512)      /* Read length at one time */
#define SMTP_CLOSE_TMOUT     (60)       /* 超时关闭时长 */
#define SMTP_CMD_RESND_TIMES (3)        /* 命令重发次数 */

#define SMTP_WORKER_HDL_QNUM (2)        /* 各Worker线程负责的队列数 */

/* Recv线程的UNIX-UDP路径 */
#define smtp_rsvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "../temp/smtp/recv/%s/usck/%s_rsvr_%d.usck", \
        conf->name, conf->name, tidx+1)
/* Worker线程的UNIX-UDP路径 */
#define smtp_worker_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "../temp/smtp/recv/%s/usck/%s_wsvr_%d.usck", \
        conf->name, conf->name, tidx+1)
/* Listen线程的UNIX-UDP路径 */
#define smtp_listen_usck_path(conf, path) \
    snprintf(path, sizeof(path), "../temp/smtp/recv/%s/usck/%s_listen.usck", \
        conf->name, conf->name)

/* 配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];       /* 服务名: 不允许重复出现 */
    int port;                           /* 侦听端口 */
    int recv_thd_num;                   /* Recv线程数 */
    int wrk_thd_num;                    /* Work线程数 */
    int recvq_num;                      /* Recv队列数 */

    queue_conf_t recvq;                 /* 队列配置信息 */
} smtp_conf_t;

/* 回调注册 */
typedef int (*smtp_reg_cb_t)(uint32_t type, char *buff, size_t len, void *args);
typedef struct
{
    uint32_t type;                      /* 消息类型. Range: 0~SMTP_TYPE_MAX */
#define SMTP_FLAG_UNREG     (0)         /* 0: 未注册 */
#define SMTP_FLAG_REGED     (1)         /* 1: 已注册 */
    uint32_t flag;                      /* 注册标识 
                                            - 0: 未注册
                                            - 1: 已注册 */
    smtp_reg_cb_t cb;                   /* 回调函数指针 */
    void *args;                         /* 附加参数 */
} smtp_reg_t;

/* 侦听对象 */
typedef struct
{
    pthread_t tid;                      /* 侦听线程ID */
    log_cycle_t *log;                   /* 日志对象 */
    int cmd_sck_id;                     /* 命令套接字 */
    int lsn_sck_id;                     /* 侦听套接字 */

    uint64_t total;                     /* 连接请求总数 */
} smtp_listen_t;

/* 套接字信息 */
typedef struct _smtp_sck_t
{
    int fd;                             /* 套接字ID */
    time_t ctm;                         /* 创建时间 */
    time_t rtm;                         /* 最近读取时间 */
    time_t wtm;                         /* 最近写入时间 */
    char ipaddr[IP_ADDR_MAX_LEN];       /* IP地址 */

    uint8_t is_primary;                 /* 是否为主SCK(一个发送端只有一个主SCK) */
    uint64_t recv_total;                /* 接收的数据条数 */
    smtp_read_snap_t read;              /* 读取操作的快照 */
    smtp_send_snap_t send;              /* 发送操作的快照 */
    list_t *message_list;               /* 发送消息链表 */
    char *null;                         /* NULL */

    struct _smtp_sck_t *next;           /* 下一结点 */
} smtp_sck_t;

/* 接收对象 */
typedef struct
{
    int tidx;                           /* 线程索引 */
    eslab_pool_t pool;                  /* 内存池 */

    int cmd_sck_id;                     /* 命令套接字 */

    int max;                            /* 最大套接字 */
    fd_set rdset;                       /* 可读集合 */
    fd_set wrset;                       /* 可写集合 */
    time_t tm;                          /* 当前时间 */

    uint32_t connections;               /* TCP连接数 */
    uint64_t recv_total;                /* 获取的数据总条数 */
    uint64_t err_total;                 /* 错误的数据条数 */
    uint64_t drop_total;                /* 丢弃的数据条数 */
    uint64_t *delay_total;              /* 滞留处理的数据条数 */

    smtp_sck_t *sck;                    /* 套接字链表 */
} smtp_rsvr_t;

/* 工作对象 */
typedef struct
{
    int tidx;                           /* 线程索引 */

    int cmd_sck_id;                     /* 命令套接字 */

    int max;                            /* 套接字最大值 */
    fd_set rdset;                       /* 可读套接字集合 */

    uint64_t work_total;                /* 已处理条数 */
    uint64_t drop_total;                /* 丢弃条数 */
    uint64_t err_total;                 /* 错误条数 */
} smtp_worker_t;

/* 全局对象 */
typedef struct
{
    smtp_conf_t conf;                   /* 配置信息 */
    log_cycle_t *log;                   /* 日志对象 */ 

    smtp_reg_t reg[SMTP_TYPE_MAX];      /* 回调注册对象 */
    
    smtp_listen_t listen;               /* 侦听对象 */
    thread_pool_t *recvtp;              /* 接收线程池 */
    thread_pool_t *worktp;              /* 工作线程池 */ 

    ORMQUEUE **recvq;                   /* 接收队列 */
} smtp_cntx_t;

/* 外部接口 */
smtp_cntx_t *smtp_init(const smtp_conf_t *conf, log_cycle_t *log);
int smtp_register(smtp_cntx_t *ctx, uint32_t type, smtp_reg_cb_t cb, void *args);
int smtp_startup(smtp_cntx_t *ctx);
int smtp_destroy(smtp_cntx_t **ctx);

/* 内部接口 */
void *smtp_listen_routine(void *args);
int smtp_listen_destroy(smtp_listen_t *lsn);

int smtp_creat_recvtp(smtp_cntx_t *ctx);
void smtp_recvtp_destroy(void *_ctx, void *args);
void *smtp_rsvr_routine(void *args);

int smtp_creat_worktp(smtp_cntx_t *ctx);
void smtp_worktp_destroy(void *_ctx, void *args);
void *smtp_worker_routine(void *args);

int smtp_work_def_hdl(unsigned int type, char *buff, size_t len, void *args);

#endif /*__SMTP_H__*/

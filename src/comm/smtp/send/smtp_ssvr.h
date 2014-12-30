#if !defined(__SMTP_SSVR_H__)
#define __SMTP_SSVR_H__

#include <sys/types.h>

#include "log.h"
#include "slab.h"
#include "avl_tree.h"
#include "shm_queue.h"
#include "thread_pool.h"

typedef enum
{
    SMTP_DATA_ADDR_UNKNOWN
    , SMTP_DATA_ADDR_SNDQ
    , SMTP_DATA_ADDR_STACK
    , SMTP_DATA_ADDR_OTHER
    
    , SMTP_DATA_ADDR_TOTAL
} smtp_ssvr_data_addr_e;

/* 发送端配置 */
typedef struct
{
    char name[SMTP_NAME_MAX_LEN];    /* 发送端名称 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* 服务端IP地址 */
    int port;                       /* 服务端端口号 */
    int snd_thd_num;                /* 发送线程的个数 */

    smtp_cpu_conf_t cpu;             /* CPU亲和性配置 */
    smtp_queue_conf_t send_qcf;      /* 发送队列配置 */
} smtp_ssvr_conf_t;

/* 套接字信息 */
typedef struct
{
    int fd;                         /* 套接字ID */
    time_t wr_tm;                   /* 最近写入操作时间 */
    time_t rd_tm;                   /* 最近读取操作时间 */

    smtp_kpalive_stat_e kpalive;     /* 保活状态 */
    list_t *message_list;           /* 消息列表 */
    char *null;                     /* NULL空间 */
    smtp_read_snap_t read;           /* 读取操作的快照 */
    smtp_send_snap_t send;           /* 发送记录 */
} smtp_ssvr_sck_t;

#define smtp_set_kpalive_stat(sck, _kpalive) (sck)->kpalive = (_kpalive)

/* SND线程上下文 */
typedef struct
{
    int tidx;                       /* 线程索引 */
    shm_queue_t *sq;                /* 发送缓冲队列 */

    int cmd_sck_id;                 /* 命令通信套接字ID */
    smtp_ssvr_sck_t sck;              /* 发送套接字 */

    int max;                        /* 套接字最大值 */
    fd_set rset;                    /* 读集合 */
    fd_set wset;                    /* 写集合 */
    slab_pool_t pool;               /* 内存池 */
} smtp_ssvr_t;

/* 发送端上下文信息 */
typedef struct
{
    smtp_ssvr_conf_t conf;            /* 客户端配置信息 */

    thread_pool_t *sendtp;          /* Send thread pool */
    thread_pool_t *worktp;          /* Work thread pool */
} smtp_ssvr_ctx_t;

/* 内部接口 */
int smtp_ssvr_creat_sendtp(smtp_ssvr_ctx_t *ctx);
void smtp_ssvr_sendtp_destroy(void *_ctx, void *args);

int smtp_ssvr_creat_worktp(smtp_ssvr_ctx_t *ctx);

/* 对外接口 */
smtp_ssvr_ctx_t *smtp_ssvr_startup(const smtp_ssvr_conf_t *conf);
extern void smtp_ssvr_destroy(smtp_ssvr_ctx_t *ctx);

#endif /*__SMTP_SSVR_H__*/

#if !defined(__SMTC_SSVR_H__)
#define __SMTC_SSVR_H__

#include <sys/types.h>

#include "log.h"
#include "slab.h"
#include "avl_tree.h"
#include "shm_queue.h"
#include "thread_pool.h"

typedef enum
{
    SMTC_DATA_ADDR_UNKNOWN
    , SMTC_DATA_ADDR_SNDQ
    , SMTC_DATA_ADDR_STACK
    , SMTC_DATA_ADDR_OTHER
    
    , SMTC_DATA_ADDR_TOTAL
} smtc_ssvr_data_addr_e;

/* 发送端配置 */
typedef struct
{
    char name[SMTC_NAME_MAX_LEN];    /* 发送端名称 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* 服务端IP地址 */
    int port;                       /* 服务端端口号 */
    int snd_thd_num;                /* 发送线程的个数 */

    smtc_cpu_conf_t cpu;             /* CPU亲和性配置 */
    smtc_queue_conf_t send_qcf;      /* 发送队列配置 */
} smtc_ssvr_conf_t;

/* 套接字信息 */
typedef struct
{
    int fd;                         /* 套接字ID */
    time_t wr_tm;                   /* 最近写入操作时间 */
    time_t rd_tm;                   /* 最近读取操作时间 */

    smtc_kpalive_stat_e kpalive;     /* 保活状态 */
    list_t *message_list;           /* 消息列表 */
    char *null;                     /* NULL空间 */
    smtc_read_snap_t read;           /* 读取操作的快照 */
    smtc_send_snap_t send;           /* 发送记录 */
} smtc_ssvr_sck_t;

#define smtc_set_kpalive_stat(sck, _kpalive) (sck)->kpalive = (_kpalive)

/* SND线程上下文 */
typedef struct
{
    int tidx;                       /* 线程索引 */
    shm_queue_t *sq;                /* 发送缓冲队列 */

    int cmd_sck_id;                 /* 命令通信套接字ID */
    smtc_ssvr_sck_t sck;              /* 发送套接字 */

    int max;                        /* 套接字最大值 */
    fd_set rset;                    /* 读集合 */
    fd_set wset;                    /* 写集合 */
    slab_pool_t pool;               /* 内存池 */
} smtc_ssvr_t;

/* 发送端上下文信息 */
typedef struct
{
    smtc_ssvr_conf_t conf;            /* 客户端配置信息 */

    thread_pool_t *sendtp;          /* Send thread pool */
    thread_pool_t *worktp;          /* Work thread pool */
} smtc_ssvr_ctx_t;

/* 内部接口 */
int smtc_ssvr_creat_sendtp(smtc_ssvr_ctx_t *ctx);
void smtc_ssvr_sendtp_destroy(void *_ctx, void *args);

int smtc_ssvr_creat_worktp(smtc_ssvr_ctx_t *ctx);

/* 对外接口 */
smtc_ssvr_ctx_t *smtc_ssvr_startup(const smtc_ssvr_conf_t *conf);
extern void smtc_ssvr_destroy(smtc_ssvr_ctx_t *ctx);

#endif /*__SMTC_SSVR_H__*/

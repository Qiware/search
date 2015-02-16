#if !defined(__SDTP_SSVR_H__)
#define __SDTP_SSVR_H__

#include <sys/types.h>

#include "log.h"
#include "slab.h"
#include "list.h"
#include "avl_tree.h"
#include "shm_queue.h"
#include "thread_pool.h"

/* 发送端配置 */
typedef struct
{
    char name[SDTP_NAME_MAX_LEN];   /* 发送端名称 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* 服务端IP地址 */
    int port;                       /* 服务端端口号 */
    int snd_thd_num;                /* 发送线程数 */

    size_t send_buff_size;          /* 发送缓存大小 */
    size_t recv_buff_size;          /* 接收缓存大小 */
    sdtp_cpu_conf_t cpu;            /* CPU亲和性配置 */
    sdtp_queue_conf_t qcf;          /* 发送队列配置 */
} sdtp_ssvr_conf_t;

/* 套接字信息 */
typedef struct
{
    int fd;                         /* 套接字ID */
    time_t wrtm;                    /* 最近写入操作时间 */
    time_t rdtm;                    /* 最近读取操作时间 */

#define SDTP_KPALIVE_STAT_UNKNOWN   (0) /* 未知状态 */
#define SDTP_KPALIVE_STAT_SENT      (1) /* 已发送保活 */
#define SDTP_KPALIVE_STAT_SUCC      (2) /* 保活成功 */
    int kpalive;                    /* 保活状态
                                        0: 未知状态
                                        1: 已发送保活
                                        2: 保活成功 */
    list_t *mesg_list;              /* 发送链表 */

    sdtp_snap_t recv;               /* 接收快照 */
    sdtp_snap_t send;               /* 发送快照 */
} sdtp_ssvr_sck_t;

#define sdtp_set_kpalive_stat(sck, _stat) (sck)->kpalive = (_stat)

/* SND线程上下文 */
typedef struct
{
    int tidx;                       /* 线程索引 */
    shm_queue_t *sq;                /* 发送缓冲队列 */
    log_cycle_t *log;               /* 日志对象 */

    int cmd_sck_id;                 /* 命令通信套接字ID */
    sdtp_ssvr_sck_t sck;            /* 发送套接字 */

    int max;                        /* 套接字最大值 */
    fd_set rset;                    /* 读集合 */
    fd_set wset;                    /* 写集合 */
    slab_pool_t *pool;              /* 内存池 */
} sdtp_ssvr_t;

/* 发送端上下文信息 */
typedef struct
{
    sdtp_ssvr_conf_t conf;          /* 客户端配置信息 */
    log_cycle_t *log;               /* 日志对象 */
    slab_pool_t *slab;              /* 内存池对象 */

    thread_pool_t *sendtp;          /* 发送线程池 */
    thread_pool_t *worktp;          /* 工作线程池 */
} sdtp_ssvr_cntx_t;

/* 对外接口 */
sdtp_ssvr_cntx_t *sdtp_ssvr_startup(const sdtp_ssvr_conf_t *conf, log_cycle_t *log);

#endif /*__SDTP_SSVR_H__*/

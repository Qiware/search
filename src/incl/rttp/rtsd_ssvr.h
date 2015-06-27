#if !defined(__RTSD_SSVR_H__)
#define __RTSD_SSVR_H__

#include "log.h"
#include "slab.h"
#include "list.h"
#include "avl_tree.h"
#include "shm_queue.h"
#include "rttp_comm.h"
#include "thread_pool.h"

/* WORKER线程的UNIX-UDP路径 */
#define rtsd_worker_usck_path(conf, path, id) \
    snprintf(path, sizeof(path), "%s/%d_swrk_%d.usck", (conf)->path, (conf)->nodeid, id+1)

/* 套接字信息 */
typedef struct
{
    int fd;                             /* 套接字ID */
    time_t wrtm;                        /* 最近写入操作时间 */
    time_t rdtm;                        /* 最近读取操作时间 */

#define RTTP_KPALIVE_STAT_UNKNOWN   (0) /* 未知状态 */
#define RTTP_KPALIVE_STAT_SENT      (1) /* 已发送保活 */
#define RTTP_KPALIVE_STAT_SUCC      (2) /* 保活成功 */
    int kpalive;                        /* 保活状态
                                            0: 未知状态
                                            1: 已发送保活
                                            2: 保活成功 */
    list_t *mesg_list;                  /* 发送链表 */

    rttp_snap_t recv;                   /* 接收快照 */
    rttp_snap_t send;                   /* 发送快照 */
} rtsd_sck_t;

#define rttp_set_kpalive_stat(sck, _stat) (sck)->kpalive = (_stat)

/* SND线程上下文 */
typedef struct
{
    int id;                             /* 对象ID */
    shm_queue_t *sendq;                 /* 发送缓存 */
    log_cycle_t *log;                   /* 日志对象 */

    int cmd_sck_id;                     /* 命令通信套接字ID */
    rtsd_sck_t sck;                    /* 发送套接字 */

    int max;                            /* 套接字最大值 */
    fd_set rset;                        /* 读集合 */
    fd_set wset;                        /* 写集合 */
    slab_pool_t *pool;                  /* 内存池 */

    /* 统计信息 */
    uint64_t recv_total;                /* 获取的数据总条数 */
    uint64_t err_total;                 /* 错误的数据条数 */
    uint64_t drop_total;                /* 丢弃的数据条数 */
} rtsd_ssvr_t;

#endif /*__RTSD_SSVR_H__*/

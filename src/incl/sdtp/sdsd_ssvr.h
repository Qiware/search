#if !defined(__SDTP_SSVR_H__)
#define __SDTP_SSVR_H__

#include "log.h"
#include "slab.h"
#include "list.h"
#include "avl_tree.h"
#include "sdtp_comm.h"
#include "sdsd_pool.h"
#include "thread_pool.h"

/* WORKER线程的UNIX-UDP路径 */
#define sdsd_worker_usck_path(conf, path, id) \
    snprintf(path, sizeof(path), "../temp/sdtp/send/%s/usck/%s_swrk_%d.usck", conf->name, conf->name, id+1)

/* 发送类型 */
typedef enum
{
    SDTP_SNAP_SHOT_SYS_DATA
    , SDTP_SNAP_SHOT_EXP_DATA 

    , SDTP_SNAP_SHOT_TOTAL
} sdtp_send_snap_e;

/* 套接字信息 */
typedef struct
{
    int fd;                             /* 套接字ID */
    time_t wrtm;                        /* 最近写入操作时间 */
    time_t rdtm;                        /* 最近读取操作时间 */

#define SDTP_KPALIVE_STAT_UNKNOWN   (0) /* 未知状态 */
#define SDTP_KPALIVE_STAT_SENT      (1) /* 已发送保活 */
#define SDTP_KPALIVE_STAT_SUCC      (2) /* 保活成功 */
    int kpalive;                        /* 保活状态
                                            0: 未知状态
                                            1: 已发送保活
                                            2: 保活成功 */
    list_t *mesg_list;                  /* 发送链表 */

    sdtp_snap_t recv;                   /* 接收快照 */
    sdtp_send_snap_e send_type;         /* 发送类型(系统数据或自定义数据) */
    sdtp_snap_t send[SDTP_SNAP_SHOT_TOTAL];  /* 发送快照 */
} sdsd_sck_t;

#define sdtp_set_kpalive_stat(sck, _stat) (sck)->kpalive = (_stat)

/* SND线程上下文 */
typedef struct
{
    int id;                             /* 对象ID */
    sdsd_pool_t *sendq;                 /* 发送缓存 */
    log_cycle_t *log;                   /* 日志对象 */

    int cmd_sck_id;                     /* 命令通信套接字ID */
    sdsd_sck_t sck;                    /* 发送套接字 */

    int max;                            /* 套接字最大值 */
    fd_set rset;                        /* 读集合 */
    fd_set wset;                        /* 写集合 */
    slab_pool_t *pool;                  /* 内存池 */

    /* 统计信息 */
    uint64_t recv_total;                /* 获取的数据总条数 */
    uint64_t err_total;                 /* 错误的数据条数 */
    uint64_t drop_total;                /* 丢弃的数据条数 */
} sdsd_ssvr_t;

#endif /*__SDTP_SSVR_H__*/

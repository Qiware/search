#if !defined(__SMTC_PRIV_H__)
#define __SMTC_PRIV_H__

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "queue.h"
#include "common.h"

#define SMTC_NAME_MAX_LEN    (64)    /* 名称长度 */
#define SMTC_SCK_INVALID     (-1)    /* 套接字非法值 */
#define SMTC_RECV_DEF_LEN    (512)   /* 一次默认接收长度 */

#define SMTC_RECONN_MIN_INTV (4)     /* 连接重连最小间隔 */
#define SMTC_RECONN_MAX_INTV (16)    /* 连接重连最大间隔 */

#define SMTC_RECV_TMOUT_SEC  (01)    /* 侦听超时: 秒 */
#define SMTC_RECV_TMOUT_USEC (0)     /* 侦听超时: 微妙 */

#define SMTC_SND_TMOUT_SEC   (05)    /* SND超时: 秒 */
#define SMTC_SND_TMOUT_USEC  (0)     /* SND超时: 微妙 */

#define SMTC_SYS_DATA        (0)     /* 系统数据类型 */
#define SMTC_EXP_DATA        (1)     /* 自定义数据类型 */
#define SMTC_MSG_MARK_KEY    (987654321) /* 校验值 */
#define SMTC_TYPE_MAX        (512)   /* 自定义数据类型的最大值 */

#define SMTC_FAIL_CMDQ_MAX   (1000)  /* 失败命令的队列长度 */
#define SMTC_THD_KPALIVE_SEC (30)    /* THD保活间隔时间 */
#define SMTC_SCK_KPALIVE_SEC (30)    /* SCK保活间隔时间 */

#define SMTC_KPALIVE_DATAID  (0x10000000)    /* 保活数据ID */
#define SMTC_NOT_NULL_DATAID (0x10000000)    /* 非空数据 */
#define SMTC_MEM_POOL_SIZE   (10*1024*1024)  /* 内存池大小 */

/* 系统数据类型 */
typedef enum
{
    SMTC_UNKNOWN_DATA                /* 未知数据类型 */
    , SMTC_KPALIVE_REQ               /* 链路保活请求 */
    , SMTC_KPALIVE_REP               /* 链路保活应答 */
    , SMTC_LINK_INFO_REPORT          /* 链路信息报告 */

    /*******************在此线以上添加系统数据类型****************************/
    , SMTC_DATA_TYPE_TOTAL
} smtc_data_type_e;

/* 套接字状态 */
typedef enum
{
    SMTC_SCK_STAT_UNKNOWN            /* 未知状态 */
    , SMTC_SCK_STAT_OK               /* 状态正常 */
    , SMTC_SCK_STAT_EAGAIN           /* 套接字不可写 */

    , SMTC_SCK_STAT_TOTAL
} smtc_sck_stat_e;

/* 保活状态 */
typedef enum
{
    SMTC_KPALIVE_STAT_UNKNOWN        /* 未知状态 */
    , SMTC_KPALIVE_SENT              /* 已发送保活 */
    , SMTC_KPALIVE_SUCC              /* 保活成功 */

    , SMTC_KPALIVE_STAT_TOTAL        /* 保活状态总数 */
} smtc_kpalive_stat_e;

/* 返回码 */
typedef enum
{
    SMTC_OK = 0                      /* 成功 */
    , SMTC_DONE                      /* 处理完成 */
    , SMTC_RECONN                    /* 重连处理 */
    , SMTC_HDL_DONE                  /* 处理完成 */
    , SMTC_HDL_DISCARD               /* 放弃处理 */

    , SMTC_AGAIN                     /* 未完成 */
    , SMTC_SCK_CLOSED                /* 套接字被关闭*/

    , SMTC_ERR = ~0x7FFFFFFF         /* 失败 */
    , SMTC_ERR_CALLOC                /* Calloc错误 */
    , SMTC_ERR_QALLOC                /* 申请Queue空间失败 */
    , SMTC_ERR_QSIZE                 /* Queue的单元空间不足 */
    , SMTC_ERR_QUEUE_NOT_ENOUGH      /* 队列空间不足 */
    , SMTC_ERR_DATA_TYPE             /* 错误的数据类型 */
    , SMTC_ERR_RECV_CMD              /* 命令接收失败 */
    , SMTC_ERR_REPEAT_REG            /* 重复注册 */
    , SMTC_ERR_UNKNOWN_CMD           /* 未知命令类型 */
} SMTC_ERR_CODE_e;

/* 当前处理阶段 */
typedef enum
{
    SMTC_PHASE_READ_INIT             /* Init Read */
    , SMTC_PHASE_READ_HEAD           /* Read Head */
    , SMTC_PHASE_READ_BODY           /* Read Body */
    , SMTC_PHASE_READ_POST           /* Read Post */

    , SMTC_PHASE_UNKNOWN             /* Unknown phase */
} smtc_phase_e;

/* 数据存储位置 */
typedef enum
{
    SMTC_DATA_LOC_UNKNOWN            /* 未知空间 */
    , SMTC_DATA_LOC_HEAP             /* 堆 */
    , SMTC_DATA_LOC_STACK            /* 栈 */
    , SMTC_DATA_LOC_STATIC           /* 静态 */
    , SMTC_DATA_LOC_SLAB             /* Slab空间 */

    , SMTC_DATA_LOC_ORM_QUEUE        /* OrmQueue */
    , SMTC_DATA_LOC_RECVQ            /* 接收队列 */

    , SMTC_DATA_LOC_OTHER            /* 其他 */
    
    , SMTC_DATA_LOC_TOTAL            /* 存储位置总数 */
} smtc_data_loc_e;

/* 接收快照 */
typedef struct
{
    smtc_phase_e phase;                  /* Range: SMTC_PHASE_READ_INIT ~ SMTC_PHASE_READ_POST */
    unsigned int rqidx;                 /* Recv队列索引 */

    uint32_t dataid;                    /* Data ID */
    void *addr;                         /* Data address */
    int total;                          /* Total characters need to be recv */
    int offset;                         /* Have received 'offset' characters */
} smtc_read_snap_t;

/* State of read operation */
#define smtc_reset_read_snap(recv) \
{ \
    memset(recv, 0, sizeof(smtc_read_snap_t));\
    \
    recv->addr = NULL; \
    recv->phase = SMTC_PHASE_READ_INIT; \
} 

#define smtc_set_read_phase(recv, _phase) ((recv)->phase = (_phase))
#define smtc_get_read_phase(recv) ((recv)->phase)
#define smtc_is_read_phase(recv, _phase) ((_phase) == (recv)->phase)

/* 发送快照 */
typedef struct
{
    /* 正在发送的数据 */
    size_t total;                   /* 需要发送的字符总数 */
    size_t off;                     /* 当前偏移量 */
    char *addr;                     /* 发送数据的起始地址 */
#if defined(__SMTC_DEBUG__)
    /* 发送统计 */
    unsigned long long succ;        /* 成功数 */
    unsigned long long fail;        /* 失败数 */
    unsigned long long again;       /* EAGAIN次数 */
#endif /*__SMTC_DEBUG__*/
} smtc_send_snap_t;

/* 重置发送记录 */
#define smtc_reset_send_snap(s) \
{ \
    s->total = 0; \
    s->off = 0; \
    s->addr = NULL; \
} 

/* 发送数据头部信息 */
typedef struct
{
    unsigned int type;              /* 数据类型 Range:0~65536 */
    int length;                     /* 消息体的长度 */
    int flag;                       /* 消息标志: Range: SMTC_DATA_FLAG */
    int mark;                       /* 校验值 */
} __attribute__((packed)) smtc_header_t;

#define smtc_is_sys_data(head) (SMTC_SYS_DATA == (head)->flag)
#define smtc_is_exp_data(head) (SMTC_EXP_DATA == (head)->flag)

#define smtc_is_type_valid(type) (type < SMTC_TYPE_MAX)
#define smtc_is_len_valid(q, len) (q->queue.size >= (sizeof(smtc_header_t)+ (len)))

/* 队列配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];   /* 队列名 */
    int size;                       /* 单元大小 */
    int count;                      /* 队列长度 */
    int flag;                       /* 新建标志:0-获取句柄 1-新建 */
} smtc_queue_conf_t;

/* 绑定CPU配置信息 */
typedef struct
{
    short ison;                     /* 是否开启绑定CPU功能 */
    short start;                    /* 绑定CPU的起始CPU编号 */
} smtc_cpu_conf_t;

/* 链路信息报告 */
typedef struct
{
    uint8_t is_primary;
} smtc_link_info_report_t;

#endif /*__SMTC_PRIV_H__*/

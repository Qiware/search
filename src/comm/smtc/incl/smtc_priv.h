#if !defined(__SMTC_PRIV_H__)
#define __SMTC_PRIV_H__

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "log.h"
#include "queue.h"
#include "common.h"
#include "sck_api.h"

#define SMTC_NAME_MAX_LEN       (64)    /* 名称长度 */
#define SMTC_RECONN_INTV        (2)     /* 连接重连间隔 */
#define SMTC_KPALIVE_INTV       (30)    /* 保活间隔 */
#define SMTC_BUFF_SIZE          (5 * MB)/* 发送/接收缓存SIZE */

#define SMTC_SSVR_TMOUT_SEC     (1)     /* SND超时: 秒 */
#define SMTC_SSVR_TMOUT_USEC    (0)     /* SND超时: 微妙 */

#define SMTC_TYPE_MAX           (0xFF)  /* 自定义数据类型的最大值 */

#define SMTC_MEM_POOL_SIZE      (10*MB) /* 内存池大小 */

/* 系统数据类型 */
typedef enum
{
    SMTC_UNKNOWN_DATA               /* 未知数据类型 */
    , SMTC_KPALIVE_REQ              /* 链路保活请求 */
    , SMTC_KPALIVE_REP              /* 链路保活应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , SMTC_DATA_TYPE_TOTAL
} smtc_sys_mesg_e;

/* 返回码 */
typedef enum
{
    SMTC_OK = 0                     /* 成功 */
    , SMTC_DONE                     /* 处理完成 */
    , SMTC_RECONN                   /* 重连处理 */
    , SMTC_AGAIN                    /* 未完成 */
    , SMTC_DISCONN                  /* 连接断开 */

    , SMTC_ERR = ~0x7FFFFFFF        /* 失败 */
    , SMTC_ERR_CALLOC               /* Calloc错误 */
    , SMTC_ERR_QALLOC               /* 申请Queue空间失败 */
    , SMTC_ERR_QSIZE                /* Queue的单元空间不足 */
    , SMTC_ERR_QUEUE_NOT_ENOUGH     /* 队列空间不足 */
    , SMTC_ERR_DATA_TYPE            /* 错误的数据类型 */
    , SMTC_ERR_RECV_CMD             /* 命令接收失败 */
    , SMTC_ERR_REPEAT_REG           /* 重复注册 */
    , SMTC_ERR_UNKNOWN_CMD          /* 未知命令类型 */
} smtc_err_e;

/* 接收/发送快照 */
typedef struct
{
    /*  |<------------       total      --------------->|
     *  | 已发送 |           未发送          | 空闲空间 |
     *  | 已处理 |           已接收          | 空闲空间 |
     *   -----------------------------------------------
     *  |XXXXXXXX|///////////////////////////|          |
     *  |XXXXXXXX|///////////////////////////|<--left-->|
     *  |XXXXXXXX|///////////////////////////|          |
     *   -----------------------------------------------
     *  ^        ^                           ^          ^
     *  |        |                           |          |
     * addr     optr                        iptr       end
     */
    char *addr;                     /* 发送缓存 */
    char *end;                      /* 结束地址 */

    int total;                      /* 缓存大小 */

    char *optr;                     /* 发送偏移 */
    char *iptr;                     /* 输入偏移 */
} smtc_snap_t;

#define smtc_snap_setup(snap, _addr, _total) \
   (snap)->addr = (_addr);  \
   (snap)->end = (_addr) + (_total); \
   (snap)->total = (_total); \
   (snap)->optr = (_addr);  \
   (snap)->iptr = (_addr); 

#define smtc_snap_reset(snap)   /* 重置标志 */\
   (snap)->optr = (snap)->addr;  \
   (snap)->iptr = (snap)->addr; 

/* 报头结构 */
typedef struct
{
    uint16_t type;                  /* 数据类型 */
#define SMTC_SYS_MESG   (0)         /* 系统类型 */
#define SMTC_EXP_MESG   (1)         /* 自定义类型 */
    uint8_t flag;                   /* 消息标志
                                        - 0: 系统消息(type: smtc_sys_mesg_e)
                                        - 1: 自定义消息(type: 0x0000~0xFFFF) */
    uint32_t length;                /* 消息体的长度 */
#define SMTC_CHECK_SUM  (0x1FE23DC4)
    uint32_t checksum;              /* 校验值 */
} __attribute__((packed)) smtc_header_t;

#define smtc_is_type_valid(type) (type < SMTC_TYPE_MAX)
#define smtc_is_len_valid(q, len) (q->queue.size >= (sizeof(smtc_header_t)+ (len)))

/* 队列配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];   /* 队列路径 */
    int size;                       /* 单元大小 */
    int count;                      /* 队列长度 */
} smtc_queue_conf_t;

/* 绑定CPU配置信息 */
typedef struct
{
    short ison;                     /* 是否开启绑定CPU功能 */
    short start;                    /* 绑定CPU的起始CPU编号 */
} smtc_cpu_conf_t;

#endif /*__SMTC_PRIV_H__*/

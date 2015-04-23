#if !defined(__SDTP_PRIV_H__)
#define __SDTP_PRIV_H__

#include "sck.h"
#include "log.h"
#include "comm.h"
#include "queue.h"

#define SDTP_NAME_MAX_LEN       (64)    /* 名称长度 */
#define SDTP_RECONN_INTV        (2)     /* 连接重连间隔 */
#define SDTP_KPALIVE_INTV       (30)    /* 保活间隔 */
#define SDTP_BUFF_SIZE          (5 * MB)/* 发送/接收缓存SIZE */

#define SDTP_SSVR_TMOUT_SEC     (1)     /* SND超时: 秒 */
#define SDTP_SSVR_TMOUT_USEC    (0)     /* SND超时: 微妙 */

#define SDTP_TYPE_MAX           (0xFF)  /* 自定义数据类型的最大值 */

#define SDTP_MEM_POOL_SIZE      (10*MB) /* 内存池大小 */

/* 系统数据类型 */
typedef enum
{
    SDTP_UNKNOWN_DATA               /* 未知数据类型 */
    , SDTP_KPALIVE_REQ              /* 链路保活请求 */
    , SDTP_KPALIVE_REP              /* 链路保活应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , SDTP_DATA_TYPE_TOTAL
} sdtp_sys_mesg_e;

/* 返回码 */
typedef enum
{
    SDTP_OK = 0                     /* 成功 */
    , SDTP_DONE                     /* 处理完成 */
    , SDTP_RECONN                   /* 重连处理 */
    , SDTP_AGAIN                    /* 未完成 */
    , SDTP_DISCONN                  /* 连接断开 */

    , SDTP_ERR = ~0x7FFFFFFF        /* 失败 */
    , SDTP_ERR_CALLOC               /* Calloc错误 */
    , SDTP_ERR_QALLOC               /* 申请Queue空间失败 */
    , SDTP_ERR_QSIZE                /* Queue的单元空间不足 */
    , SDTP_ERR_QUEUE_NOT_ENOUGH     /* 队列空间不足 */
    , SDTP_ERR_DATA_TYPE            /* 错误的数据类型 */
    , SDTP_ERR_RECV_CMD             /* 命令接收失败 */
    , SDTP_ERR_REPEAT_REG           /* 重复注册 */
    , SDTP_ERR_UNKNOWN_CMD          /* 未知命令类型 */
} sdtp_err_e;

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
} sdtp_snap_t;

#define sdtp_snap_setup(snap, _addr, _total) \
   (snap)->addr = (_addr);  \
   (snap)->end = (_addr) + (_total); \
   (snap)->total = (_total); \
   (snap)->optr = (_addr);  \
   (snap)->iptr = (_addr); 

#define sdtp_snap_reset(snap)   /* 重置标志 */\
   (snap)->optr = (snap)->addr;  \
   (snap)->iptr = (snap)->addr; 

/* 报头结构 */
typedef struct
{
    uint16_t type;                  /* 数据类型 */
#define SDTP_SYS_MESG   (0)         /* 系统类型 */
#define SDTP_EXP_MESG   (1)         /* 自定义类型 */
    uint8_t flag;                   /* 消息标志
                                        - 0: 系统消息(type: sdtp_sys_mesg_e)
                                        - 1: 自定义消息(type: 0x0000~0xFFFF) */
    uint32_t length;                /* 消息体长度 */
#define SDTP_CHECK_SUM  (0x1FE23DC4)
    uint32_t checksum;              /* 校验值 */
} __attribute__((packed)) sdtp_header_t;

#define SDTP_TYPE_ISVALID(head) ((head)->type < SDTP_TYPE_MAX)
#define SDTP_CHECKSUM_ISVALID(head) (SDTP_CHECK_SUM == (head)->checksum)

/* 校验数据头 */
#define SDTP_HEAD_ISVALID(head) (SDTP_CHECKSUM_ISVALID(head) && SDTP_TYPE_ISVALID(head))

/* 队列配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];   /* 队列路径 */
    int size;                       /* 单元大小 */
    int count;                      /* 队列长度 */
} sdtp_queue_conf_t;

/* 绑定CPU配置信息 */
typedef struct
{
    short ison;                     /* 是否开启绑定CPU功能 */
    short start;                    /* 绑定CPU的起始CPU编号 */
} sdtp_cpu_conf_t;

#endif /*__SDTP_PRIV_H__*/

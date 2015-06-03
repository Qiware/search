#if !defined(__GATE_COMM_H__)
#define __GATE_COMM_H__

#include "comm.h"
#include "list.h"
#include "mem_pool.h"

/* 错误码 */
typedef enum
{
    GATE_OK = 0                             /* 正常 */
    , GATE_SHOW_HELP                        /* 显示帮助信息 */
    , GATE_DONE                             /* 完成 */
    , GATE_SCK_AGAIN                        /* 出现EAGAIN提示 */
    , GATE_SCK_CLOSE                        /* 套接字关闭 */

    , GATE_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} gate_err_code_e;

/* 消息流水信息 */
typedef struct
{
    uint64_t  serial;                       /* 流水号(全局唯一编号) */

    int gate_agt_idx;                       /* 搜索代理索引 */
    int sck_serial;                         /* 套接字编号 */
} gate_flow_t;

/* 注册回调类型 */
typedef int (*gate_reg_cb_t)(unsigned int type, char *buff, size_t len, void *args, log_cycle_t *log);

/* 注册对象 */
typedef struct
{
    unsigned int type:8;                    /* 数据类型 范围:(0 ~ GATE_MSG_TYPE_MAX) */
#define GATE_REG_FLAG_UNREG     (0)         /* 0: 未注册 */
#define GATE_REG_FLAG_REGED     (1)         /* 1: 已注册 */
    unsigned int flag:8;                    /* 注册标志 范围:(0: 未注册 1: 已注册) */
    gate_reg_cb_t proc;                     /* 对应数据类型的处理函数 */
    void *args;                             /* 附加参数 */
} gate_reg_t;

/* 新增套接字对象 */
typedef struct
{
    int fd;                                 /* 套接字 */
    unsigned long long serial;              /* SCK流水号 */
} gate_add_sck_t;

/* 超时连接链表 */
typedef struct
{
    time_t ctm;                             /* 当前时间 */
    list_t *list;                           /* 超时链表 */
    mem_pool_t *pool;                       /* 内存池 */
} gate_conn_timeout_list_t;

#endif /*__GATE_COMM_H__*/

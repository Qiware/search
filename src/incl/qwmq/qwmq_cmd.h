#if !defined(__QWMQ_CMD_H__)
#define __QWMQ_CMD_H__

#include "comm.h"

/* 命令类型 */
typedef enum
{
    QWMQ_CMD_UNKNOWN                /* 未知请求 */
    , QWMQ_CMD_ADD_SCK              /* 接收客户端数据-请求 */
    , QWMQ_CMD_DIST_REQ             /* 分发任务请求 */
    , QWMQ_CMD_PROC_REQ             /* 处理客户端数据-请求 */
    , QWMQ_CMD_SEND                 /* 发送数据-请求 */
    , QWMQ_CMD_SEND_ALL             /* 发送所有数据-请求 */

    /* 查询命令 */
    , QWMQ_CMD_QUERY_CONF_REQ       /* 查询配置信息-请求 */
    , QWMQ_CMD_QUERY_CONF_REP       /* 查询配置信息-应答 */
    , QWMQ_CMD_QUERY_RECV_STAT_REQ  /* 查询接收状态-请求 */
    , QWMQ_CMD_QUERY_RECV_STAT_REP  /* 查询接收状态-应答 */
    , QWMQ_CMD_QUERY_PROC_STAT_REQ  /* 查询处理状态-请求 */
    , QWMQ_CMD_QUERY_PROC_STAT_REP  /* 查询处理状态-应答 */
        
    , QWMQ_CMD_TOTAL
} qwmq_cmd_e;

/* 添加套接字请求的相关参数 */
typedef struct
{
    int sckid;                      /* 套接字 */
    uint64_t sid;                   /* Session ID */
    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
} qwmq_cmd_add_sck_t;

/* 处理数据请求的相关参数 */
typedef struct
{
    uint32_t ori_svr_id;            /* 接收线程ID */
    uint32_t rqidx;                 /* 接收队列索引 */
    uint32_t num;                   /* 需要处理的数据条数 */
} qwmq_cmd_proc_req_t;

/* 发送数据请求的相关参数 */
typedef struct
{
    /* No member */
} qwmq_cmd_send_req_t;

/* 配置信息 */
typedef struct
{
    int nodeid;                     /* 节点ID: 不允许重复 */
    char path[FILE_NAME_MAX_LEN];   /* 工作路径 */
    int port;                       /* 侦听端口 */
    int recv_thd_num;               /* 接收线程数 */
    int work_thd_num;               /* 工作线程数 */
    int recvq_num;                  /* 接收队列数 */

    int qmax;                       /* 队列长度 */
    int qsize;                      /* 队列大小 */
} qwmq_cmd_conf_t;

/* Recv状态信息 */
typedef struct
{
    uint32_t connections;
    uint64_t recv_total;
    uint64_t drop_total;
    uint64_t err_total;
} qwmq_cmd_recv_stat_t;

/* Work状态信息 */
typedef struct
{
    uint64_t proc_total;
    uint64_t drop_total;
    uint64_t err_total;
} qwmq_cmd_proc_stat_t;

/* 各命令所附带的数据 */
typedef union
{
    qwmq_cmd_add_sck_t recv_req;
    qwmq_cmd_proc_req_t proc_req;
    qwmq_cmd_send_req_t send_req;
    qwmq_cmd_proc_stat_t proc_stat;
    qwmq_cmd_recv_stat_t recv_stat;
    qwmq_cmd_conf_t conf;
} qwmq_cmd_param_t;

/* 命令数据信息 */
typedef struct
{
    uint32_t type;                      /* 命令类型 Range: qwmq_cmd_e */
    char src_path[FILE_NAME_MAX_LEN];   /* 命令源路径 */
    qwmq_cmd_param_t param;             /* 其他数据信息 */
} qwmq_cmd_t;

#endif /*__QWMQ_CMD_H__*/

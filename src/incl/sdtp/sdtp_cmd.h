#if !defined(__SDTP_CMD_H__)
#define __SDTP_CMD_H__

#include "comm.h"

/* 命令类型 */
typedef enum
{
    SDTP_CMD_UNKNOWN                 /* 未知请求 */
    , SDTP_CMD_ADD_SCK               /* 接收客户端数据-请求 */
    , SDTP_CMD_PROC_REQ              /* 处理客户端数据-请求 */
    , SDTP_CMD_SEND                  /* 发送数据-请求 */
    , SDTP_CMD_SEND_ALL              /* 发送所有数据-请求 */

    /* 查询命令 */
    , SDTP_CMD_QUERY_CONF_REQ        /* 查询配置信息-请求 */
    , SDTP_CMD_QUERY_CONF_REP        /* 查询配置信息-应答 */
    , SDTP_CMD_QUERY_RECV_STAT_REQ   /* 查询接收状态-请求 */
    , SDTP_CMD_QUERY_RECV_STAT_REP   /* 查询接收状态-应答 */
    , SDTP_CMD_QUERY_PROC_STAT_REQ   /* 查询处理状态-请求 */
    , SDTP_CMD_QUERY_PROC_STAT_REP   /* 查询处理状态-应答 */
        
    , SDTP_CMD_TOTAL
} sdtp_cmd_e;

/* 添加套接字请求的相关参数 */
typedef struct
{
    int sckid;                      /* 套接字 */
    uint64_t sck_seq;               /* 套接字序列号 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
} sdtp_cmd_add_sck_t;

/* 处理数据请求的相关参数 */
typedef struct
{
    uint32_t ori_rsvr_tidx;         /* 接收线程编号 */
    uint32_t rqidx;                 /* 接收队列索引 */
    uint32_t num;                   /* 需要处理的数据条数 */
} sdtp_cmd_proc_req_t;

/* 发送数据请求的相关参数 */
typedef struct
{
    /* No member */
} sdtp_cmd_send_req_t;

/* 配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];   /* 服务名: 不允许重复出现 */
    int port;                       /* 侦听端口 */
    int recv_thd_num;               /* 接收线程数 */
    int work_thd_num;               /* 工作线程数 */
    int recvq_num;                  /* 接收队列数 */

    int qmax;                       /* 队列长度 */
    int qsize;                      /* 队列大小 */
} sdtp_cmd_conf_t;

/* Recv状态信息 */
typedef struct
{
    uint32_t connections;
    uint64_t recv_total;
    uint64_t drop_total;
    uint64_t err_total;
} sdtp_cmd_recv_stat_t;

/* Work状态信息 */
typedef struct
{
    uint64_t proc_total;
    uint64_t drop_total;
    uint64_t err_total;
} sdtp_cmd_proc_stat_t;

/* 各命令所附带的数据 */
typedef union
{
    sdtp_cmd_add_sck_t recv_req;
    sdtp_cmd_proc_req_t proc_req;
    sdtp_cmd_send_req_t send_req;
    sdtp_cmd_proc_stat_t proc_stat;
    sdtp_cmd_recv_stat_t recv_stat;
    sdtp_cmd_conf_t conf;
} sdtp_cmd_args_t;

/* 命令信息结构体 */
typedef struct
{
    uint32_t type;                      /* 命令类型 Range: sdtp_cmd_e */
    char src_path[FILE_NAME_MAX_LEN];   /* 命令源路径 */
    sdtp_cmd_args_t args;               /* 其他数据信息 */
} sdtp_cmd_t;

#endif /*__SDTP_CMD_H__*/

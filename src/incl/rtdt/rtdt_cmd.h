#if !defined(__RTDT_CMD_H__)
#define __RTDT_CMD_H__

#include "comm.h"

/* 命令类型 */
typedef enum
{
    RTDT_CMD_UNKNOWN                 /* 未知请求 */
    , RTDT_CMD_ADD_SCK               /* 接收客户端数据-请求 */
    , RTDT_CMD_PROC_REQ              /* 处理客户端数据-请求 */
    , RTDT_CMD_SEND                  /* 发送数据-请求 */
    , RTDT_CMD_SEND_ALL              /* 发送所有数据-请求 */

    /* 查询命令 */
    , RTDT_CMD_QUERY_CONF_REQ        /* 查询配置信息-请求 */
    , RTDT_CMD_QUERY_CONF_REP        /* 查询配置信息-应答 */
    , RTDT_CMD_QUERY_RECV_STAT_REQ   /* 查询接收状态-请求 */
    , RTDT_CMD_QUERY_RECV_STAT_REP   /* 查询接收状态-应答 */
    , RTDT_CMD_QUERY_PROC_STAT_REQ   /* 查询处理状态-请求 */
    , RTDT_CMD_QUERY_PROC_STAT_REP   /* 查询处理状态-应答 */
        
    , RTDT_CMD_TOTAL
} rtdt_cmd_e;

/* 添加套接字请求的相关参数 */
typedef struct
{
    int sckid;                      /* 套接字 */
    uint64_t sck_serial;            /* 套接字序列号 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
} rtdt_cmd_add_sck_t;

/* 处理数据请求的相关参数 */
typedef struct
{
    uint32_t ori_rsvr_tidx;         /* 接收线程编号 */
    uint32_t rqidx;                 /* 接收队列索引 */
    uint32_t num;                   /* 需要处理的数据条数 */
} rtdt_cmd_proc_req_t;

/* 发送数据请求的相关参数 */
typedef struct
{
    /* No member */
} rtdt_cmd_send_req_t;

/* 配置信息 */
typedef struct
{
    char name[FILE_NAME_MAX_LEN];   /* 服务名: 不允许重复出现 */
    int port;                       /* 侦听端口 */
    int recv_thd_num;               /* 接收线程数 */
    int work_thd_num;               /* 工作线程数 */
    int rqnum;                      /* 接收队列数 */

    int qmax;                       /* 队列长度 */
    int qsize;                      /* 队列大小 */
} rtdt_cmd_conf_t;

/* Recv状态信息 */
typedef struct
{
    uint32_t connections;
    uint64_t recv_total;
    uint64_t drop_total;
    uint64_t err_total;
} rtdt_cmd_recv_stat_t;

/* Work状态信息 */
typedef struct
{
    uint64_t proc_total;
    uint64_t drop_total;
    uint64_t err_total;
} rtdt_cmd_proc_stat_t;

/* 各命令所附带的数据 */
typedef union
{
    rtdt_cmd_add_sck_t recv_req;
    rtdt_cmd_proc_req_t proc_req;
    rtdt_cmd_send_req_t send_req;
    rtdt_cmd_proc_stat_t proc_stat;
    rtdt_cmd_recv_stat_t recv_stat;
    rtdt_cmd_conf_t conf;
} rtdt_cmd_args_t;

/* 命令信息结构体 */
typedef struct
{
    uint32_t type;                      /* 命令类型 Range: rtdt_cmd_e */
    char src_path[FILE_NAME_MAX_LEN];   /* 命令源路径 */
    rtdt_cmd_args_t args;               /* 其他数据信息 */
} rtdt_cmd_t;

#endif /*__RTDT_CMD_H__*/

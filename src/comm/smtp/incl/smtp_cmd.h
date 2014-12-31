#if !defined(__SMTP_CMD_H__)
#define __SMTP_CMD_H__

/* 命令类型 */
typedef enum
{
    SMTP_CMD_UNKNOWN                 /* 未知请求 */
    , SMTP_CMD_ADD_SCK               /* 接收客户端数据-请求 */
    , SMTP_CMD_WORK                  /* 处理客户端数据-请求 */
    , SMTP_CMD_SEND                  /* 发送数据-请求 */
    , SMTP_CMD_SEND_ALL              /* 发送所有数据-请求 */

    /* 查询命令 */
    , SMTP_CMD_QUERY_CONF_REQ        /* 查询配置信息-请求 */
    , SMTP_CMD_QUERY_CONF_REP        /* 查询配置信息-应答 */
    , SMTP_CMD_QUERY_RECV_STAT_REQ   /* 查询接收状态-请求 */
    , SMTP_CMD_QUERY_RECV_STAT_REP   /* 查询接收状态-应答 */
    , SMTP_CMD_QUERY_WORK_STAT_REQ   /* 查询处理状态-请求 */
    , SMTP_CMD_QUERY_WORK_STAT_REP   /* 查询处理状态-应答 */
        
    , SMTP_CMD_TOTAL
} smtp_cmd_e;

/* 添加套接字请求的相关参数 */
typedef struct
{
    int sckid;                      /* 套接字 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
} smtp_cmd_add_sck_t;

/* 处理数据请求的相关参数 */
typedef struct
{
    uint32_t ori_rsvr_tidx;         /* 接收线程编号 */
    uint32_t rqidx;                 /* 接收队列索引 */
    uint32_t num;                   /* 需要处理的数据条数 */
} smtp_cmd_work_t;

/* 发送数据请求的相关参数 */
typedef struct
{
    /* No member */
} smtp_cmd_send_t;

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
} smtp_cmd_conf_t;

/* Recv状态信息 */
typedef struct
{
    uint32_t connections;
    uint64_t recv_total;
    uint64_t drop_total;
    uint64_t err_total;
} smtp_cmd_recv_stat_t;

/* Work状态信息 */
typedef struct
{
    uint64_t work_total;
    uint64_t drop_total;
    uint64_t err_total;
} smtp_cmd_work_stat_t;

/* 各命令所附带的数据 */
typedef union
{
    smtp_cmd_add_sck_t recv_req;
    smtp_cmd_work_t work_req;
    smtp_cmd_send_t send_req;
    smtp_cmd_work_stat_t work_stat;
    smtp_cmd_recv_stat_t recv_stat;
    smtp_cmd_conf_t conf;
} smtp_cmd_args_t;

/* 命令信息结构体 */
typedef struct
{
    uint32_t type;                      /* 命令类型 Range: smtp_cmd_e */
    char src_path[FILE_NAME_MAX_LEN];   /* 命令源路径 */
    smtp_cmd_args_t args;               /* 其他数据信息 */
} smtp_cmd_t;

#endif /*__SMTP_CMD_H__*/

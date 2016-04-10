#if !defined(__RTMQ_MESG_H__)
#define __RTMQ_MESG_H__

#include "comm.h"

/* 宏定义 */
#define RTMQ_USR_MAX_LEN    (32)        /* 用户名 */
#define RTMQ_PWD_MAX_LEN    (16)        /* 登录密码 */

/* 系统数据类型 */
typedef enum
{
    RTMQ_CMD_UNKNOWN                    /* 未知命令 */

    , RTMQ_CMD_LINK_AUTH_REQ            /* 链路鉴权请求 */
    , RTMQ_CMD_LINK_AUTH_RSP            /* 链路鉴权应答 */

    , RTMQ_CMD_KPALIVE_REQ              /* 链路保活请求 */
    , RTMQ_CMD_KPALIVE_RSP              /* 链路保活应答 */

    , RTMQ_CMD_ADD_SCK                  /* 接收客户端数据-请求 */
    , RTMQ_CMD_DIST_REQ                 /* 分发任务请求 */
    , RTMQ_CMD_PROC_REQ                 /* 处理客户端数据-请求 */
    , RTMQ_CMD_SEND                     /* 发送数据-请求 */
    , RTMQ_CMD_SEND_ALL                 /* 发送所有数据-请求 */

    /* 查询命令 */
    , RTMQ_CMD_QUERY_CONF_REQ           /* 查询配置信息-请求 */
    , RTMQ_CMD_QUERY_CONF_REP           /* 查询配置信息-应答 */
    , RTMQ_CMD_QUERY_RECV_STAT_REQ      /* 查询接收状态-请求 */
    , RTMQ_CMD_QUERY_RECV_STAT_REP      /* 查询接收状态-应答 */
    , RTMQ_CMD_QUERY_PROC_STAT_REQ      /* 查询处理状态-请求 */
    , RTMQ_CMD_QUERY_PROC_STAT_REP      /* 查询处理状态-应答 */
} rtmq_mesg_e;

/* 报头结构 */
typedef struct
{
    int type;                           /* 命令类型 */
    int nodeid;                         /* 源结点ID */
#define RTMQ_SYS_MESG   (0)             /* 系统类型 */
#define RTMQ_EXP_MESG   (1)             /* 自定义类型 */
    uint8_t flag;                       /* 消息标志
                                            - 0: 系统消息(type: rtmq_mesg_e)
                                            - 1: 自定义消息(type: 0x0000~0xFFFF) */
    uint32_t length;                    /* 消息体长度 */
#define RTMQ_CHECK_SUM  (0x1FE23DC4)    
    uint32_t checksum;                  /* 校验值 */
} __attribute__((packed)) rtmq_header_t;

#define RTMQ_DATA_TOTAL_LEN(head) (head->length + sizeof(rtmq_header_t))
#define RTMQ_CHECKSUM_ISVALID(head) (RTMQ_CHECK_SUM == (head)->checksum)

/* 校验数据头 */
#define RTMQ_HEAD_ISVALID(head) (RTMQ_CHECKSUM_ISVALID(head))

/* 转发信息 */
typedef struct
{
    int type;                           /* 消息类型 */
    int dest;                           /* 目标结点ID */
    int length;                         /* 报体长度 */
} rtmq_frwd_t;

/* 链路鉴权请求 */
typedef struct
{
    int nodeid;                         /* 结点ID */
    char usr[RTMQ_USR_MAX_LEN];         /* 用户名 */
    char passwd[RTMQ_PWD_MAX_LEN];      /* 登录密码 */
} rtmq_link_auth_req_t;

/* 链路鉴权应答 */
typedef struct
{
    int nodeid;                         /* 结点ID */
#define RTMQ_LINK_AUTH_FAIL     (0)
#define RTMQ_LINK_AUTH_SUCC     (1)
    int is_succ;                        /* 应答码(0:失败 1:成功) */
} rtmq_link_auth_rsp_t;

/* 添加套接字请求的相关参数 */
typedef struct
{
    int sckid;                      /* 套接字 */
    uint64_t sid;                   /* Session ID */
    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
} rtmq_cmd_add_sck_t;

/* 处理数据请求的相关参数 */
typedef struct
{
    uint32_t ori_svr_id;            /* 接收线程ID */
    uint32_t rqidx;                 /* 接收队列索引 */
    uint32_t num;                   /* 需要处理的数据条数 */
} rtmq_cmd_proc_req_t;

/* 发送数据请求的相关参数 */
typedef struct
{
    /* No member */
} rtmq_cmd_send_req_t;

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
} rtmq_cmd_conf_t;

/* Recv状态信息 */
typedef struct
{
    uint32_t connections;
    uint64_t recv_total;
    uint64_t drop_total;
    uint64_t err_total;
} rtmq_cmd_recv_stat_t;

/* Work状态信息 */
typedef struct
{
    uint64_t proc_total;
    uint64_t drop_total;
    uint64_t err_total;
} rtmq_cmd_proc_stat_t;

/* 各命令所附带的数据 */
typedef union
{
    rtmq_cmd_add_sck_t recv_req;
    rtmq_cmd_proc_req_t proc_req;
    rtmq_cmd_send_req_t send_req;
    rtmq_cmd_proc_stat_t proc_stat;
    rtmq_cmd_recv_stat_t recv_stat;
    rtmq_cmd_conf_t conf;
} rtmq_cmd_param_t;

/* 命令数据信息 */
typedef struct
{
    uint32_t type;                      /* 命令类型 Range: rtmq_cmd_e */
    char src_path[FILE_NAME_MAX_LEN];   /* 命令源路径 */
    rtmq_cmd_param_t param;             /* 其他数据信息 */
} rtmq_cmd_t;



#endif /*__RTMQ_MESG_H__*/

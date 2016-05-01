#if !defined(__SDTP_MESG_H__)
#define __SDTP_MESG_H__

#include "comm.h"

/* 宏定义 */
#define SDTP_USR_MAX_LEN    (32)        /* 用户名 */
#define SDTP_PWD_MAX_LEN    (16)        /* 登录密码 */

/* 系统数据类型 */
typedef enum
{
    SDTP_CMD_UNKNOWN                    /* 未知数据类型 */

    , SDTP_CMD_LINK_AUTH_REQ            /* 链路鉴权请求 */
    , SDTP_CMD_LINK_AUTH_REP            /* 链路鉴权应答 */

    , SDTP_CMD_KPALIVE_REQ              /* 链路保活请求 */
    , SDTP_CMD_KPALIVE_REP              /* 链路保活应答 */

    , SDTP_CMD_ADD_SCK                  /* 接收客户端数据-请求 */
    , SDTP_CMD_PROC_REQ                 /* 处理客户端数据-请求 */
    , SDTP_CMD_SEND                     /* 发送数据-请求 */
    , SDTP_CMD_SEND_ALL                 /* 发送所有数据-请求 */

    /* 查询命令 */
    , SDTP_CMD_QUERY_CONF_REQ           /* 查询配置信息-请求 */
    , SDTP_CMD_QUERY_CONF_REP           /* 查询配置信息-应答 */
    , SDTP_CMD_QUERY_RECV_STAT_REQ      /* 查询接收状态-请求 */
    , SDTP_CMD_QUERY_RECV_STAT_REP      /* 查询接收状态-应答 */
    , SDTP_CMD_QUERY_PROC_STAT_REQ      /* 查询处理状态-请求 */
    , SDTP_CMD_QUERY_PROC_STAT_REP      /* 查询处理状态-应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , SDTP_DATA_TYPE_TOTAL
} sdtp_sys_mesg_e;

/* 报头结构 */
typedef struct
{
    uint16_t type;                      /* 数据类型 */
    int nid;                            /* 源结点ID */
#define SDTP_SYS_MESG   (0)             /* 系统类型 */
#define SDTP_EXP_MESG   (1)             /* 自定义类型 */
    uint8_t flag;                       /* 消息标志
                                            - 0: 系统消息(type: sdtp_sys_mesg_e)
                                            - 1: 自定义消息(type: 0x0000~0xFFFF) */
    uint32_t length;                    /* 消息体长度 */
#define SDTP_CHKSUM_VAL  (0x1FE23DC4)    
    uint32_t chksum;                    /* 校验值 */
} __attribute__((packed)) sdtp_header_t;

#define SDTP_TYPE_ISVALID(head) ((head)->type < SDTP_TYPE_MAX)
#define SDTP_DATA_TOTAL_LEN(head) (head->length + sizeof(sdtp_header_t))
#define SDTP_CHKSUM_ISVALID(head) (SDTP_CHKSUM_VAL == (head)->chksum)

/* 校验数据头 */
#define SDTP_HEAD_ISVALID(head) (SDTP_CHKSUM_ISVALID(head) && SDTP_TYPE_ISVALID(head))

/* 分组信息 */
typedef struct
{
    int num;                            /* 组中条数 */
} sdtp_group_t;

/* 转发信息 */
typedef struct
{
    int type;                           /* 消息类型 */
    int dest;                           /* 目标结点ID */
    int length;                         /* 报体长度 */
} sdtp_frwd_t;

/* 链路鉴权请求 */
typedef struct
{
    int nid;                            /* 结点ID */
    char usr[SDTP_USR_MAX_LEN];         /* 用户名 */
    char passwd[SDTP_PWD_MAX_LEN];      /* 登录密码 */
} sdtp_link_auth_req_t;

/* 链路鉴权应答 */
typedef struct
{
    int nid;                            /* 结点ID */
#define SDTP_LINK_AUTH_FAIL     (0)
#define SDTP_LINK_AUTH_SUCC     (1)
    int is_succ;                        /* 应答码(0:失败 1:成功) */
} sdtp_link_auth_rsp_t;

/* 添加套接字请求的相关参数 */
typedef struct
{
    int sckid;                      /* 套接字 */
    uint64_t sid;                   /* Session ID */
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

#endif /*__SDTP_MESG_H__*/

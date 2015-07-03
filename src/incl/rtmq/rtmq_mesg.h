#if !defined(__RTMQ_MESG_H__)
#define __RTMQ_MESG_H__

/* 宏定义 */
#define RTMQ_USR_MAX_LEN    (32)        /* 用户名 */
#define RTMQ_PWD_MAX_LEN    (16)        /* 登录密码 */

/* 系统数据类型 */
typedef enum
{
    RTMQ_UNKNOWN_MESG                   /* 未知数据类型 */

    , RTMQ_LINK_AUTH_REQ                /* 链路鉴权请求 */
    , RTMQ_LINK_AUTH_RSP                /* 链路鉴权应答 */

    , RTMQ_KPALIVE_REQ                  /* 链路保活请求 */
    , RTMQ_KPALIVE_RSP                  /* 链路保活应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , RTMQ_DATA_TYPE_TOTAL
} rtmq_sys_mesg_e;

/* 报头结构 */
typedef struct
{
    unsigned short type;                /* 数据类型 */
    int nodeid;                          /* 源结点ID */
#define RTMQ_SYS_MESG   (0)             /* 系统类型 */
#define RTMQ_EXP_MESG   (1)             /* 自定义类型 */
    uint8_t flag;                       /* 消息标志
                                            - 0: 系统消息(type: rtmq_sys_mesg_e)
                                            - 1: 自定义消息(type: 0x0000~0xFFFF) */
    unsigned int length;                /* 消息体长度 */
#define RTMQ_CHECK_SUM  (0x1FE23DC4)    
    unsigned int checksum;              /* 校验值 */
} __attribute__((packed)) rtmq_header_t;

#define RTMQ_TYPE_ISVALID(head) ((head)->type < RTMQ_TYPE_MAX)
#define RTMQ_DATA_TOTAL_LEN(head) (head->length + sizeof(rtmq_header_t))
#define RTMQ_CHECKSUM_ISVALID(head) (RTMQ_CHECK_SUM == (head)->checksum)

/* 校验数据头 */
#define RTMQ_HEAD_ISVALID(head) (RTMQ_CHECKSUM_ISVALID(head) && RTMQ_TYPE_ISVALID(head))

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

#endif /*__RTMQ_MESG_H__*/

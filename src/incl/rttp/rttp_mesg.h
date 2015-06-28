#if !defined(__RTTP_MESG_H__)
#define __RTTP_MESG_H__

/* 宏定义 */
#define RTTP_USR_MAX_LEN    (32)        /* 用户名 */
#define RTTP_PWD_MAX_LEN    (16)        /* 登录密码 */

/* 系统数据类型 */
typedef enum
{
    RTTP_UNKNOWN_MESG                   /* 未知数据类型 */

    , RTTP_LINK_AUTH_REQ                /* 链路鉴权请求 */
    , RTTP_LINK_AUTH_RSP                /* 链路鉴权应答 */

    , RTTP_KPALIVE_REQ                  /* 链路保活请求 */
    , RTTP_KPALIVE_RSP                  /* 链路保活应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , RTTP_DATA_TYPE_TOTAL
} rttp_sys_mesg_e;

/* 报头结构 */
typedef struct
{
    unsigned short type;                /* 数据类型 */
    int nodeid;                          /* 源结点ID */
#define RTTP_SYS_MESG   (0)             /* 系统类型 */
#define RTTP_EXP_MESG   (1)             /* 自定义类型 */
    uint8_t flag;                       /* 消息标志
                                            - 0: 系统消息(type: rttp_sys_mesg_e)
                                            - 1: 自定义消息(type: 0x0000~0xFFFF) */
    unsigned int length;                /* 消息体长度 */
#define RTTP_CHECK_SUM  (0x1FE23DC4)    
    unsigned int checksum;              /* 校验值 */
} __attribute__((packed)) rttp_header_t;

#define RTTP_TYPE_ISVALID(head) ((head)->type < RTTP_TYPE_MAX)
#define RTTP_DATA_TOTAL_LEN(head) (head->length + sizeof(rttp_header_t))
#define RTTP_CHECKSUM_ISVALID(head) (RTTP_CHECK_SUM == (head)->checksum)

/* 校验数据头 */
#define RTTP_HEAD_ISVALID(head) (RTTP_CHECKSUM_ISVALID(head) && RTTP_TYPE_ISVALID(head))

/* 转发信息 */
typedef struct
{
    int type;                           /* 消息类型 */
    int dest;                           /* 目标结点ID */
    int length;                         /* 报体长度 */
} rttp_frwd_t;

/* 链路鉴权请求 */
typedef struct
{
    int nodeid;                         /* 结点ID */
    char usr[RTTP_USR_MAX_LEN];         /* 用户名 */
    char passwd[RTTP_PWD_MAX_LEN];      /* 登录密码 */
} rttp_link_auth_req_t;

/* 链路鉴权应答 */
typedef struct
{
    int nodeid;                         /* 结点ID */
#define RTTP_LINK_AUTH_FAIL     (0)
#define RTTP_LINK_AUTH_SUCC     (1)
    int is_succ;                        /* 应答码(0:失败 1:成功) */
} rttp_link_auth_rsp_t;

#endif /*__RTTP_MESG_H__*/

#if !defined(__RTDT_MESG_H__)
#define __RTDT_MESG_H__

/* 宏定义 */
#define RTDT_USR_MAX_LEN    (32)        /* 用户名 */
#define RTDT_PWD_MAX_LEN    (16)        /* 登录密码 */

/* 系统数据类型 */
typedef enum
{
    RTDT_UNKNOWN_MESG                   /* 未知数据类型 */

    , RTDT_LINK_AUTH_REQ                /* 链路鉴权请求 */
    , RTDT_LINK_AUTH_REP                /* 链路鉴权应答 */

    , RTDT_KPALIVE_REQ                  /* 链路保活请求 */
    , RTDT_KPALIVE_REP                  /* 链路保活应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , RTDT_DATA_TYPE_TOTAL
} rtdt_sys_mesg_e;

/* 报头结构 */
typedef struct
{
    unsigned short type;                /* 数据类型 */
    int devid;                          /* 源设备ID */
#define RTDT_SYS_MESG   (0)             /* 系统类型 */
#define RTDT_EXP_MESG   (1)             /* 自定义类型 */
    uint8_t flag;                       /* 消息标志
                                            - 0: 系统消息(type: rtdt_sys_mesg_e)
                                            - 1: 自定义消息(type: 0x0000~0xFFFF) */
    unsigned int length;                /* 消息体长度 */
#define RTDT_CHECK_SUM  (0x1FE23DC4)    
    unsigned int checksum;              /* 校验值 */
} __attribute__((packed)) rtdt_header_t;

#define RTDT_TYPE_ISVALID(head) ((head)->type < RTDT_TYPE_MAX)
#define RTDT_DATA_TOTAL_LEN(head) (head->length + sizeof(rtdt_header_t))
#define RTDT_CHECKSUM_ISVALID(head) (RTDT_CHECK_SUM == (head)->checksum)

/* 校验数据头 */
#define RTDT_HEAD_ISVALID(head) (RTDT_CHECKSUM_ISVALID(head) && RTDT_TYPE_ISVALID(head))

/* 转发信息 */
typedef struct
{
    int type;                           /* 消息类型 */
    int dest_devid;                     /* 目标设备ID */
    int length;                         /* 报体长度 */
} rtdt_frwd_t;

/* 链路鉴权请求 */
typedef struct
{
    int devid;                          /* 设备ID */
    char usr[RTDT_USR_MAX_LEN];         /* 用户名 */
    char passwd[RTDT_PWD_MAX_LEN];      /* 登录密码 */
} rtdt_link_auth_req_t;

/* 链路鉴权应答 */
typedef struct
{
    int devid;                          /* 设备ID */
#define RTDT_LINK_AUTH_FAIL     (0)
#define RTDT_LINK_AUTH_SUCC     (1)
    int is_succ;                        /* 应答码(0:失败 1:成功) */
} rtdt_link_auth_rep_t;

#endif /*__RTDT_MESG_H__*/

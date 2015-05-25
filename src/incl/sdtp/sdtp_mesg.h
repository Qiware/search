#if !defined(__SDTP_MESG_H__)
#define __SDTP_MESG_H__

/* 宏定义 */
#define SDTP_USR_MAX_LEN    (32)        /* 用户名 */
#define SDTP_PWD_MAX_LEN    (16)        /* 登录密码 */

/* 系统数据类型 */
typedef enum
{
    SDTP_UNKNOWN_MESG                   /* 未知数据类型 */

    , SDTP_LINK_AUTH_REQ                /* 链路鉴权请求 */
    , SDTP_LINK_AUTH_REP                /* 链路鉴权应答 */

    , SDTP_KPALIVE_REQ                  /* 链路保活请求 */
    , SDTP_KPALIVE_REP                  /* 链路保活应答 */

    /*******************在此线以上添加系统数据类型****************************/
    , SDTP_DATA_TYPE_TOTAL
} sdtp_sys_mesg_e;

/* 报头结构 */
typedef struct
{
    unsigned short type;                /* 数据类型 */
#define SDTP_SYS_MESG   (0)             /* 系统类型 */
#define SDTP_EXP_MESG   (1)             /* 自定义类型 */
    uint8_t flag;                       /* 消息标志
                                            - 0: 系统消息(type: sdtp_sys_mesg_e)
                                            - 1: 自定义消息(type: 0x0000~0xFFFF) */
    unsigned int length;                /* 消息体长度 */
#define SDTP_CHECK_SUM  (0x1FE23DC4)    
    unsigned int checksum;              /* 校验值 */
} __attribute__((packed)) sdtp_header_t;

#define SDTP_TYPE_ISVALID(head) ((head)->type < SDTP_TYPE_MAX)
#define SDTP_DATA_TOTAL_LEN(head) (head->length + sizeof(sdtp_header_t))
#define SDTP_CHECKSUM_ISVALID(head) (SDTP_CHECK_SUM == (head)->checksum)

/* 校验数据头 */
#define SDTP_HEAD_ISVALID(head) (SDTP_CHECKSUM_ISVALID(head) && SDTP_TYPE_ISVALID(head))

/* 链路鉴权请求 */
typedef struct
{
    int devid;                          /* 设备ID */
    char usr[SDTP_USR_MAX_LEN];         /* 用户名 */
    char passwd[SDTP_PWD_MAX_LEN];      /* 登录密码 */
} sdtp_link_auth_req_t;

/* 链路鉴权应答 */
typedef struct
{
    int devid;                          /* 设备ID */
#define SDTP_LINK_AUTH_FAIL     (0)
#define SDTP_LINK_AUTH_SUCC     (1)
    int is_succ;                        /* 应答码(0:失败 1:成功) */
} sdtp_link_auth_rep_t;

#endif /*__SDTP_MESG_H__*/

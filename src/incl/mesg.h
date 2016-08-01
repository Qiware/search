/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: mesg.h
 ** 版本号: 1.0
 ** 描  述: 消息类型的定义
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 10:43:30 PM CST #
 ******************************************************************************/
#if !defined(__MESG_H__)
#define __MESG_H__

#include "uri.h"

/* 消息类型 */
typedef enum
{
    MSG_TYPE_UNKNOWN                    /* 未知消息 */

    , MSG_PING                          /* PING-请求 */
    , MSG_PONG                          /* PONG-应答 */

    , MSG_SEARCH_REQ                    /* 搜索关键字-请求 */
    , MSG_SEARCH_RSP                    /* 搜索关键字-应答 */

    , MSG_INSERT_WORD_REQ               /* 插入关键字-请求 */
    , MSG_INSERT_WORD_RSP               /* 插入关键字-应答 */

    , MSG_PRINT_INVT_TAB_REQ            /* 打印倒排表-请求 */
    , MSG_PRINT_INVT_TAB_RSP            /* 打印倒排表-应答 */

    , MSG_QUERY_CONF_REQ                /* 查询配置信息-请求 */
    , MSG_QUERY_CONF_RSP                /* 反馈配置信息-应答 */

    , MSG_QUERY_WORKER_STAT_REQ         /* 查询工作信息-请求 */
    , MSG_QUERY_WORKER_STAT_RSP         /* 反馈工作信息-应答 */

    , MSG_QUERY_WORKQ_STAT_REQ          /* 查询工作队列信息-请求 */
    , MSG_QUERY_WORKQ_STAT_RSP          /* 反馈工作队列信息-应答 */

    , MSG_SWITCH_SCHED_REQ              /* 切换调度-请求 */
    , MSG_SWITCH_SCHED_RSP              /* 反馈切换调度信息-应答 */

    , MSG_SUB_REQ                       /* 订阅-请求 */
    , MSG_SUB_RSP                       /* 订阅-应答 */

    , MSG_TYPE_TOTAL                    /* 消息类型总数 */
} mesg_type_e;

/* 通用协议头 */
typedef struct
{
    uint32_t type;                      /* 消息类型 Range: mesg_type_e */
#define MSG_FLAG_SYS   (0)              /* 0: 系统数据类型 */
#define MSG_FLAG_USR   (1)              /* 1: 自定义数据类型 */
    uint32_t flag;                      /* 标识量(0:系统数据类型 1:自定义数据类型) */
    uint32_t length;                    /* 报体长度 */
#define MSG_CHKSUM_VAL   (0x1ED23CB4)
    uint32_t chksum;                    /* 校验值 */

    uint64_t sid;                       /* 会话ID */
    uint32_t nid;                       /* 结点ID */

    uint64_t serial;                    /* 流水号(注: 全局唯一流水号) */
    char body[0];                       /* 消息体 */
} mesg_header_t;

/* 字节序转换 */
#define MESG_HEAD_HTON(h, n) do { /* 主机->网络 */\
    (n)->type = htonl((h)->type); \
    (n)->flag = htonl((h)->flag); \
    (n)->length = htonl((h)->length); \
    (n)->chksum = htonl((h)->chksum); \
    (n)->sid = hton64((h)->sid); \
    (n)->nid = htonl((h)->nid); \
    (n)->serial = hton64((h)->serial); \
} while(0)

#define MESG_HEAD_NTOH(n, h) do { /* 网络->主机*/\
    (h)->type = ntohl((n)->type); \
    (h)->flag = ntohl((n)->flag); \
    (h)->length = ntohl((n)->length); \
    (h)->chksum = ntohl((n)->chksum); \
    (h)->sid = ntoh64((n)->sid); \
    (h)->nid = ntohl((n)->nid); \
    (h)->serial = ntoh64((n)->serial); \
} while(0)

#define MESG_HEAD_SET(head, _type, _sid, _nid, _serial, _len) do { /* 设置协议头 */\
    (head)->type = (_type); \
    (head)->flag = MSG_FLAG_USR; \
    (head)->length = (_len); \
    (head)->chksum = MSG_CHKSUM_VAL; \
    (head)->sid = (_sid); \
    (head)->nid = (_nid); \
    (head)->serial = (_serial); \
} while(0)

#define MESG_TOTAL_LEN(body_len) (sizeof(mesg_header_t) + body_len)
#define MESG_CHKSUM_ISVALID(head) (MSG_CHKSUM_VAL == (head)->chksum)

#define MESG_HEAD_PRINT(log, head) \
    log_debug((log), "Call %s()! type:%d len:%d chksum:0x%X/0x%X sid:%lu nid:%u serial:%lu", \
            __func__, (head)->type, (head)->length, (head)->chksum, MSG_CHKSUM_VAL, \
            (head)->sid, (head)->nid, (head)->serial);

////////////////////////////////////////////////////////////////////////////////
/* 搜索消息结构 */
#define SRCH_WORD_LEN       (128)
typedef struct
{
    char words[SRCH_WORD_LEN];          /* 搜索关键字 */
} mesg_search_req_t;

////////////////////////////////////////////////////////////////////////////////
/* 插入关键字-请求 */
typedef struct
{
    char word[SRCH_WORD_LEN];           /* 关键字 */
    char url[URL_MAX_LEN];              /* 关键字对应的URL */
    int freq;                           /* 频率 */
} mesg_insert_word_req_t;

/* 插入关键字-应答 */
typedef struct
{
#define MESG_INSERT_WORD_FAIL   (0)
#define MESG_INSERT_WORD_SUCC   (1)
    int code;                           /* 应答码 */
    char word[SRCH_WORD_LEN];           /* 关键字 */
} mesg_insert_word_rsp_t;

#define mesg_insert_word_resp_hton(rsp) do { \
    (rsp)->code = htonl((rsp)->code); \
} while(0)

#define mesg_insert_word_resp_ntoh(rsp) do { \
    (rsp)->code = ntohl((rsp)->code); \
} while(0)

////////////////////////////////////////////////////////////////////////////////
/* 订阅-请求 */
typedef struct
{
    mesg_type_e type;                   /* 订阅类型(消息类型) */
    int nid;                            /* 结点ID */
} mesg_sub_req_t;

#define mesg_sub_req_hton(req) do { /* 主机 -> 网络 */\
    (req)->type = htonl((req)->type); \
    (req)->nid = htonl((req)->nid); \
} while(0)

#define mesg_sub_req_ntoh(req) do { /* 网络 -> 主机 */\
    (req)->type = ntohl((req)->type); \
    (req)->nid = ntohl((req)->nid); \
} while(0)

/* 订阅-应答 */
typedef struct
{
    int code;                           /* 应答码 */
    mesg_type_e type;                   /* 订阅类型(消息类型) */
} mesg_sub_rsp_t;

#define mesg_sub_rsp_hton(rsp) do { /* 主机 -> 网络 */\
    (rsp)->code = htonl((rsp)->code); \
    (rsp)->type = htonl((rsp)->type); \
} while(0)

#define mesg_sub_rsp_ntoh(rsp) do { /* 网络 -> 主机 */\
    (rsp)->code = ntohl((rsp)->code); \
    (rsp)->type = ntohl((rsp)->type); \
} while(0)

#endif /*__MESG_H__*/

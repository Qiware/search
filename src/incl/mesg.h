/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mesg.h
 ** 版本号: 1.0
 ** 描  述: 消息类型的定义
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 10:43:30 PM CST #
 ******************************************************************************/
#if !defined(__MESG_H__)
#define __MESG_H__

/* 消息类型 */
typedef enum
{
    MSG_TYPE_UNKNOWN                        /* 未知消息 */

    , MSG_SEARCH_REQ                        /* 搜索请求 */
    , MSG_SEARCH_REP                        /* 搜索应答 */

    , MSG_PRINT_INVT_TAB_REQ                /* 打印倒排表的请求 */

    , MSG_QUERY_CONF_REQ                    /* 查询配置信息 */
    , MSG_QUERY_CONF_RESP                   /* 反馈配置信息 */

    , MSG_QUERY_WORKER_STAT_REQ             /* 查询工作信息 */
    , MSG_QUERY_WORKER_STAT_RESP            /* 反馈工作信息 */

    , MSG_QUERY_WORKQ_STAT_REQ              /* 查询工作队列信息 */
    , MSG_QUERY_WORKQ_STAT_RESP             /* 反馈工作队列信息 */

    , MSG_SWITCH_SCHED_REQ                  /* 切换调度 */
    , MSG_SWITCH_SCHED_RESP                 /* 反馈切换调度信息 */

    , MSG_TYPE_TOTAL                        /* 消息类型总数 */
} mesg_type_e;

/* 消息路由信息
 * 各设备必须记录处理过的流水, 以便在收到应答时,
 * 能够正确的将应答数据发送给在此之前的设备 */
typedef struct
{
    unsigned long long serial;              /* 流水号(全局唯一编号) */
    int orig;                               /* 源设备ID */
    int dest;                               /* 目的设备ID */
} mesg_route_t;

/* 搜索关键字 */
typedef struct
{
#define SRCH_WORDS_LEN      (128)
    char words[SRCH_WORDS_LEN];             /* 搜索关键字 */
} srch_mesg_body_t;

#endif /*__MESG_H__*/

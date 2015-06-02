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

#include "uri.h"

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

/* 搜索关键字(外部使用) */
typedef struct
{
#define SRCH_WORDS_LEN      (128)
    char words[SRCH_WORDS_LEN];             /* 搜索关键字 */
} srch_mesg_body_t;

/* 搜索消息结构(内部使用) */
typedef struct
{
    uint64_t serial;                        /* 流水号(全局唯一编号) */ 
    srch_mesg_body_t body;                  /* 报体信息 */
} mesg_search_req_t;

/* 搜索应答信息(内部使用) */
typedef struct
{
    uint64_t serial;                        /* 流水号(全局唯一编号) */
#define MSG_SRCH_REP_URL_NUM    (5)
    char url[MSG_SRCH_REP_URL_NUM][URL_MAX_LEN]; /* 网址: 查找结果 */
} mesg_search_rep_t;

#endif /*__MESG_H__*/

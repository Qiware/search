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
    MSG_TYPE_UNKNOWN                        /* 未知消息 */

    , MSG_SEARCH_WORD_REQ                   /* 搜索关键字-请求 */
    , MSG_SEARCH_WORD_RSP                   /* 搜索关键字-应答 */

    , MSG_INSERT_WORD_REQ                   /* 插入关键字-请求 */
    , MSG_INSERT_WORD_RSP                   /* 插入关键字-应答 */

    , MSG_PRINT_INVT_TAB_REQ                /* 打印倒排表-请求 */
    , MSG_PRINT_INVT_TAB_RSP                /* 打印倒排表-应答 */

    , MSG_QUERY_CONF_REQ                    /* 查询配置信息-请求 */
    , MSG_QUERY_CONF_RSP                    /* 反馈配置信息-应答 */

    , MSG_QUERY_WORKER_STAT_REQ             /* 查询工作信息-请求 */
    , MSG_QUERY_WORKER_STAT_RSP             /* 反馈工作信息-应答 */

    , MSG_QUERY_WORKQ_STAT_REQ              /* 查询工作队列信息-请求 */
    , MSG_QUERY_WORKQ_STAT_RSP              /* 反馈工作队列信息-应答 */

    , MSG_SWITCH_SCHED_REQ                  /* 切换调度-请求 */
    , MSG_SWITCH_SCHED_RSP                  /* 反馈切换调度信息-应答 */

    , MSG_TYPE_TOTAL                        /* 消息类型总数 */
} mesg_type_e;

/* 搜索消息结构 */
#define SRCH_WORD_LEN       (128)
typedef struct
{
    uint64_t serial;                        /* 流水号(全局唯一编号) */ 
    char words[SRCH_WORD_LEN];              /* 搜索关键字 */
} mesg_search_word_req_t;

/* 搜索应答信息(内部使用) */
typedef struct
{
    uint64_t serial;                        /* 流水号(全局唯一编号) */
    char body[0];                           /* 应答数据 */
} mesg_search_word_rsp_t;

/* 插入关键字-请求 */
typedef struct
{
    uint64_t serial;                        /* 流水号(全局唯一编号) */

    char word[SRCH_WORD_LEN];               /* 关键字 */
    char url[URL_MAX_LEN];                  /* 关键字对应的URL */
    int freq;                               /* 频率 */
} mesg_insert_word_req_t;

/* 插入关键字-应答 */
typedef struct
{
    uint64_t serial;                        /* 流水号(全局唯一编号) */

#define MESG_INSERT_WORD_FAIL   (0)
#define MESG_INSERT_WORD_SUCC   (1)
    int code;                               /* 应答码 */
    char word[SRCH_WORD_LEN];               /* 关键字 */
} mesg_insert_word_rsp_t;

#endif /*__MESG_H__*/

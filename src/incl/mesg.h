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
    MSG_TYPE_UNKNOWN                       /* 未知消息 */

    , MSG_TYPE_SEARCH_REQ                  /* 搜索请求 */
    , MSG_TYPE_SEARCH_REP                  /* 搜索应答 */

    , MSG_TYPE_PRINT_INVT_TAB_REQ          /* 打印倒排表的请求 */

    , MSG_TYPE_TOTAL                       /* 消息类型总数 */
} mesg_type_e;

/* 报体 */
typedef struct
{
#define SRCH_WORDS_LEN      (128)
    char words[SRCH_WORDS_LEN];             /* 搜索关键字 */
} srch_mesg_body_t;

#endif /*__MESG_H__*/

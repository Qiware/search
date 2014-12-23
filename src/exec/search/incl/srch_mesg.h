#if !defined(__SRCH_MESG_H__)
#define __SRCH_MESG_H__

#include <stdint.h>

/* 消息类型 */
typedef enum
{
    SRCH_MSG_UNKNOWN                        /* 未知消息 */
    , SRCH_MSG_SRCH_REQ                     /* 搜索请求 */
    , SRCH_MSG_SRCH_REP                     /* 搜索应答 */

    , SRCH_MSG_TOTAL                        /* 消息类型总数 */
} srch_mesg_type_e;

/* 报头 */
typedef struct
{
    uint8_t type;                           /* 消息类型 Range: srch_mesg_type_e */
#define SRCH_MSG_FLAG_SYS   (0)             /* 0: 系统数据类型 */
#define SRCH_MSG_FLAG_USR   (1)             /* 1: 自定义数据类型 */
    uint8_t flag;                           /* 标识量(0:系统数据类型 1:自定义数据类型) */
    uint16_t length;                        /* 报体长度 */
#define SRCH_MSG_MARK_KEY   (0x1ED23CB4)
    uint32_t mark;                          /* 校验值 */
} srch_mesg_header_t;

/* 报体 */
typedef struct
{
#define SRCH_WORDS_LEN      (128)
    char words[SRCH_WORDS_LEN];             /* 搜索关键字 */
} srch_mesg_body_t;

#endif /*__SRCH_MESG_H__*/

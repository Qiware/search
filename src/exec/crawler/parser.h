#if !defined(__PARSER_H__)
#define __PARSER_H__

#include "log.h"
#include "gumbo_ex.h"
#include <hiredis/hiredis.h>

#define PARSER_LOG2_LEVEL  "error"  /* 日志级别 */

/* 网页基本信息 */
typedef struct
{
    char finfo[FILE_NAME_MAX_LEN];  /* 网页信息文件 */

    char uri[URI_MAX_LEN];          /* URI */
    int deep;                       /* 网页深度 */
    char ipaddr[IP_ADDR_MAX_LEN];   /* IP地址 */
    int port;                       /* 端口号 */
    char html[FILE_NAME_MAX_LEN];   /* 网页存储名 */
} parser_webpage_info_t;

/* 解析器对象 */
typedef struct
{
    log_cycle_t *log;               /* 日志对象 */
    redisContext *redis_ctx;        /* REDIS对象 */
    gumbo_cntx_t gumbo_ctx;         /* GUMBO对象 */

    gumbo_html_t *html;             /* HTML对象 */
    gumbo_result_t *result;         /* 结果集合 */

    parser_webpage_info_t info;     /* 网页信息 */
} parser_t;

#endif /*__PARSER_H__*/

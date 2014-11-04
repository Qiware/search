#if !defined(__CRWL_PARSER_H__)
#define __CRWL_PARSER_H__

#include "log.h"
#include "redis.h"
#include "crawler.h"
#include "gumbo_ex.h"
#include <hiredis/hiredis.h>

/* 网页基本信息 */
typedef struct
{
    char fname[FILE_NAME_MAX_LEN];  /* 网页信息文件 */

    char uri[URI_MAX_LEN];          /* URI */
    uint32_t depth;                 /* 网页深度 */
    char ip[IP_ADDR_MAX_LEN];       /* IP地址 */
    int port;                       /* 端口号 */
    char html[FILE_NAME_MAX_LEN];   /* 网页存储名 */
} crwl_webpage_info_t;

/* 解析器对象 */
typedef struct
{
    crwl_conf_t conf;               /* 配置信息 */

    log_cycle_t *log;               /* 日志对象 */
    redis_ctx_t *redis;             /* Redis上下文 */
    gumbo_cntx_t gumbo_ctx;         /* GUMBO对象 */

    crwl_webpage_info_t info;       /* 网页信息 */
} crwl_parser_t;

int crwl_parser_exec(const crwl_conf_t *conf);

#endif /*__CRWL_PARSER_H__*/

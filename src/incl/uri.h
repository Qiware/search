#if !defined(__URI_H__)
#define __URI_H__

#include "comm.h"

#define URL_MIN_LEN         (3)             /* URL最小长度 */
#define URL_MAX_LEN         (2048)          /* URL最大长度 */
#define URI_MIN_LEN     URL_MIN_LEN         /* URI最小长度 */
#define URI_MAX_LEN     URL_MAX_LEN         /* URI最大长度 */
#define HOST_MAX_LEN        (256)           /* 域名最大长度 */
#define PORT_MAX_LEN        (12)            /* 端口号最大长度 */
#define URI_SUFFIX_LEN      (8)             /* 后缀长度 */
#define URI_PROTOCOL_MAX_LEN    (32)        /* 协议类型 */

#define URI_DEF_PORT        (80)    /* 默认端口 */
#define URI_DEF_PROTOCOL    "http"  /* 默认协议 */

#define URI_WWW_STR         "www."          /* 万维网 */
#define URI_WWW_STR_LEN     (4)
#define URI_HTTP_STR        "http://"       /* HTTP连接 */
#define URI_HTTP_STR_LEN    (7)
#define URI_HTTPS_STR       "https://"      /* HTTPS连接 */
#define URI_HTTPS_STR_LEN   (8)
#define URI_FTP_STR         "ftp://"        /* FTP连接 */
#define URI_FTP_STR_LEN     (6)
#define URI_MAILTO_STR      "mailto:"       /* Mail连接 */
#define URI_MAILTO_STR_LEN  (7)
#define URI_THUNDER_STR     "thunder:"      /* 迅雷连接 */
#define URI_THUNDER_STR_LEN (8)
#define URI_ITEM_STR        "item://"       /* ITUNES连接 */
#define URI_ITEM_STR_LEN    (7)
#define URI_ED2K_STR        "ed2k://"       /* 电驴连接 */
#define URI_ED2K_STR_LEN    (7)

/* 错误码 */
typedef enum
{
    URI_OK
    , URI_ERR = ~0x7fffffff                 /* 异常 */
    , URI_ERR_TRIM                          /* URI过滤有误 */
    , URI_ERR_INVALID                       /* URI非法 */
    , URI_ERR_LEN                           /* 长度非法 */
    , URI_ERR_PROTO                         /* 协议非法 */
    , URI_ERR_HOST                          /* HOST非法 */
    , URI_ERR_PORT                          /* PORT非法 */
    , URI_ERR_PATH                          /* PATH非法 */
    , URI_ERR_RESLOVE                       /* 解析错误 */
} uri_err_e;

/* 协议类型 */
typedef enum
{
    URI_UNKNOWN_PROTOCOL
    , URI_HTTP_PROTOCOL
    , URI_HTTPS_PROTOCOL
    , URI_FTP_PROTOCOL
    , URI_MAIL_PROTOCOL
    , URI_THUNDER_PROTOCOL
    , URI_ITEM_PROTOCOL
    , URI_ED2K_PROTOCOL

    , URI_PROTOCOL_TOTAL
} uri_protocol_e;

#define href_is_abs(str) ('/' == str[0])            /* href是绝对路径 */
#define href_is_up(str) (!strncmp("../", str, 3))   /* href是上级路径 */
#define href_is_loc(str) (!strncmp("./", str, 2))   /* href是当前路径 */

/* URI字段 */
typedef struct
{
    char uri[URI_MAX_LEN];      /* URI */
    int len;                    /* URI长度 */

    uri_protocol_e protocol;    /* 协议类型 */
    char host[URI_MAX_LEN];     /* 域名 */
    char path[URI_MAX_LEN];     /* 路径信息 */
    int port;                   /* 端口号 */
    char suffix[URI_SUFFIX_LEN];/* 后缀字段 */
} uri_field_t;

int uri_trim(const char *in, char *out, size_t size);
bool uri_is_valid(const char *uri);
bool uri_is_valid_suffix(const char *suffix);
int uri_reslove(const char *uri, uri_field_t *f);
int href_to_uri(const char *href, const char *site, uri_field_t *field);

#endif /*__URI_H__*/

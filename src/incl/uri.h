#if !defined(__URI_H__)
#define __URI_H__

#include "common.h"

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

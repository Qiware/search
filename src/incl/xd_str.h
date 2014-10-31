#if !defined(__XD_STR_H__)
#define __XD_STR_H__

#include "common.h"

/* 字串 */
typedef struct
{
    char *str;      /* 字串值 */
    int len;        /* 字串长 */
} xd_str_t;
 
xd_str_t *str_to_lower(xd_str_t *s);
xd_str_t *str_to_upper(xd_str_t *s);
int str_trim(const char *in, char *out, size_t size);

#define URI_HTTP_STR        "http://"       /* HTTP连接 */
#define URI_HTTP_STR_LEN    (7)
#define URI_HTTPS_STR       "https://"      /* HTTPS连接 */
#define URI_HTTPS_STR_LEN   (8)

#define href_is_abs(str) ('/' == str[0])            /* href是绝对路径 */
#define href_is_up(str) (!strncmp("../", str, 3))   /* href是上级路径 */
#define href_is_loc(str) (!strncmp("./", str, 2))   /* href是当前路径 */

/* URI字段 */
typedef struct
{
    char uri[URI_MAX_LEN];      /* URI */
    int len;                    /* URI长度 */

    char protocol[URI_PROTOCOL_MAX_LEN];    /* 协议类型 */
    char host[URI_MAX_LEN];     /* 域名 */
    char path[URI_MAX_LEN];     /* 路径信息 */
    int port;                   /* 端口号 */
} uri_field_t;

int uri_reslove(const char *uri, uri_field_t *f);
int href_to_uri(const char *href, const char *site, uri_field_t *field);

#endif /*__XD_STR_H__*/

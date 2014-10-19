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

/* URI字段 */
typedef struct
{
    int type;                   /* 协议类型 */

    char uri[URI_MAX_LEN];      /* URI */
    char host[URI_MAX_LEN];     /* 域名 */
    char path[URI_MAX_LEN];     /* 路径信息 */
    int port;                   /* 端口号 */
} uri_field_t;

bool uri_is_valid(const char *uri);
int uri_get_path(const char *uri, char *path, int size);
int uri_get_host(const char *uri, char *host, int size);
int uri_reslove(const char *uri, uri_field_t *f);

#endif /*__XD_STR_H__*/

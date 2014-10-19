#if !defined(__HTTP_H__)
#define __HTTP_H__

#include "common.h"

#define HTTP_GET_REQ_STR_LEN    (4096)  /* HTTP GET请求的最大长度 */

typedef enum
{
    HTTP_OK                     /* 正常 */

    , HTTP_ERR                  /* 异常 */
} http_err_code_e;

/* HTTP GET请求的相关字段 */
typedef struct
{
    char path[URI_MAX_LEN];     /* 路径信息 */
    char host[URI_MAX_LEN];     /* HOST信息 */
} http_get_req_field_t;

int http_get_host_from_uri(const char *uri, char *host, int size);
int http_get_request(const char *uri, char *req, int size);
bool uri_is_valid(const char *uri);

#endif /*__HTTP_H__*/

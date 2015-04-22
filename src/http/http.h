#if !defined(__HTTP_H__)
#define __HTTP_H__

#include "comm.h"

/* 关键词定义 */
#define HTTP_KEY_VERS_09        "HTTP/0.9 "
#define HTTP_KEY_VERS_10        "HTTP/1.0 "
#define HTTP_KEY_VERS_11        "HTTP/1.1 "
#define HTTP_KEY_VERS_20        "HTTP/2.0 "
#define HTTP_KEY_VERS_LEN       (9)

#define HTTP_KEY_CONNECTION     "Connection:"
#define HTTP_KEY_CONNECTION_LEN (11)
#define HTTP_KEY_CONTENT_LEN    "Content-Length:"
#define HTTP_KEY_CONTENT_LEN_LEN    (15)

#define HTTP_KEY_CONNECTION_CLOSE       "Close"
#define HTTP_KEY_CONNECTION_CLOSE_LEN   (5)
#define HTTP_KEY_CONNECTION_KEEPALIVE   "Keep-alive"
#define HTTP_KEY_CONNECTION_KEEPALIVE_LEN   (10)

#define HTTP_DEF_PORT           (80)    /* HTTP默认端口号 */
#define HTTP_GET_REQ_STR_LEN    (3072)  /* HTTP GET请求的最大长度 */

#define HTTP_REP_STATUS_OK      (200)   /* 应答码200 */

typedef enum
{
    HTTP_OK                             /* 正常 */
    , HTTP_NOT_WHOLE                    /* 不完整 */

    , HTTP_ERR                          /* 异常 */
} http_err_code_e;

typedef enum
{
    HTTP_VERSION_09                     /* HTTP/0.9 */
    , HTTP_VERSION_10                   /* HTTP/1.0 */
    , HTTP_VERSION_11                   /* HTTP/1.1 */
    , HTTP_VERSION_20                   /* HTTP/2.0 */

    , HTTP_VERSION_TOTAL
} http_version_e;

typedef struct
{
    int version;                        /* 版本号 */
    int status;                         /* 状态码 */
#define HTTP_CONNECTION_CLOSE       (0) /* 关闭 */
#define HTTP_CONNECTION_KEEPALIVE   (1) /* 保活 */
    int connection;                     /* 连接方式 */
#define HTTP_CONTENT_MAX_LEN (0xFFFFFFFF)
    size_t content_len;                 /* 应答内容长度 */

    /* 计算而来 */
    size_t header_len;                  /* HTTP头长度 */
    size_t total_len;                   /* 总长: HTTP头长 + 内容长度(content_len) */
} http_response_t;

int http_get_request(const char *uri, char *req, int size);
int http_parse_response(const char *str, http_response_t *rep);

#endif /*__HTTP_H__*/

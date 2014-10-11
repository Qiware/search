#if !defined(__HTTP_H__)
#define __HTTP_H__

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

#endif /*__HTTP_H__*/

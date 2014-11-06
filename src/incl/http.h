#if !defined(__HTTP_H__)
#define __HTTP_H__

#include "common.h"

#define HTTP_DEF_PORT           (80)    /* HTTP默认端口号 */
#define HTTP_GET_REQ_STR_LEN    (3072)  /* HTTP GET请求的最大长度 */

typedef enum
{
    HTTP_OK                     /* 正常 */

    , HTTP_ERR                  /* 异常 */
} http_err_code_e;

int http_get_request(const char *uri, char *req, int size);

#endif /*__HTTP_H__*/

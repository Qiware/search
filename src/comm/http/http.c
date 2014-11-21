#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "log.h"
#include "http.h"
#include "xd_str.h"
#include "common.h"

/******************************************************************************
 **函数名称: http_get_request
 **功    能: 构造HTTP GET请求
 **输入参数:
 **     uri: URI
 **     size: req长度
 **输出参数:
 **     req: HTTP GET请求字串
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Menglai.Wang # 2014.09.23 #
 ******************************************************************************/
int http_get_request(const char *uri, char *req, int size)
{
    uri_field_t field;

    /* 1. 获取GET相关字段 */
    if(0 != uri_reslove(uri, &field))
    {
        return HTTP_ERR;
    }

    /* 2. 构建HTTP GET请求 */
    snprintf(req, size,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Accept: */*\r\n"
        "Accept-Language: zh-cn\r\n"
        "Accept-Charset: utf-8\r\n"
        "Connection: close\r\n"
        "User-Agent: XunDao Crawler\r\n"
        "\r\n", field.path, field.host);

    return HTTP_OK;
}

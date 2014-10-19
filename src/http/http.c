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
 **函数名称: http_get_field_from_uri
 **功    能: 从URI中获取GET请求字段
 **输入参数:
 **     uri: URI
 **     filed: HTTP GET请求的域
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.10 #
 ******************************************************************************/
int http_get_field_from_uri(const char *uri, uri_field_t *f)
{
    int ret;
    
    /* 1. 获取PATH字段 */
    ret = uri_get_path(uri, f->path, sizeof(f->path));
    if (HTTP_OK != ret)
    {
        log2_error("Get path failed! uri:%s", uri);
        return HTTP_ERR;
    }

    /* 2. 获取HOST字段 */
    ret  = uri_get_host(uri, f->host, sizeof(f->host));
    if (HTTP_OK != ret)
    {
        log2_error("Get host failed! uri:%s", uri);
        return HTTP_ERR;
    }

    return HTTP_OK;
}

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
    int ret;
    uri_field_t field;

    /* 1. 获取GET相关字段 */
    ret = uri_get_path(uri, field.path, sizeof(field.path));
    if (0 != ret)
    {
        return HTTP_ERR;
    }

    ret = uri_get_host(uri, field.host, sizeof(field.host));
    if (0 != ret)
    {
        return HTTP_ERR;
    }

    /* 2. 构建HTTP GET请求 */
    snprintf(req, size,
        "GET %s HTTP/1.1\r\n"
        "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:33.0) Gecko/20100101 Firefox/33.0\r\n"
        "Host: %s\r\n"
        "Accept: */*\r\n"
        "Accept-Language: zh-cn\r\n"
        "Accept-Charset: utf-8\r\n"
        "Connection: Close\r\n"
        "\r\n", field.path, field.host);

    return HTTP_OK;
}

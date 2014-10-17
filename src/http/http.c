#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "log.h"
#include "http.h"
#include "common.h"

/******************************************************************************
 **函数名称: http_get_path_from_uri
 **功    能: 从URI中获取PATH字串
 **输入参数:
 **     uri: URI
 **     size: req长度
 **输出参数:
 **     path: PATH字串
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     uri:http://www.bai.com/news.html
 **     如果URI为以上值，那么提取之后的PATH为/new.html
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.10 #
 ******************************************************************************/
int http_get_path_from_uri(const char *uri, char *path, int size)
{
    const char *ptr = uri;

    while ('\0' != *ptr)
    {
        if ('/' != *ptr)
        {
            ptr++;
            continue;
        }

        if ('/' == *(ptr + 1))
        {
            ptr += 2;
            continue;
        }

        break;
    }

    if ('\0' == *ptr)
    {
        snprintf(path, size, "/");
        return HTTP_OK;
    }

    snprintf(path, size, "%s", ptr);

    return HTTP_OK;
}

/******************************************************************************
 **函数名称: http_get_host_from_uri
 **功    能: 从URI中获取HOST字串
 **输入参数:
 **     uri: URI
 **     size: req长度
 **输出参数:
 **     host: HOST字串
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.10 #
 ******************************************************************************/
int http_get_host_from_uri(const char *uri, char *host, int size)
{
    int len;
    const char *ptr = uri,
          *start = uri, *end = uri;

    while ('\0' != *ptr)
    {
        if ('/' != *ptr)
        {
            ptr++;
            continue;
        }

        if ('/' == *(ptr + 1))
        {
            start = ptr + 2;
            ptr += 2;
            continue;
        }
        else if ('/' != *(ptr + 1))
        {
            end = ptr - 1;
            break;
        }

        ptr++;
    }

    if (start >= end)
    {
        snprintf(host, size, "%s", start);
        return HTTP_OK;
    }

    len = (end - start) + 1;
    if (size <= len)
    {
        log2_error("Host name is too long! len:%d", len);
        return HTTP_OK;
    }

    memcpy(host, start, len);
    host[len] = '\0';

    return HTTP_OK;
}

/******************************************************************************
 **函数名称: http_parse_get_req_field_from_uri
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
int http_parse_get_req_field_from_uri(const char *uri, http_get_req_field_t *f)
{
    int ret;
    
    /* 1. 获取PATH字段 */
    ret = http_get_path_from_uri(uri, f->path, sizeof(f->path));
    if (HTTP_OK != ret)
    {
        log2_error("Get path failed! uri:%s", uri);
        return HTTP_ERR;
    }

    /* 2. 获取HOST字段 */
    ret  = http_get_host_from_uri(uri, f->host, sizeof(f->host));
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
    http_get_req_field_t field;

    /* 1. 获取GET相关字段 */
    ret  = http_parse_get_req_field_from_uri(uri, &field);
    if (HTTP_OK != ret)
    {
        log2_error("Get request field failed! uri:%s", uri);
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

    fprintf(stderr, "%s", req);

    return HTTP_OK;
}

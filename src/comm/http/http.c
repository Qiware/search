#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "log.h"
#include "str.h"
#include "http.h"
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
 **作    者: # Qifeng.zou # 2014.09.23 #
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

/******************************************************************************
 **函数名称: http_parse_response
 **功    能: 解析HTTP应答
 **输入参数:
 **     str: 应答字串
 **输出参数:
 **     resp: 应答信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.03 #
 ******************************************************************************/
int http_parse_response(const char *str, http_response_t *rep)
{
    int len;
    const char *p, *end, *tmp;

    end = strstr(str, "\r\n\r\n");
    if (NULL == end)
    {
        return HTTP_ERR; /* 不完整 */
    }

    p = str;
    while (' ' == *p) { ++p; }

    /* > 获取版本, 状态 */
    if (!strncasecmp(p, HTTP_KEY_VERS_09, strlen(HTTP_KEY_VERS_09)))
    {
        rep->version = HTTP_VERSION_09;
        len = strlen(HTTP_KEY_VERS_09);
    }
    else if (!strncasecmp(p, HTTP_KEY_VERS_10, strlen(HTTP_KEY_VERS_10)))
    {
        rep->version = HTTP_VERSION_10;
        len = strlen(HTTP_KEY_VERS_10);
    }
    else if (!strncasecmp(p, HTTP_KEY_VERS_11, strlen(HTTP_KEY_VERS_11)))
    {
        rep->version = HTTP_VERSION_11;
        len = strlen(HTTP_KEY_VERS_11);
    }
    else if (!strncasecmp(p, HTTP_KEY_VERS_20, strlen(HTTP_KEY_VERS_20)))
    {
        rep->version = HTTP_VERSION_20;
        len = strlen(HTTP_KEY_VERS_20);
    }
    else
    {
        return HTTP_ERR;
    }

    p += len;
    tmp = p;
    while (isdigit(*tmp))
    {
        rep->status *= 10;
        rep->status += (*tmp - '0');
        ++tmp;
    }

    if (rep->status < 100 || rep->status > 999)
    {
        return HTTP_ERR;
    }
    else if (200 != rep->status)
    {
        return HTTP_OK;
    }

    p = strstr(p, "\r\n");
    if (p == end)
    {
        return HTTP_ERR;
    }

    p += 2;

    /* > 获取其他信息 */
    while (p != end)
    {
        while (' ' == *p) { ++p; }
        if (p == end)
        {
            break;
        }

        /* 连接方式 */
        if (!strncasecmp(p, HTTP_KEY_CONNECTION, strlen(HTTP_KEY_CONNECTION)))
        {
            p += strlen(HTTP_KEY_CONNECTION);
            while (' ' == *p) { ++p; }
            if (!strncasecmp(p, HTTP_KEY_CONNECTION_CLOSE, strlen(HTTP_KEY_CONNECTION_CLOSE)))
            {
                rep->connection = HTTP_CONNECTION_CLOSE;
            }
            else if (!strncasecmp(p, HTTP_KEY_CONNECTION_KEEPALIVE, strlen(HTTP_KEY_CONNECTION_KEEPALIVE)))
            {
                rep->connection = HTTP_CONNECTION_KEEPALIVE;
            }
            else
            {
                return HTTP_ERR;
            }
        }
        /* 内容长度 */
        else if (!strncasecmp(p, HTTP_KEY_CONTENT_LEN, strlen(HTTP_KEY_CONTENT_LEN)))
        {
            p += strlen(HTTP_KEY_CONTENT_LEN);
            while (' ' == *p) { ++p; }
            while (isdigit(*p))
            {
                rep->content_len *= 10;
                rep->content_len += (*p - '0');
            }
        }

        p = strstr(p, "\r\n");
        if (p == end)
        {
            break;
        }
        p += 2;
    }

    return HTTP_OK;
}

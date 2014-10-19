#include <string.h>

#include "xd_str.h"
#include <ctype.h>

/******************************************************************************
 **函数名称: str_to_lower
 **功    能: 将字串转为小字母
 **输入参数:
 **     str: 字串
 **输出参数:
 **返    回: 字串对象
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
xd_str_t *str_to_lower(xd_str_t *s)
{
    int idx;

    for (idx=0; idx<s->len; ++idx)
    {
        switch (s->str[idx])
        {
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            {
                s->str[idx] += 32;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return s;
}

/******************************************************************************
 **函数名称: str_to_upper
 **功    能: 将字串转为大字母
 **输入参数:
 **     str: 字串
 **输出参数:
 **返    回: 字串对象
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
xd_str_t *str_to_upper(xd_str_t *s)
{
    int idx;

    for (idx=0; idx<s->len; ++idx)
    {
        switch (s->str[idx])
        {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            {
                s->str[idx] -= 32;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return s;
}

/******************************************************************************
 **函数名称: str_trim
 **功    能: 删除字串前后的空格、换行符.
 **输入参数:
 **     str: 字串
 **     size: out空间大小
 **输出参数:
 **     out: 输出字符串
 **返    回: out长度
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.19 #
 ******************************************************************************/
int str_trim(const char *in, char *out, size_t size)
{
    size_t len;
    const char *s = in, *e = in + strlen(in);

    /* 1. 删除头部空格、换行符等 */
    while ('\0' != *s)
    {
        if ((' ' == *s)
            || ('\n' == *s) || ('\r' == *s))
        {
            ++s;
            continue;
        }
        break;
    }

    if (e == s)
    {
        out[0] = '\0';
        return 0;
    }

    /* 2. 删除尾部空格、换行符等 */
    --e;
    while (e > s)
    {
        if ((' ' == *e)
            || ('\n' == *e) || ('\r' == *e))
        {
            --e;
            continue;
        }
        break;
    }

    len = e - s + 1;
    len = (len < size)? len : (size - 1);
    strncpy(out, s, len);
    out[len] = '\0';
   
    return len;
}

/******************************************************************************
 **函数名称: uri_is_valid
 **功    能: 判断URI是否合法
 **输入参数:
 **     uri: URI
 **输出参数:
 **返    回: true:合法 false:不合法
 **实现描述: 
 **     1. URI中不能有空格
 **     2. 域名只能出现字母、数字、点、下划线
 **注意事项:
 **     URI格式: 域名 + 路径 + 参数
 **作    者: # Qifeng.zou # 2014.10.19 #
 ******************************************************************************/
bool uri_is_valid(const char *uri)
{
    int ret;
    char host[URI_MAX_LEN];
    const char *ch;

    /* 1. 判断域名合法性 */
    ret = uri_get_host(uri, host, sizeof(host));
    if (0 != ret)
    {
        return false;
    }

    ch = host;
    while ('\0' != *ch)
    {
        if (!isalpha(*ch)
            && !isdigit(*ch)
            && '.' != *ch && '_' != *ch && '-' != *ch)
        {
            return false;
        }

        ++ch;
    }

    /* 2. 判断是否有空格 */
    ch = uri;
    while ('\0' != *ch)
    {
        if (isspace(*ch))
        {
            return false;
        }

        ++ch;
    }
   
    return true;
}

/******************************************************************************
 **函数名称: uri_get_path
 **功    能: 从URI中获取PATH字串
 **输入参数:
 **     uri: URI
 **     size: req长度
 **输出参数:
 **     path: PATH字串
 **返    回: 端口号 (<0: 失败)
 **实现描述: 
 **     uri:http://www.bai.com/news.html
 **     如果URI为以上值，那么提取之后的PATH为/new.html
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.19 #
 ******************************************************************************/
int uri_get_path(const char *uri, char *path, int size)
{
    int port = 80;
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
        return port;
    }

    snprintf(path, size, "%s", ptr);

    return port;
}

/******************************************************************************
 **函数名称: uri_get_host
 **功    能: 从URI中获取HOST字串
 **输入参数:
 **     uri: URI
 **     size: Host长度
 **输出参数:
 **     host: HOST字串
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.19 #
 ******************************************************************************/
int uri_get_host(const char *uri, char *host, int size)
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
        return 0;
    }

    len = (end - start) + 1;
    if (size <= len)
    {
        return -1;   /* Host name is too long */
    }

    memcpy(host, start, len);
    host[len] = '\0';

    return 0;
}

/******************************************************************************
 **函数名称: uri_reslove
 **功    能: 分解URI字串
 **输入参数:
 **     uri: URI
 **输出参数:
 **     field: 从URI中提取字段
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.19 #
 ******************************************************************************/
int uri_reslove(const char *uri, uri_field_t *field)
{
    int ret, len;

    /* 1. 剔除URI前后的非法字符 */
    len = str_trim(uri, field->uri, sizeof(field->uri));
    if (0 == len)
    {
        return 0;
    }
    
    /* 2. 从URI中提取域名、端口、路径等信息 */
    field->port = uri_get_host(field->uri, field->host, sizeof(field->host));
    if (0 != field->port)
    {
        return -1;
    }

    ret = uri_get_path(field->uri, field->path, sizeof(field->path));
    if (0 != ret)
    {
        return -1;
    }

    return 0;
}

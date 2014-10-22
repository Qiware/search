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
            || ('\n' == *s)
            || ('\r' == *s))
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
            || ('\n' == *e)
            || ('\r' == *e))
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
#define URI_DEF_PORT        (80)
#define URI_DEF_PROTOCOL    "http"
    int len;
    const char *ch, *s;

    memset(field, 0, sizeof(uri_field_t));

    /* 1. 剔除URI前后的非法字符 */
    field->len = str_trim(uri, field->uri, sizeof(field->uri));
    if (0 == field->len)
    {
        return -1;  /* 长度=0 */
    }
    
    /* 2. 从URI中提取域名、端口、路径等信息 */
    ch = field->uri;
    s = ch;

    /* 判断是否有协议(http:// | https:// | ftp:// ...) */
    while ('\0' != *ch)
    {
        if (':' != *ch && '/' != *ch)
        {
            ++ch;
            continue;
        }
        break;
    }

    if ('\0' == *ch)
    {
        /* 只有域名: (协议/端口/路径 使用默认值) */
        snprintf(field->protocol, sizeof(field->protocol), "%s", URI_DEF_PROTOCOL);
        snprintf(field->host, sizeof(field->host), "%s", field->uri);
        snprintf(field->path, sizeof(field->path), "/");
        field->port = URI_DEF_PORT;
        return 0;
    }
    /* 1. 有协议 或 有端口号 */
    else if (':' == *ch)
    {
        /* 有协议类型 */
        if ('/' == *(ch + 1)
            && '/' == *(ch + 1))
        {
            /* 提取协议 */
            len = ch - field->uri;
            len = (len < (sizeof(field->protocol) - 1))?
                        len : (sizeof(field->protocol) - 1);
            memcpy(field->protocol, field->uri, len);
            field->protocol[len] = '\0';

            /* 提取域名 */
            ch += 3;
            s = ch;
            while ('\0' != *ch)
            {
                if (isalpha(*ch)
                    || isdigit(*ch)
                    || ('.' == *ch)
                    || ('-' == *ch)
                    || ('_' == *ch))
                {
                    ++ch;
                    continue;
                }
                break;
            }

            if ('\0' == *ch)        /* 无端口、无路径 */
            {
                /* 有协议与域名: (端口/路径 使用默认值) */
                snprintf(field->host, sizeof(field->host), "%s", field->uri);
                snprintf(field->path, sizeof(field->path), "/");
                field->port = URI_DEF_PORT;
                return 0;
            }
            else if (':' == *ch)    /* 有端口 */
            {
                if (!isdigit(*(ch + 1)))
                {
                    return -1;  /* 格式有误 */
                }

            URI_GET_HOST:
                /* 提取域名 */
                len = ch - s;
                len = (len < sizeof(field->host) - 1)?
                            len : sizeof(field->host) - 1;
                memcpy(field->host, s, len);
                field->host[len] = '\0';

            URI_GET_PORT:
                /* 提取端口号 */
                ++ch;
                s = ch;
                while ('\0' != *ch)
                {
                    if (isdigit(*ch))
                    {
                        ++ch;
                        continue;
                    }
                    break;
                }

                field->port = atoi(s);

                /* 提取路径 */
                if ('\0' == *ch)
                {
                    snprintf(field->path, sizeof(field->path), "/");
                    return 0;
                }
                else if ('/' != *ch)
                {
                    return -1;  /* 格式有误 */
                }

                snprintf(field->path, sizeof(field->path), "%s", ch);
                return 0;
            }
            else if ('/' == *ch)     /* 无端口、有路径 */
            {
                /* 提取域名 */
                len = ch - s;
                len = (len < sizeof(field->host) - 1)?
                            len : sizeof(field->host) - 1;
                memcpy(field->host, s, len);
                field->host[len] = '\0';
                field->port = URI_DEF_PORT;   /* 默认值 */
                snprintf(field->path, sizeof(field->path), "%s", ch);
                return 0;
            }
            else
            {
                return -1;  /* 格式有误 */
            }
            return 0;
        }
        /* 无协议、有端口号 */
        else if (isdigit(*(ch + 1)))
        {
            /* 设置协议(默认:HTTP) */
            snprintf(field->protocol, sizeof(field->protocol), "%s", URI_DEF_PROTOCOL);

            /* 提取域名 */
            s = field->uri;

            len = ch - s;
            len = (len < sizeof(field->host) - 1)? len : sizeof(field->host) - 1;
            memcpy(field->host, s, len);
            field->host[len] = '\0';

            /* 提取端口号 */
            goto URI_GET_PORT;
        }

        return -1; /* 格式有误 */
    }
    /* 2. 无协议 且 无端口号 */
    else if ('/' == *ch)
    {
        if (s == ch)
        {
            return -1;  /* 此URL为相对地址: /Default.asp?opt=query&tel=123 */
        }

        /* 设置协议(默认:HTTP) */
        snprintf(field->protocol, sizeof(field->protocol), "%s", URI_DEF_PROTOCOL);

        goto URI_GET_HOST;
    }

    return -1;
}

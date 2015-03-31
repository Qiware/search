#include "uri.h"

/******************************************************************************
 **函数名称: uri_trim
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
int uri_trim(const char *in, char *out, size_t size)
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

    /* 判断合法性
     * 1. 长度是否为0
     * 2. 首字符是否合法
     * */
    if ((e == s)
        || (!isalpha(*s) && !isdigit(*s)))
    {
        out[0] = '\0';
        return 0;
    }

    /* 2. 删除尾部空格、换行符、路径分隔符等 */
    --e;
    while (e > s)
    {
        if ((' ' == *e)
            || ('/' == *e)
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
 **函数名称: uri_get_protocol
 **功    能: 获取协议类型
 **输入参数:
 **     uri: URI
 **输出参数:
 **返    回: 协议类型
 **实现描述: 
 **		依次与各协议标签进行比较
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.28 #
 ******************************************************************************/
int uri_get_protocol(const char *uri)
{
	const char *p= uri;

	if (!strncmp(uri, URI_WWW_STR, URI_WWW_STR_LEN))
	{
		return  URI_HTTP_PROTOCOL;
	}
	else if (!strncmp(uri, URI_HTTP_STR, URI_HTTP_STR_LEN))
	{
		return URI_HTTP_PROTOCOL;
	}
	else if (!strncmp(uri, URI_HTTPS_STR, URI_HTTPS_STR_LEN))
	{
		return URI_HTTPS_PROTOCOL;
	}
	else if (!strncmp(uri, URI_FTP_STR, URI_FTP_STR_LEN))
	{
		return URI_FTP_PROTOCOL;
	}
	else if (!strncmp(uri, URI_THUNDER_STR, URI_THUNDER_STR_LEN))
	{
		return URI_THUNDER_PROTOCOL;
	}
	else if (!strncmp(uri, URI_ITEM_STR, URI_ITEM_STR_LEN))
	{
		return URI_ITEM_PROTOCOL;
	}
	else if (!strncmp(uri, URI_ED2K_STR, URI_ED2K_STR_LEN))
	{
		return URI_ED2K_PROTOCOL;
	}

	while ('\0' != *p)
	{
		if (isalpha(*p))
		{
			++p;
			continue;
		}
		else if ('.' == *p)
		{
			return URI_HTTP_PROTOCOL; /* 如：baidu.com */
		}

		return URI_UNKNOWN_PROTOCOL;
	}

	return URI_UNKNOWN_PROTOCOL;
}

/******************************************************************************
 **函数名称: uri_get_host
 **功    能: 获取HOST(IP或域名)
 **输入参数:
 **     uri: URI
 **		size: host空间大小
 **输出参数:
 **		host: 主机IP或域名
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.28 #
 ******************************************************************************/
int uri_get_host(const char *uri, char *host, int size)
{
	int len;
	const char *p, *s;

	p = s = uri;

	while (isalpha(*p) || isdigit(*p))
	{
		++p;
	}

    switch (*p)
    {
        case ':':
        {
            if ('/' == *(p+1))
            {
                if ('/' == *(p+2))
                {
                    p += 3;
                    s = p;
                    break;
                }
                return -1;
            }
            else if (isdigit(*(p+1)))
            {
                ++p;
                goto GET_HOST;
            }
            return -1;
        }
        case '\0':
        {
            goto GET_HOST;
        }
	    case '.':
        {
            ++p;
            break; /* 继续处理 */
        }
        default:
        {
            return -1;
        }
    }

	while (isalpha(*p) || isdigit(*p)
        || '.' == *p || '-' == *p || '_' == *p)
	{
		++p;
	}

GET_HOST:
	len = p - s;
	if (len >= size)
	{
		return -1;
	}

	memcpy(host, s, len);
	host[len] = '\0';

	return 0;
}

/******************************************************************************
 **函数名称: uri_get_port
 **功    能: 获取通信端口号
 **输入参数:
 **     uri: URI
 **输出参数:
 **返    回: 通信端口号
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.28 #
 ******************************************************************************/
int uri_get_port(const char *uri)
{
	int len;
	const char *p, *s;
    char port[PORT_MAX_LEN];

	p = s = uri;

	while (isalpha(*p) || isdigit(*p) || '.' == *p)
	{
		++p;
	}

    switch (*p)
    {
        case ':':
        {
            if ('/' == *(p+1)) {
                if ('/' == *(p+2)) {
                    p += 3;
                    break;
                }
                else {
                    return -1;
                }
            }
            else if (isdigit(*(p+1))) {
                ++p;
                goto GET_PORT;
            }
            return -1;
        }
        case '\0':
        default:
        {
            return URI_DEF_PORT;
        }
    }

	while (isalpha(*p) || isdigit(*p) || '.' == (*p))
    {
        ++p;
    }

    if ('\0' == *p || ':' != *p)
    {
        return URI_DEF_PORT;
    }

GET_PORT:
	++p;
    s = p;
	while (isdigit(*p))
	{
		++p;
	}

	len = p - s;
    if (0 == len || len > (int)sizeof(port))
    {
        return -1;
    }

    memcpy(port, s, len);

	return atoi(port);
}

/******************************************************************************
 **函数名称: uri_get_path
 **功    能: 获取URI路径
 **输入参数:
 **     uri: URI
 **		size: path空间大小
 **输出参数:
 **		path: 网页路径
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.29 #
 ******************************************************************************/
int uri_get_path(const char *uri, char *path, int size)
{
    const char *p;

    p = strstr(uri, "/");
	if (NULL == p)
	{
        snprintf(path, size, "/");
        return 0;
	}

    if ('/' != *(p+1))
    {
        snprintf(path, size, "%s", p);
        return 0;
    }

    p = strstr(p+2, "/");
    if (NULL == p)
    {
        snprintf(path, size, "/");
        return 0;
    }

    snprintf(path, size, "%s", p);

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
 **     1) 在路径符号'/'之前, 不允许出现特殊符号:# ?等
 **     2) 出现PDF EXE等后缀的话, 放弃处理
 **作    者: # Qifeng.zou # 2014.10.19 #
 ******************************************************************************/
int uri_reslove(const char *uri, uri_field_t *field)
{
    memset(field, 0, sizeof(uri_field_t));

    /* 1. 剔除URI前后的非法字符 */
    field->len = uri_trim(uri, field->uri, sizeof(field->uri));
    if (field->len <= URI_MIN_LEN)
    {
        return -1;  /* 长度非法 */
    }
    
	/* 2. 获取协议类型 */
	field->protocol = uri_get_protocol(uri);
    if (URI_UNKNOWN_PROTOCOL == field->protocol)
    {
		return -1; /* 只支持HTTP协议 */
    }

	/* 3. 获取HOST 端口 路径等 */
    if (uri_get_host(uri, field->host, sizeof(field->host)))
    {
        return -1;
    }

    field->port = uri_get_port(uri);
	if (field->port < 0)
    {
        return -1;
    }

	if (uri_get_path(uri, field->path, sizeof(field->path)))
	{
		return -1;
	}

    return 0;
}

/******************************************************************************
 **函数名称: uri_is_valid
 **功    能: 判断URI的合法性
 **输入参数: 
 **     uri: URI
 **输出参数: NONE
 **返    回: true:合法 false:不合法
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.31 #
 ******************************************************************************/
bool uri_is_valid(const char *uri)
{
    if (NULL == uri
        || '\0' == *uri)
    {
        return false;
    }

    for (; '\0' != *uri; ++uri)
    {
        switch (*uri)
        {
            case ' ':
            case '\n':
            case '\r':
            case '[':
            case ']':
            case '(':
            case ')':
            case ';':
            {
                return false;
            }
        }
    }

    return true;
}

/* URI合法后缀 */
static const char g_uri_suffix[][URI_SUFFIX_LEN] = 
{
    /* 按网页划分 */
    ".html"         /* 静态网页 */
    , ".htm"        /* 静态网页 */
    , ".asp"        /* ASP动态网页 */
    , ".aspx"       /* ASP动态网页 */
    , ".php"        /* PHP动态网页 */
    , ".shtml"      /* 静态网页 */
    , ".shtm"       /* 静态网页 */
    , ".stm"        /* 静态网页 */

    /* 按机构划分 */
    , ".rec"        /* 娱乐类 */
    , ".arts"       /* 艺术类 */
    , ".hom"        /* 个人类 */
    , ".com"        /* 商业类 */
    , ".edu"        /* 教育类 */
    , ".gov"        /* 政府类 */
    , ".org"        /* 非盈利类 */
    , ".info"       /* 信息服务类 */
    , ".net"        /* 网络服务机构类 */

    /* 按地域划分 */
    , ".cn"         /* 中国大陆 */
    , ".hk"         /* 中国香港 */
    , ".tw"         /* 中国台湾 */

    /* 结束标志: 请在此行上方添加合法后缀 */
    , ""
};

/******************************************************************************
 **函数名称: uri_is_valid_suffix
 **功    能: 判断URI的后缀是否合法性
 **输入参数: 
 **     uri: URI
 **输出参数: NONE
 **返    回: true:合法 false:不合法
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
bool uri_is_valid_suffix(const char *suffix)
{
    int idx;

    for (idx=0; '\0' != g_uri_suffix[idx][0]; ++idx)
    {
        if (!strcmp(suffix, g_uri_suffix[idx]))
        {
            return true;
        }
    }

    return false;
}

/******************************************************************************
 **函数名称: href_to_uri
 **功    能: 将href字段转化成uri
 **输入参数: 
 **     href: 从网页site中提取出来的href字段
 **     site: 网址
 **输出参数:
 **     field: 解析href后URI的各域信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     href字段值可能为正常的URI, 也可能为绝对路径, 也可能为相对路径,也可能错误.
 **作    者: # Qifeng.zou # 2014.10.30 #
 ******************************************************************************/
int href_to_uri(const char *href, const char *site, uri_field_t *field)
{
    int len, up = 0;
    const char *p, *p2;
    char uri[URI_MAX_LEN], tmp[URI_MAX_LEN];

    /* 1. 踢出URI前后的空格 */
    len = uri_trim(href, tmp, sizeof(tmp));
    if (len <= 0)
    {
        return -1;
    }

    if (!uri_is_valid(tmp))
    {
        return -1;
    }

    /* 2. 判断URI类型(正常的URI, 相对路径, 绝对路径, 也可能异常) */
    /* 类似格式: http://www.baidu.com */
    if (!strncmp(URI_HTTP_STR, tmp, URI_HTTP_STR_LEN)
        || !strncmp(URI_WWW_STR, tmp, URI_WWW_STR_LEN))
    {
        return uri_reslove(tmp, field);
    }
    else if (!strncmp(URI_HTTPS_STR, tmp, URI_HTTPS_STR_LEN)
        || !strncmp(URI_FTP_STR, tmp, URI_FTP_STR_LEN)
        || !strncmp(URI_MAILTO_STR, tmp, URI_MAILTO_STR_LEN)
        || !strncmp(URI_THUNDER_STR, tmp, URI_THUNDER_STR_LEN)
        || !strncmp(URI_ITEM_STR, tmp, URI_ITEM_STR_LEN)
        || !strncmp(URI_ED2K_STR, tmp, URI_ED2K_STR_LEN))
    {
        return -1;  /* 不支持的协议类型 */
    }

    /* 绝对路径(以斜杠'/'开头或以字母或数字开头的路径，都可视为绝对路径) */
    if (href_is_abs(tmp) || isalpha(*tmp))
    {
        if (0 != uri_reslove(site, field))
        {
            return -1;
        }

        if (isalpha(*tmp))
        {
            snprintf(uri, sizeof(uri), "%s%s:%d/%s", URI_HTTP_STR, field->host, field->port, tmp);
        }
        else
        {
            snprintf(uri, sizeof(uri), "%s%s:%d%s", URI_HTTP_STR, field->host, field->port, tmp);
        }

        return uri_reslove(uri, field);
    }

    /* 相对路径 */
    if (href_is_up(tmp)) /* 上一级目录 */
    {
        p = tmp;
        do
        {
            ++up;
            p += 3; 
        } while(href_is_up(p));

        p2 = site + strlen(site) - 1;
        if ('/' == *p2)
        {
            --p2;
        }

        for (; up > 0; --up)
        {
            while (('/' != *p2) && (p2 != site))
            {
                --p2;
            }

            if (p2 == site)
            {
                return -1;
            }

            --p2;
        }

        ++p2; /* 指向'/' */
        if (p2 - site <= URI_HTTP_STR_LEN)
        {
            return -1;
        }

        snprintf(uri, sizeof(uri), "%s%s", p2, p);

        return uri_reslove(uri, field);
    }
    else if (href_is_loc(tmp)) /* 本级目录 */
    {
        p = tmp + 2;

    HREF_IS_LOCAL_PATH:
        p2 = site + strlen(site) - 1;
        if ('/' == *p2)
        {
            --p2;
        }

        while (p2 != site && '/' != *p2)
        {
            --p2;
        }

        /* p2此时指向'/' */
        len = p2 - site;
        if (len <= URI_HTTP_STR_LEN
            || len >= URI_MAX_LEN)
        {
            return -1;
        }

        snprintf(uri, sizeof(uri), "%s", site);
        snprintf(uri+len, sizeof(uri)-len, "/%s", p);

        return uri_reslove(uri, field);
    }

    /* 本级目录分析 */
    p = tmp;
    goto HREF_IS_LOCAL_PATH;

    return -1;
}

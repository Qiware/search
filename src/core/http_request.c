

/******************************************************************************
 **函数名称: http_get_request
 **功    能: 构造HTTP请求字串
 **输入参数:
 **     url: URL
 **输出参数:
 **     req: HTTP请求字串
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Menglai.Wang # 2014.09.23 #
 ******************************************************************************/
void http_get_request(char *req, int size, const char *host, const char *path)
{
    snprintf(req, size,
        "GET %s HTTP/1.1\r\n"
        "User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022)\r\n"
        "Host: %s\r\n"
        "Accept: */*\r\n"
        "Accept-Language: zh-cn\r\n"
        "Accept-Charset: utf-8\r\n"
        "\r\n", path, host);
}

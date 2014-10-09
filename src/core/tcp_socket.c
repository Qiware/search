#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "syscall.h"
#include "xdo_socket.h"

/******************************************************************************
 **函数名称: tcp_listen
 **功    能: 侦听指定端口
 **输入参数: 
 **     port: 端口号
 **输出参数: NONE
 **返    回: 套接字ID
 **实现描述: 
 **     1. 创建套接字
 **     2. 绑定指定端口
 **     3. 侦听指定端口
 **     4. 设置套接字属性(可重用、非阻塞)
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int tcp_listen(int port)
{
    int fd, ret, opt = 1;
    struct sockaddr_in svraddr;

    /* 1. 创建套接字 */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    /* 2. 绑定指定端口 */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    svraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    svraddr.sin_port = htons(port);

    ret = bind(fd, (struct sockaddr *)&svraddr, sizeof(svraddr));
    if (ret < 0)
    {
        close(fd);
        return -1;
    }

    /* 3. 侦听指定端口 */
    ret = listen(fd, 20);
    if (ret < 0)
    {
        close(fd);
        return -1;
    }

    /* 4. 设置套接字属性(可重用、非阻塞) */
    opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt));

    fd_set_nonblocking(fd);

    return fd;
}

/******************************************************************************
 **函数名称: tcp_connect
 **功    能: 连接指定服务器
 **输入参数: 
 **     ipaddr: IP地址
 **     port: 端口号
 **输出参数: NONE
 **返    回: 套接字ID
 **实现描述: 
 **     1. 创建套接字
 **     2. 连接指定服务器
 **     3. 设置套接字属性(非阻塞)
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
int tcp_connect(const char *ipaddr, int port)
{
    int ret, fd;
    struct sockaddr_in svraddr;

    /* 1. 创建套接字 */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    /* 2. 连接远程服务器 */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipaddr, &svraddr.sin_addr);
    svraddr.sin_port = htons(port);

    ret = connect(fd, (struct sockaddr *)&svraddr, sizeof(svraddr));
    if (0 != ret)
    {
        close(fd);
        return -1;
    }

    fd_set_nonblocking(fd);

    return fd;
}

/******************************************************************************
 **函数名称: tcp_connect_ex
 **功    能: 连接指定服务器(超时返回)
 **输入参数: 
 **     ipaddr: IP地址
 **     port: 端口号
 **     sec: 超时时间
 **输出参数: NONE
 **返    回: 套接字ID
 **实现描述: 
 **     1. 创建套接字
 **     2. 连接指定服务器
 **     3. 设置套接字属性(非阻塞)
 **注意事项: 
 **作    者: # Qifeng.zou & Menglai.Wang # 2014.09.25 #
 ******************************************************************************/
int tcp_connect_ex(const char *ipaddr, int port, int sec)
{
    int ret, fd;
    fd_set rdset, wrset;
    struct timeval tv;
    struct sockaddr_in svraddr;

    /* 1. 创建套接字 */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    fd_set_nonblocking(fd);

    /* 2. 连接远程服务器 */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipaddr, &svraddr.sin_addr);
    svraddr.sin_port = htons(port);

    ret = connect(fd, (struct sockaddr *)&svraddr, sizeof(svraddr));
    if (0 == ret)
    {
        return fd;
    }

    if (EINPROGRESS != errno)
    {
        close(fd);
        return -1;
    }

    /* 3. 判断是否超时 */
AGAIN:
    FD_ZERO(&rdset);
    FD_ZERO(&wrset);

    FD_SET(fd, &rdset);
    FD_SET(fd, &wrset);

    tv.tv_sec = sec;
    tv.tv_usec = 0;
    ret = select(fd+1, &rdset, &wrset, NULL, &tv);
    if (ret < 0)
    {
        if (EINTR == errno)
        {
            goto AGAIN;
        }

        close(fd);
        return -1;
    }
    else if (0 == ret)
    {
        close(fd);
        return -1;
    }
    
    return fd;
}

/******************************************************************************
 **函数名称: tcp_connect_ex2
 **功    能: 连接指定服务器
 **输入参数: 
 **     ipaddr: IP地址
 **     port: 端口号
 **输出参数: NONE
 **返    回: 套接字ID
 **实现描述: 
 **     1. 创建套接字
 **     2. 设置为非阻塞套接字
 **     3. 连接指定服务器
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.09 #
 ******************************************************************************/
int tcp_connect_ex2(const char *ipaddr, int port)
{
    int ret, fd;
    struct sockaddr_in svraddr;

    /* 1. 创建套接字 */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    fd_set_nonblocking(fd);

    /* 2. 连接远程服务器 */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipaddr, &svraddr.sin_addr);
    svraddr.sin_port = htons(port);

    ret = connect(fd, (struct sockaddr *)&svraddr, sizeof(svraddr));
    if (0 == ret)
    {
        return fd;
    }

    if (EINPROGRESS != errno)
    {
        close(fd);
        return -1;
    }

    return fd;
}

/******************************************************************************
 **函数名称: fd_is_writable
 **功    能: 判断文件描述符是否可写
 **输入参数: 
 **     fd: 文件描述符
 **输出参数: NONE
 **返    回: 0:否 !0:是
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.06.07 #
 ******************************************************************************/
int fd_is_writable(int fd)
{
    fd_set wset;
    struct timeval tv;

    FD_ZERO(&wset);
    FD_SET(fd, &wset);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    return select(fd+1, NULL, &wset, NULL, &tv);
}

/******************************************************************************
 **函数名称: tcp_block_send
 **功    能: 阻塞发送
 **输入参数: 
 **    fd: 文件描述符
 **    addr: 被发送数据的起始地址
 **    len: 发送字节数
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **    等待所有字节发送完成后才返回，除非出现严重错误.
 **作    者: # Qifeng.zou # 2014.06.24 #
 ******************************************************************************/
int tcp_block_send(int fd, const void *addr, int len, int timeout)
{
    int ret, n, left = len, off = 0;
    fd_set wrset;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&wrset);
        FD_SET(fd, &wrset);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        ret = select(fd+1, NULL, &wrset, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            return -1;
        }
        else if (0 == ret)
        {
            return -1;
        }

        n = Writen(fd, (const char *)addr + off, left);
        if (n < 0)
        {
            return -1;
        }
        else if (left != n)
        {
            left -= n;
            off += n;
            continue;
        }

        return 0; /* Done */
    }
}

/******************************************************************************
 **函数名称: tcp_block_recv
 **功    能: 阻塞接收
 **输入参数: 
 **     fd: 文件描述符
 **     addr: 被发送数据的起始地址
 **     len: 发送字节数
 **     timeout: 阻塞时长(秒)
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **    等待所有字节接收完成后才返回，除非出现严重错误或超时.
 **作    者: # Qifeng.zou # 2014.06.24 #
 ******************************************************************************/
int tcp_block_recv(int fd, void *addr, int len, int timeout)
{
    int ret, n, left = len, off = 0;
    fd_set rdset;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);
        
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        ret = select(fd+1, &rdset, NULL, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }
            return -1;
        }
        else if (0 == ret)
        {
            return -1;
        }

        n = Readn(fd, (char *)addr + off, left);
        if (n < 0)
        {
            return -1;
        }
        else if (left != n)
        {
            left -= n;
            off += n;
            continue;
        }

        return 0; /* Done */
    }
}

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

#include "xdo_socket.h"
#include "xdo_unistd.h"

/******************************************************************************
 ** Name : xdo_tcp_listen
 ** Desc : Listen special port
 ** Input: 
 **     port: xdo_listen port
 ** Output: NONE
 ** Return: Socket fd
 ** Proc : 
 **     1. Create socket
 **     2. xdo_listen port
 **     3. Set socket attribute
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int xdo_tcp_listen(int port)
{
    int sckid = 0;
    int ret = 0, opt = 1;
    struct sockaddr_in svraddr;


    /* 1. Create socket */
    sckid = socket(AF_INET, SOCK_STREAM, 0);
    if (sckid < 0)
    {
        printf("errmsg:[%d] %s", errno, strerror(errno));
        return -1;
    }

    /* 2. Bind port */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    svraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    svraddr.sin_port = htons(port);

    ret = bind(sckid, (struct sockaddr *)&svraddr, sizeof(svraddr));
    if (ret < 0)
    {
        Close(sckid);
        printf("errmsg:[%d] %s", errno, strerror(errno));
        return -1;
    }

    /* 3. Set max queue */
    ret = listen(sckid, 20);
    if (ret < 0)
    {
        Close(sckid);
        printf("errmsg:[%d] %s", errno, strerror(errno));
        return -1;
    }

    /* 4. Set socket attribute */
    setsockopt(sckid, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt));

    ret = xdo_nonblocking(sckid);
    if (ret < 0)
    {
        Close(sckid);
        printf("errmsg:[%d] %s", errno, strerror(errno));
        return -1;
    }

    return sckid;
}

/******************************************************************************
 ** Name : xdo_usck_udp_creat
 ** Desc : Create Unix-UDP socket.
 ** Input: 
 **     path: file path
 ** Output: NONE
 ** Return: file descriptor
 ** Proc :
 **     1. Create unix-udp socket.
 **     2. Create file path.
 **     3. Bind special file path.
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int xdo_usck_udp_creat(const char *path)
{
    int fd = -1, len = 0, ret = 0;
    struct sockaddr_un svraddr;

    memset(&svraddr, 0, sizeof(svraddr));

    /* 1. Create Unix-UDP socket */
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd <0)
    {
        return -1;
    }

    /* 2. Create file path */
    Mkdir2(path, 0755);

    unlink(path);

    /* 3. Bind special file path */
    svraddr.sun_family = AF_UNIX;
    snprintf(svraddr.sun_path, sizeof(svraddr.sun_path), "%s", path);
    
    len = strlen(svraddr.sun_path) + sizeof(svraddr.sun_family);

    ret = bind(fd, (struct sockaddr *)&svraddr, len);
    if (ret<0)
    {
        return -1;
    }

    xdo_nonblocking(fd);

    return  fd;
}

/******************************************************************************
 ** Name : xdo_usck_udp_send
 ** Desc : Send data to special path by unix-udp socket.
 ** Input: 
 **     sckid: Socket file descriptor
 **     path: File path
 **     buff: Send buffer
 **     sndlen: Send length
 ** Output: NONE
 ** Return: Send number of sent.
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int xdo_usck_udp_send(int sckid, const char *path, const void *buff, size_t sndlen)
{
    int n = 0;
    int	addrlen = 0;
    struct sockaddr_un toaddr;

    memset(&toaddr, 0, sizeof(toaddr));
    
AGAIN:
    toaddr.sun_family = AF_UNIX;
    snprintf(toaddr.sun_path, sizeof(toaddr.sun_path), "%s", path);
    addrlen = strlen(toaddr.sun_path) + sizeof(toaddr.sun_family);

    n = sendto(sckid, buff, sndlen, 0, (struct sockaddr*)&toaddr, addrlen);
    if (n < 0)
    {
        if (EINTR == errno)
        {
            goto AGAIN;
        }
        return -1;
    }

    return n;
}

/******************************************************************************
 ** Name : xdo_usck_udp_recv
 ** Desc : Recv data by unix-udp socket.
 ** Input: 
 **     sckid: Socket file descriptor
 **     rcvlen: Receive length
 ** Output: 
 **     buff: Receive buffer
 **     from: 数据源信息
 ** Return: Receive number of received.
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int xdo_usck_udp_recv(int sckid, void *buff, int rcvlen)
{
    int	len = 0;
    struct sockaddr_un from;

    memset(&from, 0, sizeof(struct sockaddr_un));

    from.sun_family = AF_UNIX;

    return recvfrom(sckid, buff, rcvlen, 0, (struct sockaddr *)&from, (socklen_t *)&len);
}

/******************************************************************************
 ** Name : xdo_fd_is_writable
 ** Desc : Wether file descriptor is writable?
 ** Input: 
 ** Output: NONE
 ** Return: 1:Yes 0:No
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.06.07 #
 ******************************************************************************/
int xdo_fd_is_writable(int fd)
{
    fd_set wset;
    struct timeval tmout;

    FD_ZERO(&wset);
    FD_SET(fd, &wset);

    tmout.tv_sec = 0;
    tmout.tv_usec = 0;
    return select(fd+1, NULL, &wset, NULL, &tmout);
}

/******************************************************************************
 **函数名称: xdo_block_send
 **功    能: 阻塞发送
 **输入参数: 
 **    fd: 文件描述符
 **    addr: 被发送数据的起始地址
 **    size: 发送字节数
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **    等待所有字节发送完成后才返回，除非出现严重错误.
 **作    者: # Qifeng.zou # 2014.06.24 #
 ******************************************************************************/
int xdo_block_send(int fd, const void *addr, size_t size, int timeout_sec)
{
    int ret = 0, n = 0, left = size, off = 0;
    fd_set wrset;
    struct timeval tmout;

    for (;;)
    {
        FD_ZERO(&wrset);
        FD_SET(fd, &wrset);

        tmout.tv_sec = timeout_sec;
        tmout.tv_usec = 0;
        ret = select(fd+1, NULL, &wrset, NULL, &tmout);
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

        n = Writen(fd, (const char *)addr+off, left);
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
 **函数名称: xdo_block_recv
 **功    能: 阻塞接收
 **输入参数: 
 **     fd: 文件描述符
 **     addr: 被发送数据的起始地址
 **     size: 发送字节数
 **     timeout_sec: 阻塞时长(秒)
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **    等待所有字节接收完成后才返回，除非出现严重错误或超时.
 **作    者: # Qifeng.zou # 2014.06.24 #
 ******************************************************************************/
int xdo_block_recv(int fd, void *addr, size_t size, int timeout_sec)
{
    int ret = 0, n = 0, left = size, off = 0;
    fd_set rdset;
    struct timeval tmout;

    for (;;)
    {
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);

        
        tmout.tv_sec = timeout_sec;
        tmout.tv_usec = 0;
        ret = select(fd+1, &rdset, NULL, NULL, &tmout);
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

        n = Readn(fd, (char *)addr+off, left);
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



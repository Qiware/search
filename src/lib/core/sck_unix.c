#include "sck.h"
#include "redo.h"

/******************************************************************************
 **函数名称: unix_udp_creat
 **功    能: 创建UNIX-UDP套接字
 **输入参数:
 **     path: 绑定路径
 **输出参数: NONE
 **返    回: 套接字ID
 **实现描述:
 **     1. 创建套接字
 **     2. 绑定指定路径
 **     3. 设置套接字属性(可重用、非阻塞)
 **注意事项:
 **作    者: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int unix_udp_creat(const char *path)
{
    int fd, len;
    struct sockaddr_un svraddr;

    memset(&svraddr, 0, sizeof(svraddr));

    /* 1. 创建套接字 */
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd <0) {
        return -1;
    }

    Mkdir2(path, 0755);

    unlink(path);

    /* 2. 绑定指定路径 */
    svraddr.sun_family = AF_UNIX;
    snprintf(svraddr.sun_path, sizeof(svraddr.sun_path), "%s", path);

    len = strlen(svraddr.sun_path) + sizeof(svraddr.sun_family);

    if (bind(fd, (struct sockaddr *)&svraddr, len) < 0) {
        return -1;
    }

    fd_set_nonblocking(fd);

    return  fd;
}

/******************************************************************************
 **函数名称: unix_udp_send
 **功    能: 发送数据(UNIX-UDP套接字)
 **输入参数:
 **     fd: 套接字ID
 **     path: 绑定路径
 **     buff: 发送数据
 **     len: 发送长度
 **输出参数: NONE
 **返    回: 发送长度
 **实现描述:
 **注意事项: 因使用的是UNIX-UDP协议, 因此, 要么都成功, 要么都发送失败!
 **作    者: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int unix_udp_send(int fd, const char *path, const void *buff, int len)
{
    int n;
    socklen_t addrlen;
    struct sockaddr_un toaddr;

AGAIN:
    memset(&toaddr, 0, sizeof(toaddr));

    toaddr.sun_family = AF_UNIX;
    snprintf(toaddr.sun_path, sizeof(toaddr.sun_path), "%s", path);
    addrlen = strlen(toaddr.sun_path) + sizeof(toaddr.sun_family);

    n = sendto(fd, buff, len, 0, (struct sockaddr*)&toaddr, addrlen);
    if (n < 0) {
        if (EINTR == errno) {
            goto AGAIN;
        }
        return -1;
    }

    return n;
}

/******************************************************************************
 **函数名称: unix_udp_recv
 **功    能: 接收数据(UNIX-UDP套接字)
 **输入参数:
 **     fd: 套接字ID
 **     len: 接收长度
 **输出参数:
 **     buff: 接收数据
 **返    回: 接收长度
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int unix_udp_recv(int fd, void *buff, int len)
{
    socklen_t addrlen = 0;
    struct sockaddr_un from;

    memset(&from, 0, sizeof(struct sockaddr_un));

    from.sun_family = AF_UNIX;

    return recvfrom(fd, buff, len, 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
}

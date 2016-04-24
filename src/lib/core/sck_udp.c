#include "sck.h"
#include "syscall.h"

/******************************************************************************
 **函数名称: udp_listen
 **功    能: 侦听指定端口
 **输入参数:
 **     port: 端口号
 **输出参数: NONE
 **返    回: 套接字ID(<0:失败)
 **实现描述:
 **     1. 创建套接字
 **     2. 绑定指定端口
 **     3. 侦听指定端口
 **     4. 设置套接字属性(可重用、非阻塞)
 **注意事项:
 **作    者: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int udp_listen(int port)
{
    int fd, opt = 1;
    struct sockaddr_in svraddr;

    /* 1. 创建套接字 */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    /* 2. 绑定指定端口 */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    svraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    svraddr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&svraddr, sizeof(svraddr)) < 0) {
        close(fd);
        return -1;
    }

    /* 3. 设置非阻塞属性 */
    fd_set_nonblocking(fd);

    return fd;
}

#if !defined(__XD_SOCKET_H__)
#define __XD_SOCKET_H__

#include "common.h"

/* IP地址信息 */
typedef struct
{
    int family;                 /* TCP协议(AF_INET/AF_INET6) */
    char ip[IP_ADDR_MAX_LEN];   /* IP地址信息 */
} ipaddr_t;

int tcp_listen(int port);
int tcp_connect(int family, const char *ipaddr, int port);
int tcp_connect_ex(int family, const char *ipaddr, int port, int sec);
int tcp_connect_ex2(int family, const char *ipaddr, int port);
int tcp_block_send(int fd, const void *addr, int len, int timeout);
int tcp_block_recv(int fd, void *addr, int len, int timeout);

int fd_is_writable(int fd);
#define fd_set_nonblocking(fd)     /* 设置fd为非阻塞模式 */\
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

int unix_udp_creat(const char *path);
int unix_udp_send(int sckid, const char *path, const void *buff, int len);
int unix_udp_recv(int sckid, void *buff, int len);

#endif /*__XD_SOCKET_H__*/

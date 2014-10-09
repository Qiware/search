#if !defined(__XDO_SOCKET_H__)
#define __XDO_SOCKET_H__

int tcp_listen(int port);
int tcp_connect(const char *ipaddr, int port);
int tcp_connect_ex(const char *ipaddr, int port, int sec);
int tcp_connect_ex2(const char *ipaddr, int port);
int tcp_block_send(int fd, const void *addr, int len, int timeout);
int tcp_block_recv(int fd, void *addr, int len, int timeout);

int fd_is_writable(int fd);
#define fd_set_nonblocking(fd)     /* 设置fd为非阻塞模式 */\
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

int unix_udp_creat(const char *path);
int unix_udp_send(int sckid, const char *path, const void *buff, int len);
int unix_udp_recv(int sckid, void *buff, int len);

#endif /*__XDO_SOCKET_H__*/

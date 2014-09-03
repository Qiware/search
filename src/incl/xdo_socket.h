#if !defined(__XDO_SOCKET_H__)
#define __XDO_SOCKET_H__

int xdo_tcp_listen(int port);

int xdo_fd_is_writable(int fd);
#define xdo_nonblocking(fd)     /* 设置fd为非阻塞模式 */\
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

int xdo_usck_udp_creat(const char *path);
int xdo_usck_udp_send(int sckid, const char *path, const void *buff, size_t sndlen);
int xdo_usck_udp_recv(int sckid, void *buff, int rcvlen);

int xdo_block_send(int fd, const void *addr, size_t size, int timeout_sec);
int xdo_block_recv(int fd, void *addr, size_t size, int timeout_sec);
#endif /*__XDO_SOCKET_H__*/

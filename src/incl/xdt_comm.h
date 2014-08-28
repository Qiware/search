#if !defined(__XDT_COMM_H__)
#define __XDT_COMM_H__

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
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


typedef int bool;
#define false (0)
#define true  (1)

#define FILE_NAME_MAX_LEN   (256)   /* 文件名最大长度 */
#define FILE_LINE_MAX_LEN   (256)   /* 文件行最大长度 */
#define FILE_PATH_MAX_LEN   (256)   /* 文件路径最大长度 */
#define IP_ADDR_MAX_LEN     (16)    /* IP地址最大长度 */
#define CMD_LINE_MAX_LEN    (1024)  /* 命令行最大长度 */
#define ERR_MSG_MAX_LEN     (1024)  /* 错误信息最大长度 */

#define MD5_SUM_CHK_LEN     (32)    /* Md5校验值长度 */
#define XDR_STR_MAX_LEN     (2048)  /* XDR的最大长度 */

int Readn(int fd, void *buff, int n);
int Writen(int fd, const void *buff, int n);
int Rename(const char *oldpath, const char *newpath);
int Open(const char *fpath, int flags, mode_t mode);
#define Close(fd)  \
{ \
    if(fd > 0) { close(fd), fd = -1; } \
}
#define fClose(fp) {fclose(fp), fp = NULL;}
#define Free(p) {free(p), p=NULL; }

int Sleep(int seconds);
unsigned int Hash(const char *str);
int Random(void);

int Mkdir(const char *dir, mode_t mode);
int Mkdir2(const char *fname, mode_t mode);

int Listen(int port);
int fd_nonblock(int fd);
int fd_is_writable(int fd);
int block_recv(int fd, void *addr, size_t size, int secs);
int block_send(int fd, const void *addr, size_t size, int secs);

int usck_udp_creat(const char *fname);
int usck_udp_send(int sckid, const char *path, const void *buff, size_t bufflen);
int usck_udp_recv(int sckid, void *buff, int rcvlen);

int creat_thread(pthread_t *tid, void *(*process)(void *args), void *args);

/* 文件锁相关 */
int _flock(int fd, int type, int whence, int offset, int len);
int _try_flock(int fd, int type, int whence, int offset, int len);

#define proc_wrlock(fd) _flock(fd, F_WRLCK, SEEK_SET, 0, 0)  /* 写锁(全文件) */
#define proc_rdlock(fd) _flock(fd, F_RDLCK, SEEK_SET, 0, 0)  /* 读锁(全文件) */
#define proc_try_wrlock(fd) _try_flock(fd, F_WRLCK, SEEK_SET, 0, 0) /* 尝试写锁(全文件) */
#define proc_try_rdlock(fd) _try_flock(fd, F_RDLCK, SEEK_SET, 0, 0) /* 尝试读锁(全文件) */
#define proc_unlock(fd) _flock(fd, F_UNLCK, SEEK_SET, 0, 0)  /* 解锁(全文件) */

#define proc_wrlock_ex(fd, idx) _flock(fd, F_WRLCK, SEEK_SET, idx, 1)   /* 写锁(单字节) */
#define proc_rdlock_ex(fd, idx) _flock(fd, F_RDLCK, SEEK_SET, idx, 1)   /* 读锁(单字节) */
#define proc_try_wrlock_ex(fd, idx) _try_flock(fd, F_WRLCK, SEEK_SET, idx, 1) /* 尝试写锁(单字节) */
#define proc_try_rdlock_ex(fd, idx) _try_flock(fd, F_RDLCK, SEEK_SET, idx, 1) /* 尝试读锁(单字节) */
#define proc_unlock_ex(fd, idx) _flock(fd, F_UNLCK, SEEK_SET, idx, 1)   /* 解锁(单字节) */

void sha256_digest(char *str, unsigned int len, unsigned char digest[32]);
#endif /*__XDT_COMM_H__*/

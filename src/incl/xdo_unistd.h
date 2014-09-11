#if !defined(__XDO_UNISTD_H__)
#define __XDO_UNISTD_H__

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

#include "common.h"

#define OPEN_MODE           (0666)  /* 文件权值 */
#define OPEN_FLAGS          (O_CREAT|O_WRONLY|O_APPEND) /* 文件标志 */
#define DIR_MODE            (0777)  /* 目录权值 */

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
int Mkdir(const char *dir, mode_t mode);
int Mkdir2(const char *fname, mode_t mode);

int Random(void);

int proc_is_exist(pid_t pid);
int creat_thread(pthread_t *tid, void *(*process)(void *args), void *args);

#if defined(HAVE_POSIX_MEMALIGN) || defined(HAVE_MEMALIGN)
void *xdo_mem_align(size_t alignment, size_t size);
#else
#define xdo_mem_align(alignment, size) malloc(size)
#endif

void *xdo_shm_creat(int key, size_t size);
#endif /*__XDO_UNISTD_H__*/

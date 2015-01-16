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
int Open(const char *fpath, int flags, mode_t mode);
#define Close(fd)  \
{ \
    if(fd > 0) { close(fd), fd = -1; } \
}
#define fClose(fp) {fclose(fp), fp = NULL;}
#define Free(p) { if (p) { free(p), p = NULL; } }

void Sleep(int sec);
int Mkdir(const char *dir, mode_t mode);
int Mkdir2(const char *fname, mode_t mode);

int Random(void);

int proc_is_exist(pid_t pid);

#if defined(HAVE_POSIX_MEMALIGN)
void *memalign_alloc(size_t alignment, size_t size);
#elif defined(HAVE_MEMALIGN)
#define memalign_alloc(alignment, size) memalign(alignment, size)
#else
#define memalign_alloc(alignment, size) malloc(size)
#endif

void *shm_creat(int key, size_t size);
void *shm_attach(int key, size_t size);
void *shm_creat_and_attach(int key, size_t size);
int bind_cpu(uint16_t id);
int limit_file_num(int max);

/* 取数值上限 */
#define math_ceiling(num, mod) ((num)%(mod)? (num)/(mod)+1 : (num)/(mod))

#endif /*__XDO_UNISTD_H__*/

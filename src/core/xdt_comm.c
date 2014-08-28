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

#include "xdt_comm.h"
#include "sha256.h"
#include "xdt_log.h"

/******************************************************************************
 ** Name : Open
 ** Desc : Recall open system call when errno is EINTR. 
 ** Input: 
 **     fpath: Path of file
 **     flags: Open flags
 **     mode: file mode
 ** Output: NONE
 ** Return: file descriptor
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Open(const char *fpath, int flags, mode_t mode)
{
    int fd = 0;
    
AGAIN:
    fd = open(fpath, flags, mode);
    if (fd < 0)
    {
        if (EINTR == errno)
        {
            goto AGAIN;
        }
        return -1;
    }
    return fd;        
}

/******************************************************************************
 ** Name : Readn
 ** Desc : Readn special number of characters. 
 ** Input: 
 **     fd: file descriptor.
 **     n: special number of characters.
 ** Output:
 **     buff: read buffer
 ** Return: number of read characters
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Readn(int fd, void *buff, int n)
{
    int left = n, len = 0;
    char *ptr = (char *)buff;

    while (left > 0)
    {
        len = read(fd, ptr, left);
        if (len < 0)
        {
            if (EAGAIN == errno)
            {
                return (n - left);
            }
            else if (EINTR == errno)
            {
                continue;
            }
            return -1;
        }
        else if (0 == len)
        {
            break;
        }

        left -= len;
        ptr += len;
    }
    
    return (n - left);
}

/******************************************************************************
 ** Name : Writen
 ** Desc : Writen special number of characters. 
 ** Input: 
 **     fd: file descriptor.
 **     buff: write buffer.
 **     n: special number of characters.
 ** Output:
 ** Return: number of write characters
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Writen(int fd, const void *buff, int n)
{
    int left = n, len = 0;
    const char *ptr = (const char *)buff;

    while (left > 0)
    {
        len = write(fd, ptr, left);
        if (len < 0)
        {
            if (EAGAIN == errno)
            {
                return (n - left);
            }
            else if (EINTR == errno)
            {
                continue;
            }
            return -1;
        }

        left -= len;
        ptr += len;
    }
    
    return n;
}

/******************************************************************************
 ** Name : Sleep
 ** Desc : Sleep special seconds. 
 ** Input: 
 **     secs: special seconds.
 ** Output: NONE
 ** Return: 0-Success !0-Failed
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Sleep(int secs)
{
    int left = 0;

    left = secs;
    do
    {
        left = sleep(left);
    }while (left > 0);

    return 0;
}

/******************************************************************************
 ** Name : Hash
 ** Desc : Generate hash value 
 ** Input: 
 **     str: string
 ** Output: NONE
 ** Return: Hash value
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
unsigned int Hash(const char *str)
{
    const char *p = str;
    unsigned int hash = 5381;

    while (*p)
    {
        hash += (hash << 5) + (*p++);
    }

    return (hash & 0x7FFFFFFF);
}

/******************************************************************************
 ** Name : Random
 ** Desc : Generate random value.
 ** Input: NONE
 ** Output: NONE
 ** Return: Random value
 ** Proc :
 **     1. Get current time
 **     2. Generate a random number
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Random(void)
{
    struct timeval cur_time;

    memset(&cur_time, 0, sizeof(cur_time));

    gettimeofday(&cur_time, NULL);

    return ((random()*cur_time.tv_usec)&0x7FFFFFFF);
}

/******************************************************************************
 ** Name : fd_nonblock
 ** Desc : Set nonblock attribute of sepcial file-descriptor.
 ** Input: 
 **     fd: file descriptor
 ** Output: NONE
 ** Return: 0-Success !0-Failed
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int fd_nonblock(int fd)
{
    int flags = 0;

    flags = fcntl(fd, F_GETFL, 0);
    
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/******************************************************************************
 ** Name : usck_udp_creat
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
int usck_udp_creat(const char *path)
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

    fd_nonblock(fd);

    return  fd;
}

/******************************************************************************
 ** Name : usck_udp_send
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
int usck_udp_send(int sckid, const char *path, const void *buff, size_t sndlen)
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
 ** Name : usck_udp_recv
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
int usck_udp_recv(int sckid, void *buff, int rcvlen)
{
    int	len = 0;
    struct sockaddr_un from;

    memset(&from, 0, sizeof(struct sockaddr_un));

    from.sun_family = AF_UNIX;

    return recvfrom(sckid, buff, rcvlen, 0, (struct sockaddr *)&from, (socklen_t *)&len);
}

/******************************************************************************
 ** Name : Mkdir
 ** Desc : Create directory
 ** Input: 
 **     dir: Special directory
 **     mode: Directory mode
 ** Output: 
 ** Return: 0-Success !0-Failed
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Mkdir(const char *dir, mode_t mode)
{
    int ret = 0, len = 0;
    const char *p = dir;
    struct stat file_stat;
    char part[FILE_PATH_MAX_LEN] = {0};

    memset(&file_stat, 0, sizeof(file_stat));

    ret = stat(dir, &file_stat);
    if (0 == ret)
    {
        if (S_ISDIR(file_stat.st_mode))
        {
            return 0;
        }
        return -1;  /* Exist, but not directory */
    }

    p = strchr(p, '/');
    while (NULL != p)
    {
        len = p - dir + 1;
        memcpy(part, dir, len);

        ret = stat(part, &file_stat);
        if (0 == ret)
        {
            if (S_ISDIR(file_stat.st_mode))
            {
                p++;
                p = strchr(p, '/');
                continue;
            }
            return -1;  /* Exist, but not directory */
        }
        
        ret = mkdir(part, mode);
        if (0 != ret)
        {
            if (EEXIST != errno)
            {
                return -1;
            }
            /* Exist, then continue */
        }
        p++;
        p = strchr(p, '/');
    }

    mkdir(dir, mode);
    return 0;
}


/******************************************************************************
 ** Name : Mkdir2
 ** Desc : Only build directory when filename contain direcotry.
 ** Input: 
 **     fname: file-name which contain directory
 **     mode: Directory mode
 ** Output: 
 ** Return: 0-Success !0-Failed
 ** Proc :
 ** Note :
 **     Ext: fname="/home/svn/etc/lsn.log", will create "/home/svn/etc/"
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Mkdir2(const char *fname, mode_t mode)
{
    const char *p = fname;
    char dir[FILE_PATH_MAX_LEN] = {0};

    p += strlen(fname);

    p = strrchr(fname, '/');
    if (NULL == p)
    {
        return 0;   /* ÔÚµ±Ç°Â·¾¶ÏÂ£¬Òò´Ë²»±Ø´´½¨Â·¾¶ */
    }

    /* È¥³ýÎÄ¼þÃû£¬Ö»±£ÁôÎÄ¼þÂ·¾¶ */
    memcpy(dir, fname, p - fname);

    return Mkdir(dir, mode);
}

/******************************************************************************
 ** Name : creat_thread
 ** Desc : Create thread
 ** Input: 
 **     process: Callback
 **     args: Arguments of callback
 ** Output: 
 **     tid: Thread ID
 ** Return: 0:success !0:failed
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int creat_thread(pthread_t *tid, void *(*process)(void *args), void *args)
{
    int ret = 0;
    pthread_attr_t attr;

    do
    {
        ret = pthread_attr_init(&attr);
        if (0 != ret)
        {
            break;
        }

        ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (0 != ret)
        {
            break;
        }

        ret = pthread_attr_setstacksize(&attr, 0x800000);

        ret = pthread_create(tid, &attr, process, args);
        if (0 != ret)
        {
            if (EINTR == errno)
            {
                pthread_attr_destroy(&attr);
                continue;
            }

            break;
        }
        break;
    }while (1);

    pthread_attr_destroy(&attr);
    return ret;
}

/******************************************************************************
 ** Name : fd_is_writable
 ** Desc : Wether file descriptor is writable?
 ** Input: 
 ** Output: NONE
 ** Return: 1:Yes 0:No
 ** Proc :
 ** Note :
 ** Author: # Qifeng.zou # 2014.06.07 #
 ******************************************************************************/
int fd_is_writable(int fd)
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
 ** Name : Listen
 ** Desc : Listen special port
 ** Input: 
 **     port: Listen port
 ** Output: NONE
 ** Return: Socket fd
 ** Proc : 
 **     1. Create socket
 **     2. Listen port
 **     3. Set socket attribute
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
int Listen(int port)
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

    ret = fd_nonblock(sckid);
    if (ret < 0)
    {
        Close(sckid);
        printf("errmsg:[%d] %s", errno, strerror(errno));
        return -1;
    }

    return sckid;
}

/******************************************************************************
 **函数名称: _try_flock
 **功    能: 尝试加锁(文件锁)
 **输入参数: 
 **     fd: 文件描述符
 **     type: 锁类型(读:F_RDLCK, 写:F_WRLCK, 解锁:F_UNLCK)
 **     whence: 相对位置(SEEK_SET, CURR_SET, END_SET)
 **     start: 其实位置
 **     len: 加锁长度
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     F_SETLK 设置文件锁定的状态。
 **         此时flcok 结构的l_type 值必须是F_RDLCK、F_WRLCK或F_UNLCK。
 **         如果无法建立锁定，则返回-1，错误代码为EACCES 或EAGAIN。
 **作    者: # Qifeng.zou # 2013.09.06 #
 ******************************************************************************/
int _try_flock(int fd, int type, int whence, int start, int len)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = start;
    fl.l_len = len;
    
    return fcntl(fd, F_SETLK, &fl);
}

/******************************************************************************
 **函数名称: _flock
 **功    能: 阻塞加读写锁(文件锁)
 **输入参数: 
 **     fd: 文件描述符
 **     type: 锁类型(读锁:F_RDLCK, 写锁:F_WRLCK, 解锁:F_UNLCK)
 **     whence: 相对位置(SEEK_SET, CURR_SET, END_SET)
 **     offset: 偏移位置
 **     len: 加锁长度
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     1) 如果offset和len都为0时, 表示锁住整个文件.
 **     2) F_SETLKW 与F_SETLK作用相似，
 **         但是无法建立锁定时，此调用会一直等到锁定动作成功为止。
 **         若在等待锁定的过程中被信号中断时，会立即返回-1，错误代码为EINTR。
 **作    者: # Qifeng.zou # 2013.09.06 #
 ******************************************************************************/
int _flock(int fd, int type, int whence, int offset, int len)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = offset;
    fl.l_len = len;
    
    return fcntl(fd, F_SETLKW, &fl);
}

/******************************************************************************
 **函数名称: Rename
 **功    能: 重命名文件
 **输入参数: 
 **     oldpath: 原文件名
 **     newpath: 新文件名
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
int Rename(const char *oldpath, const char *newpath)
{
    int ret = 0;

AGAIN:
    ret = rename(oldpath, newpath);
    if(ret < 0)
    {
        if(EINTR == errno)
        {
            goto AGAIN;
        }
        return -1;
    }
    return 0;
}

/******************************************************************************
 **函数名称: block_send
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
int block_send(int fd, const void *addr, size_t size, int secs)
{
    int ret = 0, n = 0, left = size, off = 0;
    fd_set wrset;
    struct timeval tmout;

    for (;;)
    {
        FD_ZERO(&wrset);
        FD_SET(fd, &wrset);

        tmout.tv_sec = secs;
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
 **函数名称: block_recv
 **功    能: 阻塞接收
 **输入参数: 
 **     fd: 文件描述符
 **     addr: 被发送数据的起始地址
 **     size: 发送字节数
 **     secs: 阻塞时长(秒)
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **    等待所有字节接收完成后才返回，除非出现严重错误或超时.
 **作    者: # Qifeng.zou # 2014.06.24 #
 ******************************************************************************/
int block_recv(int fd, void *addr, size_t size, int secs)
{
    int ret = 0, n = 0, left = size, off = 0;
    fd_set rdset;
    struct timeval tmout;

    for (;;)
    {
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);

        
        tmout.tv_sec = secs;
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

/******************************************************************************
 **函数名称: sha256_digest
 **功    能: 生成SHA256值
 **输入参数: 
 **     str: 原始串
 **     len: Str长度
 **输出参数:
 **     digest: SHA256摘要值
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.08.07 #
 ******************************************************************************/
void sha256_digest(char *str, unsigned int len, unsigned char digest[32])
{
    sha256_t sha256;

    sha256_init(&sha256);
	
    sha256_calculate(&sha256, str, len);

    memcpy(digest, sha256.Value, sizeof(sha256.Value));
}

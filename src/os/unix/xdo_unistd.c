#include <sys/shm.h>
#include <sys/ipc.h>

#include "xdo_unistd.h"

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
int Readn(int32_t fd, void *buff, int n)
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
int Writen(int32_t fd, const void *buff, int n)
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
int Sleep(int32_t secs)
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
        return 0;
    }

    memcpy(dir, fname, p - fname);

    return Mkdir(dir, mode);
}

/******************************************************************************
 **函数名称: proc_is_exist
 **功    能: 检查进程是否存在
 **输入参数: 
 **     pid: 进程ID
 **输出参数: NONE
 **返    回: 1:存在 0:不存在
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
int proc_is_exist(pid_t pid)
{
	char fname[FILE_NAME_MAX_LEN];
	
	snprintf(fname, sizeof(fname), "/proc/%d", pid);

    return (0 == access(fname, 0));
}

/******************************************************************************
 ** Name : creat_thread
 ** Desc : Create thread
 ** Input: 
 **     process: Callback
 **     args: Arguments of callback
 ** Output: 
 **     tid: Thread ID
 ** Return: 0:成功 !0:失败
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
 **函数名称: Rename
 **功    能: 重命名文件
 **输入参数: 
 **     oldpath: 原文件名
 **     newpath: 新文件名
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
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

#if (HAVE_POSIX_MEMALIGN)
/******************************************************************************
 **函数名称: memalign_alloc
 **功    能: 按照指定对齐方式申请内存
 **输入参数: 
 **     alignment: 内存对齐大小
 **     size: 空间SIZE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void *memalign_alloc(size_t alignment, size_t size)
{
	void  *p = NULL;
	
	posix_memalign(&p, alignment, size);
	
	return p;
}
#endif /*HAVE_POSIX_MEMALIGN*/

/******************************************************************************
 **函数名称: xdo_shm_creat
 **功    能: 创建共享内存
 **输入参数: 
 **     key: 共享内存KEY
 **     size: 空间SIZE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
void *xdo_shm_creat(int key, size_t size)
{
    int shmid;
    void *addr;

    /* 1 判断是否已经创建 */
    shmid = shmget(key, 0, 0666);
    if(shmid >= 0)
    {
        return shmat(shmid, NULL, 0);  /* 已创建 */
    }

    /* 2 异常，则退出处理 */
    if(ENOENT != errno)
    {
        return NULL;
    }

    /* 3 创建共享内存 */
    shmid = shmget(key, size, IPC_CREAT|0660);
    if(shmid < 0)
    {
        return NULL;
    }

    /* 4. ATTACH共享内存 */
    addr = (void *)shmat(shmid, NULL, 0);
    if((void *)-1 == addr)
    {
        return NULL;
    }

    return addr;
}

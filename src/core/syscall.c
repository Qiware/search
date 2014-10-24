#define _GNU_SOURCE
#include <sys/shm.h>
#include <sys/ipc.h>
#include <pthread.h>

#include "syscall.h"

/******************************************************************************
 **函数名称: Open
 **功    能: 打开指定文件
 **输入参数: 
 **     fname: 文件名
 **     flags: 打开标志
 **         O_RDONLY: 只读
 **         O_WRONLY: 只写
 **         O_RDWR: 可读可写
 **         O_APPEND: 追加方式(使用了此标识，则函数lseek()将失效)
 **         O_CREAT: 创建文件(如果已存在，则无任何效果)
 **         O_EXCL: 配合O_CREAT一起使用，如果文件已存在，将返回错误.
 **         O_DSYNC: 每次写入操作需要等待物理IO返回成功, 但不等待文件属性被更新
 **         O_SYNC: 每次写入操作需要等待物理IO返回成功, 且需等待文件属性被更新
 **         O_NONBLOCK: 非阻塞方式(详细信息请参考文档)
 **         O_TRUNC: 截断文件(文件存在，且为普通文件，并且使用了O_RDWR或O_WRONLY)
 **         ...: 其他请参考http://linux.die.net/man/3/open
 **     mode: 文件权限
 **输出参数: NONE
 **返    回: 文件描述符
 **实现描述: 
 **     http://linux.die.net/man/3/open
 **注意事项: 
 **     示例: fd = Open("./abc/t.data", O_CREAT|O_APPEND, 0666);
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Open(const char *fname, int flags, mode_t mode)
{
    int fd;
    
AGAIN:
    fd = open(fname, flags, mode);
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
 **函数名称: Readn
 **功    能: 读取指定字节数
 **输入参数: 
 **     fd: 文件描述符
 **     n: 希望读取的字节数
 **输出参数:
 **     buff: 接收缓存
 **返    回: 真正读取的字节数
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
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
 **函数名称: Writen
 **功    能: 写入指定字节数
 **输入参数: 
 **     fd: 文件描述符
 **     n: 希望写入的字节数
 **输出参数:
 **     buff: 数据地址
 **返    回: 真正写入的字节数
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
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
 **函数名称: Sleep
 **功    能: 睡眠指定时间
 **输入参数: 
 **     sec: 秒
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
void Sleep(int sec)
{
    int left = sec;

    do
    {
        left = sleep(left);
    }while (left > 0);
}

/******************************************************************************
 **函数名称: Random
 **功    能: 产生随机数
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 随机数
 **实现描述: 
 **     1. 获取当前时间
 **     2. 产生随机数
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Random(void)
{
    struct timeval ctm;

    memset(&ctm, 0, sizeof(ctm));

    gettimeofday(&ctm, NULL);

    return ((random() * ctm.tv_usec) & 0x7FFFFFFF);
}

/******************************************************************************
 **函数名称: Mkdir
 **功    能: 新建目录
 **输入参数:
 **     dir: 目录路径
 **     mode: 目录权限
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断目录是否存在
 **     2. 递归创建目录结构
 **扩展知识: 
 **     S_ISLNK(st_mode): 是否是连接
 **     S_ISREG(st_mode): 是否是常规文件
 **     S_ISDIR(st_mode): 是否是目录
 **     S_ISCHR(st_mode): 是否是字符设备
 **     S_ISBLK(st_mode): 是否是块设备
 **     S_ISFIFO(st_mode): 是否是FIFO文件
 **     S_ISSOCK(st_mode): 是否是SOCKET文件
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Mkdir(const char *dir, mode_t mode)
{
    int len;
    const char *p = dir;
    struct stat st;
    char part[FILE_PATH_MAX_LEN];

    /* 1. 判断目录是否存在 */
    if (0 == stat(dir, &st))
    {
        return !S_ISDIR(st.st_mode); /* 为目录, 则成功, 否则失败 */
    }

    /* 2. 递归创建目录结构 */
    p = strchr(p, '/');
    while (NULL != p)
    {
        memset(part, 0, sizeof(part));

        len = p - dir + 1;
        memcpy(part, dir, len);

        if (0 == stat(part, &st))
        {
            if (S_ISDIR(st.st_mode))
            {
                p++;
                p = strchr(p, '/');
                continue;
            }
            return -1;  /* Exist, but not directory */
        }
        
        if (0 != mkdir(part, mode))
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
 **函数名称: Mkdir2
 **功    能: 构建文件路径中的目录
 **输入参数:
 **     fname: 文件路径
 **     mode: 目录权限
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     如果fname为/home/svn/etc/lsn.log, 则会构建目录/home/svn/etc/
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Mkdir2(const char *fname, mode_t mode)
{
    const char *p;
    char dir[FILE_PATH_MAX_LEN];

    memset(dir, 0, sizeof(dir));

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
    int ret;

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
 **函数名称: shm_creat
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
void *shm_creat(int key, size_t size)
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

/******************************************************************************
 **函数名称: System
 **功    能: 执行系统命令
 **输入参数: 
 **     cmd: Shell命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
int System(const char *cmd)
{ 
    int status;
    
    status = system(cmd);
    if(-1 == status)
    {
        return -1;
    }

    if(WIFEXITED(status))
    {
        if(0 == WEXITSTATUS(status))
        {
            return WEXITSTATUS(status);
        }
        else
        {
            return WEXITSTATUS(status);
        }
    }

    return WEXITSTATUS(status);
}

/******************************************************************************
 **函数名称: bind_cpu
 **功    能: 绑定CPU
 **输入参数: 
 **     num: 指定CPU编号(从0开始计数)
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **     如果id大于最大cpu编号，将id %= cpu_num重新计算id值.
 **作    者: # Qifeng.zou # 2014.10.24 #
 ******************************************************************************/
int bind_cpu(uint16_t id)
{
    cpu_set_t cpuset;

    if ((sysconf(_SC_NPROCESSORS_CONF) - id) <= 0)
    {
        id %= sysconf(_SC_NPROCESSORS_CONF);
    }

    CPU_ZERO(&cpuset);
    CPU_SET(id, &cpuset);

    return pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

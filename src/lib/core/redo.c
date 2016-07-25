#include "redo.h"
#include "mem_ref.h"

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
    if (fd < 0) {
        if (EINTR == errno) {
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
ssize_t Readn(int fd, void *buff, size_t n)
{
    int left = n, len = 0;
    char *ptr = (char *)buff;

    while (left > 0) {
        len = read(fd, ptr, left);
        if (len < 0) {
            if (EAGAIN == errno) {
                return (n - left);
            }
            else if (EINTR == errno) {
                continue;
            }
            return -1;
        }
        else if (0 == len) {
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
 **     buff: 数据地址
 **     n: 希望写入的字节数
 **输出参数:
 **返    回: 真正写入的字节数
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
ssize_t Writen(int fd, const void *buff, size_t n)
{
    ssize_t len;
    int left = n;
    const char *ptr = (const char *)buff;

    while (left > 0) {
        len = write(fd, ptr, left);
        if (len < 0) {
            if (EAGAIN == errno) {
                return (n - left);
            }
            else if (EINTR == errno) {
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

    do {
        left = sleep(left);
    } while(left > 0);
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
    if (0 == stat(dir, &st)) {
        return !S_ISDIR(st.st_mode); /* 为目录, 则成功, 否则失败 */
    }

    /* 2. 递归创建目录结构 */
    p = strchr(p, '/');
    while (NULL != p) {
        memset(part, 0, sizeof(part));

        len = p - dir + 1;
        memcpy(part, dir, len);

        if (0 == stat(part, &st)) {
            if (S_ISDIR(st.st_mode)) {
                p++;
                p = strchr(p, '/');
                continue;
            }
            return -1;  /* Exist, but not directory */
        }

        if (0 != mkdir(part, mode)) {
            if (EEXIST != errno) {
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
 **注意事项: 如果fname为/home/svn/etc/lsn.log, 则会构建目录/home/svn/etc/
 **作    者: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
int Mkdir2(const char *fname, mode_t mode)
{
    const char *p;
    char dir[FILE_PATH_MAX_LEN];

    memset(dir, 0, sizeof(dir));

    p = strrchr(fname, '/');
    if (NULL == p) {
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
 **实现描述: 通过判断目录/proc/pid是否存在, 从而判断进程是否存在.
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
bool proc_is_exist(pid_t pid)
{
	char fname[FILE_NAME_MAX_LEN];
	
	snprintf(fname, sizeof(fname), "/proc/%d", pid);

    return (0 == access(fname, 0));
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
	void *p;

	if (posix_memalign(&p, alignment, size)) {
        return NULL;
    }

	return p;
}
#endif /*HAVE_POSIX_MEMALIGN*/

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
    if (-1 == status) {
        return -1;
    }

    if (WIFEXITED(status)) {
        if (0 == WEXITSTATUS(status)) {
            return WEXITSTATUS(status);
        }
        else {
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
 **注意事项: 如果id大于最大cpu编号，将id %= cpu_num重新计算id值.
 **作    者: # Qifeng.zou # 2014.10.24 #
 ******************************************************************************/
int bind_cpu(uint16_t id)
{
    cpu_set_t cpuset;

    if ((sysconf(_SC_NPROCESSORS_CONF) - id) <= 0) {
        id %= sysconf(_SC_NPROCESSORS_CONF);
    }

    CPU_ZERO(&cpuset);
    CPU_SET(id, &cpuset);

    return pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

/******************************************************************************
 **函数名称: set_fd_limit
 **功    能: 修改进程最多可打开的文件数目
 **输入参数:
 **     max: 进程最多可打开的文件数目
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     只有root权限才可以修改硬限制
 **     1) rlim_cur: 软限制
 **     2) rlim_max: 硬限制
 **其他说明:
 **     1) 任何进程可以将软限制改为小于或等于硬限制
 **     2) 任何进程都可以将硬限制降低，但普通用户降低了就无法提高，该值必须等于或大于软限制
 **     3) 只有超级用户可以提高硬限制
 **作    者: # Qifeng.zou # 2014.11.01 #
 ******************************************************************************/
int set_fd_limit(int max)
{
    struct rlimit limit;

#if defined(__MEM_LEAK_CHECK__)
    getrlimit(RLIMIT_NOFILE, &limit);

    limit.rlim_cur = limit.rlim_max;

    return setrlimit(RLIMIT_NOFILE, &limit);
#else /*!__MEM_LEAK_CHECK__*/
    limit.rlim_cur = max;
    limit.rlim_max = max;

    return setrlimit(RLIMIT_NOFILE, &limit);
#endif /*!__MEM_LEAK_CHECK__*/
}

/******************************************************************************
 **函数名称: mem_alloc
 **功    能: 默认分配内存
 **输入参数:
 **     pool: 内存池
 **     size: 申请SZ
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述:
 **注意事项:
 **     请尽量不要使用此函数申请空间, 否则将造成内存碎片. 但在测试是否存在内存
 **     泄露的情况时, 可以使用此接口结合VALGRIND等测试工具, 可快速的定位到内存泄
 **     露的代码.
 **作    者: # Qifeng.zou # 2015.02.06 #
 ******************************************************************************/
void *mem_alloc(void *pool, size_t size)
{
    return (void *)calloc(1, size);
}

/******************************************************************************
 **函数名称: mem_dealloc
 **功    能: 默认释放内存
 **输入参数:
 **     pool: 内存池
 **     p: 释放地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.02.06 #
 ******************************************************************************/
void mem_dealloc(void *pool, void *p)
{
    free(p);
}

/******************************************************************************
 **函数名称: local_time
 **功    能: 获取当前时间
 **输入参数:
 **     timep: 当前秒(从1970年1月1日开始计时)
 **输出参数:
 **     result: 当前时间
 **返    回: 当前时间
 **实现描述:
 **     公历闰年的简单计算方法(符合以下条件之一的年份即为闰年)
 **     1.能被4整除而不能被100整除.
 **     2.能被400整除.
 **     详情如下:
 **       闰年(leap year),指在公历(格里历)或夏历中有闰日的年份,以及在中国旧历农历
 **     中有闰月的年份.
 **       地球绕太阳运行周期为365天5小时48分46秒(合365.24219天),即一回归年(tropical year).
 **     公历的平年只有365日,比回归年短约0.2422日,每四年累积约一天,把这一天加于2月
 **     末(2月29日),使当年的历年长度为366日,这一年就为闰年. 按照每四年一个闰年计算,
 **     平均每年就要多算出0.0078天,经过四百年就会多出大约3天来,因此,每四百年中要减少
 **     三个闰年.所以规定公历年份是整百数的,必须是400的倍数才是闰年,不是400的倍数的
 **     就是平年.比如,1700年、1800年和1900年为平年,2000年为闰年.闰年的计算,归结起来
 **     就是通常说的：四年一闰；百年不闰,四百年再闰；千年不闰,四千年再闰；万年不闰,
 **     五十万年再闰.
 **注意事项:
 **     1. 此函数相对localtime()而言, 是线程安全的
 **     2. 此函数相对localtime_r()而言, 无加锁、拥有更高性能!
 **     struct tm {
 **        int tm_sec;    -- seconds
 **        int tm_min;    -- minutes
 **        int tm_hour;   -- hours
 **        int tm_mday;   -- day of the month
 **        int tm_mon;    -- month
 **        int tm_year;   -- year
 **        int tm_wday;   -- day of the week
 **        int tm_yday;   -- day in the year
 **        int tm_isdst;  -- daylight saving time(夏令制)
 **     };
 **作    者: # Qifeng.zou # 2015.04.04 #
 ******************************************************************************/
#define ONE_YEAR_HOURS  (8760)  /* 一年小时数 (365 * 24) */
#define EIGHT_EAST_AREA_TM_DIFF_SEC     (28800)     /* 东八区时差: (8 * 60 * 60) */
struct tm *local_time(const time_t *timep, struct tm *result)
{
    time_t time = *timep;
    long int pass_4year_num, n32_hpery;
    static const int mon_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; /* 各月天数(非闰年) */

    /* > 计算时差: 北京时间处在东八区 */
    time += EIGHT_EAST_AREA_TM_DIFF_SEC;
    result->tm_isdst = 0;

    /* 取秒时间 */
    result->tm_sec = (int)(time % 60);

    time /= 60; /* 总分钟数 */

    /* 取分钟时间 */
    result->tm_min = (int)(time % 60);

    time /= 60; /* 总小时数 */

    /* 计算星期几: 1970.01.01是星期四 */
    result->tm_wday = (int)(time/24 + 4) % 7;

    /* 取过去多少个四年，每四年有(3 * 365 + 366) * 24 = 35064 小时
     * 注：假设每4年出现一次闰年，不过再后面会进行校正处理 */
    pass_4year_num = ((unsigned int)time / 35064);

    /* 计算年份 */
    result->tm_year = (pass_4year_num << 2) + 70;

    /* 四年中剩下的小时数 */
    time %= 35064;

    /* 计算在这一年的第几天 */
    result->tm_yday = (time / 24) % 365;

    /* 校正闰年影响的年份，计算一年中剩下的小时数 */
    for (;;) {
        /* 一年的小时数 */
        n32_hpery = ONE_YEAR_HOURS;

        /* 判断闰年 */
        if (0 == (result->tm_year & 3)) {
            /* 是闰年, 一年则多24小时, 即一天 */
            n32_hpery += 24;
        }

        if (time < n32_hpery) {
            break;
        }

        result->tm_year++;
        time -= n32_hpery;
    }

    /* 小时数 */
    result->tm_hour = (int)(time % 24);

    /* 一年中剩下的天数 */
    time /= 24;

    //假定为闰年
    time++;

    //校正润年的误差 计算月份 日期
    if (0 == (result->tm_year & 3)) {
        if (time > 60) {
            time--;
        }
        else {
            if (60 == time) {
                result->tm_mon = 1;
                result->tm_mday = 29;
                return result;
            }
        }
    }

    //计算月日
    for (result->tm_mon = 0; mon_days[result->tm_mon] < time; ++result->tm_mon) {
        time -= mon_days[result->tm_mon];
    }

    result->tm_mday = (int)(time);
    return result;
}

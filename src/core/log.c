/*******************************************************************************
 * 模块: 异步日志模块 - 客户端代码
 * 说明: 
 **     跟踪日志: 考虑性能要求, 跟踪日志首先存储在缓冲区中, 当缓存区中的日志达到
 **               一定量、或超过一定时间未同步、或出现错误级别日志时, 
 **               则将跟踪日志同步到日志文件中.
 **     错误日志: 因错误日志是很少出现的, 为了使系统管理人员第一时间知道系统错误,
 **               因此错误日志是实时打印到日志文件中
 * 注意: 
 *      跟踪日志: 不同的"进程"和"线程"是不能使用的同名的日志文件
 * 作者: # Qifeng.zou # 2013.11.07 #
 ******************************************************************************/
#include "log.h"
#include "common.h"

/* 宏定义 */
/* 共享内存 */
static void *g_alog_shm_addr = (void *)-1;  /* 共享内存地址 */

#define AlogGetShmAddr() (g_alog_shm_addr)              /* 获取共享内存地址 */
#define AlogSetShmAddr(addr) (g_alog_shm_addr = (addr)) /* 设置共享内存地址 */
#define AlogIsShmAddrValid()    /* 判断共享内存地址是否合法 */ \
    ((NULL != g_alog_shm_addr) && ((void *)-1 != g_alog_shm_addr))

/* 文件锁 */
static int g_alog_lock_fd = -1;             /* 文件锁FD */

#define alog_get_lock_fd() (g_alog_lock_fd)             /* 获取文件锁FD */
#define alog_set_lock_fd(fd) (g_alog_lock_fd = (fd))    /* 设置文件锁FD */
#define alog_is_lock_fd_valid() (g_alog_lock_fd >= 0)   /* 判断文件锁FD是否合法 */
#define alog_fcache_wrlock(file) fbit_wrlock(g_alog_lock_fd, (file)->idx+1)  /* 加缓存写锁 */
#define alog_fcache_rdlock(file) fbit_rdlock(g_alog_lock_fd, (file)->idx+1)  /* 加缓存写锁 */
#define alog_fcache_unlock(file) fbit_unlock(g_alog_lock_fd, (file)->idx+1)  /* 解缓存锁 */
#define alog_fcache_all_wrlock() file_wrlock(g_alog_lock_fd)  /* 缓存加写锁(整个都加锁) */
#define alog_fcache_all_unlock() file_unlock(g_alog_lock_fd)/* 缓存解锁锁(整个都解锁) */

static size_t g_alog_max_size = ALOG_FILE_MAX_SIZE;
#define AlogGetMaxSize() (g_alog_max_size)    /* 获取日志大小 */
#define AlogSetMaxSize(size) (g_alog_max_size = (size))
#define AlogIsTooLarge(size) ((size) >= g_alog_max_size)

/* 线程同步数据 */
static pthread_key_t g_alog_mthrd_key;      /* 线程特定数据KEY */
static bool g_trclog_mthrd_flag = false;    /* 跟踪日志-多线程标识 */
static bool g_errlog_mthrd_flag = false;    /* 错误日志-多线程标识 */
static pthread_mutex_t g_trclog_mutex = PTHREAD_MUTEX_INITIALIZER;  /* 跟踪日志设置-线程互斥锁 */
static pthread_mutex_t g_errlog_mutex = PTHREAD_MUTEX_INITIALIZER;  /* 错误日志设置-线程互斥锁 */

#define alog_get_mthrd_flag() (g_trclog_mthrd_flag)
#define alog_set_mthrd_flag() (g_trclog_mthrd_flag = true)
#define alog_reset_mthrd_flag() (g_trclog_mthrd_flag = false)

/* 跟踪日志对象和配置信息锁 */
#define alog_trclog_lock() pthread_mutex_lock(&g_trclog_mutex)
#define alog_trclog_trylock() pthread_mutex_trylock(&g_trclog_mutex)
#define alog_trclog_unlock() pthread_mutex_unlock(&g_trclog_mutex)

/* 错误日志对象锁 */
#define alog_errlog_lock() pthread_mutex_lock(&g_errlog_mutex)
#define alog_errlog_trylock() pthread_mutex_trylock(&g_errlog_mutex)
#define alog_errlog_unlock() pthread_mutex_unlock(&g_errlog_mutex)

/* 日志级别设置 */
static alog_level_e g_AlogLevel = LOG_LEVEL_DEBUG;
#define AlogGetLevel() (g_AlogLevel)
void alog_set_level(alog_level_e level) { g_AlogLevel = level; }

/* 日志缓存数据空间大小 */
static const size_t g_AlogDataSize =  (ALOG_FILE_CACHE_SIZE - sizeof(alog_file_t));
#define AlogGetDataSize() (g_AlogDataSize)

#define AlogHash(path) (Hash(path)%ALOG_FILE_MAX_NUM)   /* 异步日志哈希 */
#define AlogIsErrLevel(level) (LOG_LEVEL_ERROR == (level))

/* 函数声明 */
static alog_cycle_t *alog_get_cycle(int level);

static int alog_errlog_init(alog_cycle_t *cycle, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *curr_time);
static int alog_errlog_set_path(const char *path);
static int alog_errlog_write(alog_cycle_t *cycle, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *curr_time);

static int alog_trclog_init(alog_cycle_t *cycle, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *curr_time);
static int _alog_trclog_init(void);
static int alog_trclog_set_path(const char *path);
static alog_file_t *alog_trclog_creat(void *addr, const char *path);
static void alog_trclog_release(alog_file_t *file);
static int alog_trclog_write(alog_cycle_t *cycle, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *curr_time);
static int alog_trclog_print_dump(char *addr, const void *dump, int dumplen);
static int alog_trclog_sync_ext(alog_cycle_t *cycle);

static void _alog_errlog_write(int fd, pid_t pid, int level,
        const char *dump, int dumplen, const char *msg, const struct timeb *curr_time);
static void alog_errlog_print_dump(sw_int_t fd, const void *dump, int dumplen);

static int alog_rename(const alog_file_t *file, const struct timeb *time);
static size_t alog_sync(alog_file_t *file, int *fd);

/* 全局变量 */
static alog_cycle_t g_alog_errlog = {-1, NULL, -1, alog_errlog_init}; /* 默认错误日志对象 */
static alog_cycle_t g_alog_trclog = {-1, NULL, -1, alog_trclog_init}; /* 默认跟踪日志对象 */

#define alog_default_errlog() (&g_alog_errlog)
#define alog_default_trclog() (&g_alog_trclog)

/* 日志级别提示符 */
static const char *g_AlogLevelStr[LOG_LEVEL_TOTAL] = 
{
    "D", "I", "W", "ERR", "O"
};

#define AlogGetLevelStr(level) /* 获取对应级别的提示字串 */ \
    ((level <= LOG_LEVEL_UNKNOWN)? \
        g_AlogLevelStr[level] : g_AlogLevelStr[LOG_LEVEL_UNKNOWN])

/* 是否强制写(注意: 系数必须小于或等于0.8，否则可能出现严重问题) */
static const int g_AlogSyncSize = 0.8 * ALOG_FILE_CACHE_SIZE;
#define AlogIsForceSync(file) (((file)->in_offset - (file)->out_offset) > g_AlogSyncSize)

/* 判断文件缓存相关进程ID的状态 */
#if defined(__ALOG_MULT_SHARE__)
static int alog_file_add_pid(alog_file_t *file, pid_t pid);
static bool alog_file_check_pid(const alog_file_t *file, pid_t pid);
static bool alog_file_has_others(alog_file_t *file, pid_t pid);
#else /*__ALOG_MULT_SHARE__*/
#define alog_file_add_pid(file, _pid) ((file)->pid = (_pid))
#define alog_file_check_pid(file, _pid) ((_pid) == (file)->pid)
#define alog_file_has_others(file, _pid) ((_pid) != (file)->pid)
#endif /*__ALOG_MULT_SHARE__*/

/******************************************************************************
 **函数名称: log_core
 **功    能: 日志核心调用
 **输入参数: 
 **     level: 日志级别(LOG_LEVEL_DEBUG ~ LOG_LEVEL_ERROR)
 **     fname: 文件名
 **     lineno: 文件行号
 **     dump: 需打印的内存地址
 **     dumplen: 需打印的内存长度
 **     fmt: 格式化输出
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     1. 判断日志级别
 **     2. 获取系统时间
 **     3. 组合日志信息
 **     4. 日志同步处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.24 #
 ******************************************************************************/
void log_core(int level, const char *fname, int lineno,
        const void *dump, int dumplen, const char *fmt, ...)
{
    int len = 0;
    va_list args;
    struct timeb curr_time;
    alog_cycle_t *cycle = NULL;
    char msg[ALOG_MSG_MAX_LEN] = {0};

    memset(&curr_time, 0, sizeof(curr_time));

    if(level < AlogGetLevel())
    {
        return;
    }

    cycle = alog_get_cycle(level);
    if(NULL == cycle)
    {
        alog_exception("Get log cycle failed!");
        return;
    }

    va_start(args, fmt);
    if(NULL != fname)
    {
        len = snprintf(msg, sizeof(msg), "[%s][%d] ", fname, lineno);
        vsnprintf(msg+len, sizeof(msg)-len, fmt, args);
    }
    else
    {
        vsnprintf(msg, sizeof(msg), fmt, args);
    }
    
    va_end(args);

    ftime(&curr_time);
    
    cycle->action(cycle, level, dump, dumplen, msg, &curr_time);
}

/******************************************************************************
 **函数名称: alog_get_cycle
 **功    能: 获取日志对象
 **输入参数: 
 **     level: 日志级别
 **输出参数: NONE
 **返    回: 日志周期
 **实现描述: 
 **注意事项: 
 **     1. 一个进程中的所有线程的错误日志都打印到一个日志中
 **     2. 而跟踪日志可以视具体的设置而定
 **作    者: # Qifeng.zou # 2013.11.04 #
 ******************************************************************************/
static alog_cycle_t *alog_get_cycle(int level)
{
    alog_cycle_t *cycle = NULL;
    
    if(AlogIsErrLevel(level))
    {
        return alog_default_errlog();
    }
    else if(!alog_get_mthrd_flag())
    {
        return alog_default_trclog();
    }

    cycle = (alog_cycle_t *)pthread_getspecific(g_alog_mthrd_key);
    if(NULL == cycle)
    {
        return NULL;
    }

    return cycle;
}

/******************************************************************************
 **函数名称: alog_set_path
 **功    能: 设置日志路径
 **输入参数: 
 **     type: 参数类型(ALOG_PARAM_ERR or ALOG_PARAM_TRC)
 **     path: 文件名(绝对路径)
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **     请使用绝对路径，以免因为服务进程在其他目录，造成同步时路径异常
 **作    者: # Qifeng.zou # 2013.10.29 #
 ******************************************************************************/
int alog_set_path(int type, const char *path)
{
    int ret = -1;
    
    SetWorkDir();
    
    if(ALOG_PARAM_ERR == type)      /* 错误日志路径 */
    {
        ret = alog_errlog_set_path(path);
        if(ret < 0)
        {
            log_error("Set error path failed! path:[%s]", path);
            return -1;
        }
        return 0;
    }
    else if(ALOG_PARAM_TRC == type) /* 跟踪日志路径 */
    {
        ret = alog_trclog_set_path(path);
        if(ret < 0)
        {
            log_error("Set trace path failed! path:[%s]", path);
            return -1;
        }
        return 0;
    }
    
    return -1;
}

/******************************************************************************
 **函数名称: alog_set_max_size
 **功    能: 设置日志尺寸的最大值
 **输入参数: 
 **     size: 日志尺寸
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.29 #
 ******************************************************************************/
void alog_set_max_size(size_t size)
{
    alog_trclog_lock();
    
    AlogSetMaxSize(size);

    alog_trclog_unlock();
}
/******************************************************************************
 **函数名称: alog_errlog_set_path
 **功    能: 设置错误日志的路径
 **输入参数: 
 **     path: 日志绝对路径
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     如果在此申请了空间，则必须设置相关联的初始数据
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int alog_errlog_set_path(const char *path)
{
    pid_t pid = getpid();
    alog_file_t *file = NULL;
    alog_cycle_t *cycle = alog_default_errlog();

    alog_errlog_lock();

    file = cycle->file? cycle->file : NULL;
    if(NULL != file)
    {
        snprintf(file->path, sizeof(file->path), "%s", path);

        alog_file_add_pid(file, pid);
        Close(cycle->fd);
        
        cycle->pid = pid;
        cycle->action = alog_errlog_init;

        alog_errlog_unlock();
        return 0;
    }

    /* 申请空间 并设置相关初始数据 */
    file = (alog_file_t *)calloc(1, sizeof(alog_file_t));
    if(NULL == file)
    {
        alog_errlog_unlock();
        alog_exception("errmsg:[%d]%s", errno, strerror(errno));
        return -1;
    }

    alog_file_add_pid(file, pid);
    ftime(&file->sync_time);
    snprintf(file->path, sizeof(file->path), "%s", path);

    cycle->file = file;
    cycle->pid = pid;
    Close(cycle->fd);
    cycle->action = alog_errlog_init;

    alog_errlog_unlock();
    return 0;
}

/******************************************************************************
 **函数名称: alog_trclog_set_path
 **功    能: 设置非错误级别日志路径
 **输入参数: 
 **     path: 日志绝对路径
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     请勿调用错误日志函数 - 小心死锁!
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int alog_trclog_set_path(const char *path)
{
    pid_t pid = getpid();
    int ret = 0, shmid = 0;
    alog_file_t *file = NULL;
    alog_cycle_t *cycle = NULL;
    void *addr = NULL;

    alog_trclog_lock();

    ret = _alog_trclog_init();
    if(ret < 0)
    {
        alog_trclog_unlock();
        alog_exception("Initialize trace log failed!");
        return -1;
    }

    addr = AlogGetShmAddr();
            
    /* 1. 判断是否创建线程特有数据 */
    if(!alog_get_mthrd_flag())
    {
        ret = pthread_key_create(&g_alog_mthrd_key, free);
        if(0 != ret)
        {
            alog_trclog_unlock();
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }

        /* 为各线程日志分配对象空间 */
        cycle = (alog_cycle_t *)calloc(1, sizeof(alog_cycle_t));
        if(NULL == cycle)
        {
            alog_trclog_unlock();
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }

        cycle->fd = ALOG_INVALID_FD;
        cycle->pid = ALOG_INVALID_PID;

        /* 创建跟踪日志 - 注意: 日志名可能被改变，之后的路径请使用file->path */
        cycle->file = alog_trclog_creat(addr, path);
        if(NULL == cycle->file)
        {
            free(cycle);
            alog_trclog_unlock();
            alog_exception("Create [%s] failed!", path);
            return -1;
        }

        ret = pthread_setspecific(g_alog_mthrd_key, (void *)cycle);
        if(0 != ret)
        {
            alog_trclog_release(cycle->file);
            free(cycle);
            alog_trclog_unlock();
            alog_exception("errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
            return -1;
        }

        cycle->pid = pid;
        cycle->action = alog_trclog_write;

        alog_set_mthrd_flag();
        alog_trclog_unlock();
        return 0;
    }

    /* 2. 更改路径信息 */
    cycle = alog_get_cycle(LOG_LEVEL_DEBUG);
    if(NULL == cycle)
    {
        alog_trclog_unlock();
        alog_exception("Get alog cycle failed!");
        return -1;
    }

    if(NULL != cycle->file)
    {
        if(!alog_file_has_others(cycle->file, pid))
        {
            alog_trclog_release(cycle->file);
        }
        cycle->file = NULL;
    }
    
    /* 创建跟踪日志 - 注意: 日志名可能被改变，之后的路径请使用file->path */
    cycle->file = alog_trclog_creat(addr, path);
    if(NULL == cycle->file)
    {
        free(cycle);
        alog_trclog_unlock();
        alog_exception("Alloc log file failed!");
        return -1;
    }
    alog_trclog_unlock();

    return 0;
}

/******************************************************************************
 **函数名称: alog_errlog_init
 **功    能: 初始化错误级别日志信息
 **输入参数: 
 **     cycle: 日志对象
 **     level: 日志级别
 **     dump: 打印的内存
 **     dumplen: 打印的内存长度
 **     msg: 日志内容
 **     curr_time: 当前时间
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     注意: 错误日志中绝对不能调用调试日志函数，小心死锁!
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int alog_errlog_init(alog_cycle_t *cycle, int level,
    const void *dump, int dumplen, const char *msg, const struct timeb *curr_time)
{
    pid_t pid = getpid();
    int ret = 0, fd = 0;
    alog_file_t *file = NULL;
    char path[FILE_PATH_MAX_LEN] = {0};

    alog_errlog_lock();

    SetWorkDir();
    
    if(NULL == cycle->file)
    {
        cycle->file = (alog_file_t *)calloc(1, sizeof(alog_file_t));
        if(NULL == cycle->file)
        {
            alog_errlog_unlock();
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }

        file = cycle->file;

        AlogErrlogDefPath(file->path, sizeof(file->path));
        Mkdir2(file->path, ALOG_DIR_MODE);
    }
    else
    {
        file = cycle->file;
    }

    if(cycle->fd < 0)
    {
        /* 打开错误日志 */
        cycle->fd = Open(file->path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
        if(cycle->fd < 0)
        {
            alog_errlog_unlock();
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
    }

    alog_file_add_pid(file, pid);

    cycle->pid = pid;
    cycle->action = alog_errlog_write;
    
    /* 执行日志处理 */
    cycle->action(cycle, level, dump, dumplen, msg, curr_time);

    alog_errlog_unlock();
    return 0;
}

/******************************************************************************
 **函数名称: alog_errlog_write
 **功    能: 写入错误级别日志信息
 **输入参数: 
 **     cycle: 日志对象
 **     level: 日志级别(此接口中只能为LOG_LEVEL_ERROR)
 **     msg: 日志内容
 **     curr_time: 当前时间
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 打开文件
 **     2. 锁文件，并计算文件长度
 **     3. 将信息写入文件，并关闭
 **     4. 重命名日志文件
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int alog_errlog_write(alog_cycle_t *cycle, int level,
    const void *dump, int dumplen, const char *msg, const struct timeb *curr_time)
{
    int ret = 0;
    size_t fsize = 0;
    struct stat buff;
    alog_file_t *file = cycle->file;
    alog_cycle_t *trclog = NULL;

    memset(&buff, 0, sizeof(buff));

    /* 判断是否新建日志文件 */
    ret = lstat(file->path, &buff);
    if(0 != ret)
    {
        if(ENOENT != errno)
        {
            Close(cycle->fd);
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }

        Close(cycle->fd);
    }

    if(cycle->fd < 0)
    {
        /* 重新创建文件 */
        cycle->fd = Open(file->path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
        if(cycle->fd < 0)
        {
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
    }

    /* 2. 计算文件长度 */
    file_wrlock(cycle->fd); /* 加锁 */

    fsize = lseek(cycle->fd, 0, SEEK_END);
    if(-1 == fsize)
    {
        file_unlock(cycle->fd);
        Close(cycle->fd);
        alog_exception("errmsg:[%d]%s! path:[%s]!", errno, strerror(errno), file->path);
        return -1;
    }

    /* 3. 将信息写入文件，并关闭 */
    _alog_errlog_write(cycle->fd, cycle->pid, level, dump, dumplen, msg, curr_time);

    file_unlock(cycle->fd); /* 解锁 */

    /* 4. 重命名日志文件 */
    if(AlogIsTooLarge(fsize))
    {
        Close(cycle->fd);
        alog_rename(file, curr_time);
    }

    if(AlogGetLevel() < LOG_LEVEL_ERROR)
    {
        /* 5. 强制刷信息日志信息 */
        trclog = alog_get_cycle(LOG_LEVEL_DEBUG);
        trclog->action(trclog, level, dump, dumplen, msg, curr_time);
    }

    return 0;
}

/******************************************************************************
 **函数名称: _alog_errlog_write
 **功    能: 写入错误级别日志信息
 **输入参数: 
 **     cycle: 日志对象
 **     level: 日志级别
 **     dump: 需打印的内存
 **     dumplen: 打印的内存长度
 **     msg: 日志内容
 **     curr_time: 当前时间
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static void _alog_errlog_write(int fd, pid_t pid, int level,
    const char *dump, int dumplen, const char *msg, const struct timeb *curr_time)
{
    struct tm loctm;
    const char *levelstr = NULL;
    char errmsg[ALOG_MSG_MAX_LEN] = {0};

    memset(&loctm, 0, sizeof(loctm));

    localtime_r(&curr_time->time, &loctm);

    levelstr = AlogGetLevelStr(level);

    /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
    snprintf(errmsg, sizeof(errmsg),
        "@%d|%04d%02d%02d|%02d:%02d:%02d.%0.3d|%s %s\n",
        pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec, curr_time->millitm, levelstr, msg);    
    
    Writen(fd, errmsg, strlen(errmsg));
    
    if((NULL != dump) && (dumplen > 0)) 
    {        
        alog_errlog_print_dump(fd, dump, dumplen);    
    }
    return;
}

/******************************************************************************
 **函数名称: alog_errlog_print_dump
 **功    能: 以16进制打印日志信息
 **输入参数: 
 **     fd: 日志文件描述符
 **     dump: 内存首地址
 **     dumplen: 打印长度
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **修    改: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static void alog_errlog_print_dump(sw_int_t fd, const void *dump, int dumplen)
{
    char *ptr = NULL;
    const char *addr = NULL, *end = NULL;    
    unsigned char var[2] = {0, 31};    
    char line[ALOG_DUMP_LINE_MAX_SIZE] = {0},
            page[ALOG_DUMP_PAGE_MAX_SIZE] = {0};
    sw_int_t i = 0, n = 0, lineno = 0,
            offset = 0, count = 0, lines = 0, page_length = 0;


    addr = (const char *)dump;
    end = dump + dumplen;                       /* 内存结束地址 */
    lines = (dumplen - 1)/ALOG_DUMP_COL_NUM;    /* 每页行数 */

    while(addr < end) 
    {        
        for(lineno=0; lineno<=lines; lineno++) 
        {
            offset = 0;
            ptr = line;
            memset(line, 0, sizeof(line));

            count = lineno * ALOG_DUMP_COL_NUM;            

            sprintf(ptr, "%05d", count);
            ptr += 5;
            sprintf(ptr, "(%05x) ", count);        
            ptr += 8;

            for(i=0; (i<ALOG_DUMP_COL_NUM) && (addr<end); i++)
            {
                sprintf(ptr, "%02x ", *addr);
                ptr += 3;
                addr += 1;
            }        

            for(n=0; n<ALOG_DUMP_COL_NUM-i; n++) 
            {
                sprintf(ptr, "   ");
                ptr += 3;
            }            

            sprintf(ptr, " ");            
            ptr += 1;
            addr = addr - i;            

            for(n=0; n<i; n++) 
            {
                if(((unsigned char)(*addr) <= (var[1])) &&    ((unsigned char)(*addr) >= (var[0]))) 
                {                    
                    sprintf(ptr, "*");
                    ptr += 1;
                }     
                else 
                {                    
                    sprintf(ptr, "%c", *addr);
                    ptr += 1;
                }                
                addr += 1;            
            }        

            strcat(page, line);    
            strcat(page, "\n");            
            page_length += (ptr-line+1);

            if(0 == (lineno + 1)%ALOG_DUMP_PAGE_MAX_LINE)
            {
                Writen(fd, ALOG_DUMP_HEAD_STR, strlen(ALOG_DUMP_HEAD_STR));

                Writen(fd, page, page_length);
                memset(page, 0, sizeof(page));
                page_length = 0;
            }        
        } /* end of for    */    
    } /* end of while    */    

    Writen(fd, ALOG_DUMP_HEAD_STR, strlen(ALOG_DUMP_HEAD_STR));

    Writen(fd, page, page_length);
    return;
}

/******************************************************************************
 **函数名称: alog_rename
 **功    能: 获取备份日志文件名
 **输入参数: 
 **     file: 文件信息
 **     time: 当前时间
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int alog_rename(const alog_file_t *file, const struct timeb *time)
{
    struct tm loctm;
    char newpath[FILE_PATH_MAX_LEN] = {0};

    localtime_r(&time->time, &loctm);

    /* FORMAT: *.logYYYYMMDDHHMMSS.bak */
    snprintf(newpath, sizeof(newpath), 
        "%s%04d%02d%02d%02d%02d%02d.bak",
        file->path, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

    return Rename(file->path, newpath);
}

/******************************************************************************
 **函数名称: alog_trclog_init
 **功    能: 初始化跟踪日志信息
 **输入参数: 
 **     cycle: 日志对象
 **     level: 日志级别
 **     dump: 内存地址
 **     dumplen: 需打印的地址长度
 **     msg: 日志内容
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 日志模块初始化
 **     2. 将日志信息写入共享内存
 **注意事项: 
 **     注意: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int alog_trclog_init(alog_cycle_t *cycle, int level,
    const void *dump, int dumplen, const char *msg, const struct timeb *curr_time)
{
    int ret = 0;
    void *addr = NULL;
    pid_t pid = getpid();
    alog_file_t *file = NULL;
    char path[FILE_PATH_MAX_LEN] = {0};
    
    alog_trclog_lock();

    SetWorkDir();
    
    /* 1. 日志模块初始化 */
    ret = _alog_trclog_init();
    if(ret < 0)
    {
        alog_trclog_unlock();
        alog_exception("Initialize trace log failed!");
        return -1;
    }

    /* 2. 完成日志对象的创建 */
    if(NULL == cycle->file)
    {
        addr = AlogGetShmAddr();

        AlogTrclogDefPath(path, sizeof(path));

        file = alog_trclog_creat(addr, path);
        if(NULL == file)
        {
            alog_trclog_unlock();
            alog_exception("Create [%s] failed!", path);
            return -1;
        }
        cycle->file = file;
    }

    cycle->pid = pid;

    if(cycle->fd < 0)
    {
        cycle->fd = Open(path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
        if(cycle->fd < 0)
        {
            alog_trclog_unlock();
            alog_exception("errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
            return -1;
        }
    }

    /* 3. 将日志信息写入共享内存 */
    cycle->action = alog_trclog_write;

    cycle->action(cycle, level, dump, dumplen, msg, curr_time);
	
    alog_trclog_unlock();
	return 0;
}

/******************************************************************************
 **函数名称: alog_creat_shm
 **功    能: 创建共享内存
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 共享内存地址
 **实现描述: 
 **注意事项: 
 **     注意: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.12.06 #
 ******************************************************************************/
void *alog_creat_shm(void)
{
    int shmid = -1;
    void *addr = NULL;
    
    shmid = shmget(ALOG_SHM_KEY, 0, 0);
    if(shmid < 0)
    {
        shmid = shmget(ALOG_SHM_KEY, ALOG_SHM_SIZE, IPC_CREAT|0666);
        if(shmid < 0)
        {
            alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
            return NULL;
        }
    }

    addr = shmat(shmid, NULL, 0);
    if((void *)-1 == addr)
    {
        alog_exception("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    return addr;
}

/******************************************************************************
 **函数名称: _alog_trclog_init
 **功    能: 初始化跟踪日志信息
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 连接共享内存
 **     2. 打开消息队列
 **注意事项: 
 **     注意: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int _alog_trclog_init(void)
{
    void *addr = AlogGetShmAddr();
    int shmid = 0, msqid = 0, fd = 0;
    char path[FILE_PATH_MAX_LEN] = {0};

    /* 1. 连接共享内存 */
    if(!AlogIsShmAddrValid())
    {
        addr = alog_creat_shm();
        if(NULL == addr)
        {
            alog_exception("Create share-memory failed!");
            return -1;
        }
        
        AlogSetShmAddr(addr);
    }

    if(!alog_is_lock_fd_valid())
    {
        AlogGetLockPath(path, sizeof(path));

        Mkdir2(path, ALOG_DIR_MODE);
        
        fd = Open(path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
        if(fd < 0)
        {
            alog_exception("errmsg:[%d] %s! [%s]", errno, strerror(errno), path);
            return -1;
        }

        alog_set_lock_fd(fd);
    }

    return 0;
}

#if !defined(__ALOG_MULT_SHARE__)
/******************************************************************************
 **函数名称: alog_trclog_conflict
 **功    能: 日志名冲突时，产生新的日志名
 **输入参数: 
 **     oripath: 原始日志名
 **     size: newpath的空间大小
 **     idx: 索引
 **输出参数: 
 **     newpath: 新日志名
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.05 #
 ******************************************************************************/
static int alog_trclog_conflict(const char *oripath, char *newpath, int size, int idx)
{
    int len = 0;
    char *ptr = NULL;
    char suffix[FILE_NAME_MAX_LEN] = {0};
    
	memset(newpath, 0, size);
	
    snprintf(newpath, size, "%s", oripath);
    snprintf(suffix, sizeof(suffix), "-[%02d].log", idx);

    len = strlen(newpath) + strlen(suffix) - strlen(ALOG_SUFFIX);
    if(len >= size)
    {
        alog_exception("Not enough memory! newpath:[%s] suffix:[%s] len:[%d]/[%d]",
            newpath, suffix, len, size);
        return -1;  /* Not enough memory */
    }

    ptr = strrchr(newpath, '.');
    if(NULL != ptr)
    {
        if(0 == strcasecmp(ptr, ALOG_SUFFIX))
        {
            sprintf(ptr, "%s", suffix);
        }
        else
        {
            strcat(ptr, suffix);
        }
        return 0;
    }
    
    strcat(newpath, suffix);
    return 0;
}
#endif /*__ALOG_MULT_SHARE__*/

/******************************************************************************
 **函数名称: alog_trclog_creat
 **功    能: 创建跟踪日志信息
 **输入参数: 
 **     addr: 共享内存首地址
 **     path: 日志绝对路径
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 判断当前功能内存中是否有文件名一致的日志
 **     2. 选择合适的日志缓存，并返回
 **注意事项: 
 **     1. 当存在日志名一致的日志时, 判断进程ID是否一样.
 **        如果不一样, 且对应进程, 依然正在运行, 则提示创建跟踪日志失败!
 **     2. 请勿在此函数中调用错误日志函数 - 小心死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static alog_file_t *alog_trclog_creat(void *addr, const char *path)
{
    pid_t pid = getpid();
    alog_file_t *file = NULL;
    int ret = 0, idx = 0, hash_idx = 0, repeat = 0, idle_idx = -1;
    char newpath[FILE_NAME_MAX_LEN] = {0};
    const char *ptr = path;

    hash_idx = AlogHash(path);

    alog_fcache_all_wrlock();
    
    for(idx=0; idx<ALOG_FILE_MAX_NUM; idx++, hash_idx++)
    {
        hash_idx %= ALOG_FILE_MAX_NUM;

        file = (alog_file_t *)(addr + hash_idx * ALOG_FILE_CACHE_SIZE);
        
        /* 1. 判断文件名是否一致 */
        if(!strcmp(file->path, ptr))   /* 文件名出现一致 */
        {
            /* 判断进程是否存在，是否和当前进程ID一致 */
            if(alog_file_check_pid(file, pid))
            {
                alog_fcache_all_unlock();
                return file;
            }
            
        #if defined(__ALOG_MULT_SHARE__)
            ret = alog_file_add_pid(file, pid);
            if(0 != ret)
            {
                alog_fcache_all_unlock();
                return NULL;
            }
            
            alog_fcache_all_unlock();
            return file;
        #else /*__ALOG_MULT_SHARE__*/
            ret = pub_proc_checkpid(file->pid);
            if(SW_OK != ret)
            {
                alog_file_add_pid(file, pid);
                alog_trclog_sync(file);

                alog_fcache_all_unlock();
                return file;
            }

            /* 文件名重复，且其他进程正在运行... */
            ret = alog_trclog_conflict(path, newpath, sizeof(newpath), ++repeat);
            if(ret < 0)
            {
                alog_fcache_all_unlock();
                return NULL;
            }
            
            ptr = newpath;
            hash_idx = AlogHash(ptr);
            idle_idx = -1;
            idx = -1;
            continue;
        #endif /*__ALOG_MULT_SHARE__*/
        }
        else if(-1 == idle_idx)  /* 当为找到空闲时，则进行后续判断 */
        {
            /* 路径是否为空 */
            if(strlen_iszero(file->path))
            {
                idle_idx = hash_idx;

                memset(file, 0, sizeof(alog_file_t));
                file->idx = hash_idx;
                alog_file_reset_pid(file);
                continue;
            }

        #if defined(__ALOG_MULT_SHARE__)
            /* 当缓存路径不为空，且所有相关进程都死亡，则进行强制同步 */
            if(!alog_file_is_pid_live(file))
            {
                alog_trclog_sync(file);
				
                memset(file, 0, sizeof(alog_file_t));
				
                idle_idx = hash_idx;
                file->idx = hash_idx;
                continue;
            }
        #else /*__ALOG_MULT_SHARE__*/
            /* 进程是否存在 */
            ret = pub_proc_checkpid(file->pid);
            if(SW_OK != ret)
            {
                idle_idx = hash_idx;
                
                memset(file, 0, sizeof(alog_file_t));
                file->idx = hash_idx;
                file->pid = ALOG_INVALID_PID;
                continue;
            }
        #endif /*__ALOG_MULT_SHARE__*/
        }
    }

    if(-1 == idle_idx)
    {
        alog_fcache_all_unlock();
        return NULL;
    }

    file = (alog_file_t *)(addr + idle_idx * ALOG_FILE_CACHE_SIZE);

    alog_file_add_pid(file, pid);
    snprintf(file->path, sizeof(file->path), "%s", ptr);

    alog_fcache_all_unlock();

    return file;
}

/******************************************************************************
 **函数名称: alog_trclog_release
 **功    能: 释放申请的日志缓存
 **输入参数: 
 **     file: 日志对象
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.05 #
 ******************************************************************************/
static void alog_trclog_release(alog_file_t *file)
{
    pid_t pid = getpid();
    int idx = 0, num = 0;
    

    idx = file->idx;
    
    alog_fcache_wrlock(file);

    alog_trclog_sync(file);

#if defined(__ALOG_MULT_SHARE__)
    for(num=0; num<ALOG_PID_MAX_NUM; num++)
    {
        if(pid == file->pid[num])
        {
            file->pid[num] = ALOG_INVALID_PID;
        }
    }
#else /*__ALOG_MULT_SHARE__*/
    memset(file, 0, sizeof(alog_file_t));

    alog_file_reset_pid(file);
#endif /*__ALOG_MULT_SHARE__*/

    file->idx = idx;
    
    alog_fcache_unlock(file);
}

/******************************************************************************
 **函数名称: alog_trclog_write
 **功    能: 将跟踪日志信息写入缓存
 **输入参数: 
 **     cycle: 日志对象
 **     level: 日志级别
 **     dump: 内存地址
 **     dumplen: 需打印的地址长度
 **     msg: 日志内容
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int alog_trclog_write(alog_cycle_t *cycle, int level,
    const void *dump, int dumplen, const char *msg, const struct timeb *curr_time)
{
    int msglen = 0, left = 0;
    char *addr = NULL;
    struct tm loctm;
    time_t diff_time = 0;
    const char *prompt = NULL;
    alog_file_t *file = cycle->file;

    memset(&loctm, 0, sizeof(loctm));

    prompt = AlogGetLevelStr(level);        /* 获取日志级别对应的提示符 */
    localtime_r(&curr_time->time, &loctm);  /* 获取当前系统时间 */

#if defined(__ALOG_MULT_SHARE__) || defined(__ALOG_SVR_SYNC__)
    alog_fcache_wrlock(file);      /* 缓存加锁 */
#endif /*__ALOG_MULT_SHARE__ || __ALOG_SVR_SYNC__*/

    addr = (char *)(file + 1) + file->in_offset;
    left = AlogGetDataSize() - file->in_offset;

    /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
    snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%0.3d|%s %s\n",
        cycle->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec, curr_time->millitm, prompt, msg);    
    
    msglen = strlen(addr);
    file->in_offset += msglen;
    addr += msglen;
    left -= msglen;

    /* 打印DUMP数据 */
    if((NULL != dump) && (dumplen > 0) && (left > dumplen)) 
    {
        msglen = alog_trclog_print_dump(addr, dump, dumplen);

        file->in_offset += msglen;
    }

    /* 判断是否强制写或发送通知 */
    diff_time = curr_time->time - file->sync_time.time;
    if(AlogIsForceSync(file)  
    #if defined(__ALOG_ERR_FORCE__)
        || (AlogIsErrLevel(level) && (level != AlogGetLevel()))
    #endif /*__ALOG_ERR_FORCE__*/
        || AlogIsTimeout(diff_time))
    {
        memcpy(&file->sync_time, curr_time, sizeof(file->sync_time));
        
        alog_trclog_sync_ext(cycle);
    }

#if defined(__ALOG_MULT_SHARE__) || defined(__ALOG_SVR_SYNC__)
    alog_fcache_unlock(file);      /* 缓存解锁 */
#endif /*__ALOG_MULT_SHARE__ || __ALOG_SVR_SYNC__*/

    return 0;
}

/******************************************************************************
 **函数名称: alog_trclog_print_dump
 **功    能: 以16进制打印日志信息
 **输入参数: 
 **     fd: 日志文件描述符
 **     dump: 内存首地址
 **     dumplen: 打印长度
 **输出参数: NONE
 **返    回: 长度
 **实现描述: 
 **注意事项: 
 **修    改: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int alog_trclog_print_dump(char *addr, const void *dump, int dumplen)
{
    char *in = addr;
    const char *dump_ptr = NULL, *dump_end = NULL;    
    unsigned char var[2] = {0, 31};    
    sw_int_t idx = 0, n = 0, lineno = 0, count = 0, lines = 0, head_len = 0;


    dump_ptr = (const char *)dump;
    dump_end = dump + dumplen;                  /* 内存结束地址 */
    lines = (dumplen - 1)/ALOG_DUMP_COL_NUM;    /* 每页行数 */
    head_len = strlen(ALOG_DUMP_HEAD_STR);

    while(dump_ptr < dump_end) 
    {        
        for(lineno=0; lineno<=lines; lineno++) 
        {
            /* 1. 判断是否打印头部字串 */
            if(0 == (lineno + 1)%ALOG_DUMP_PAGE_MAX_LINE)
            {
                sprintf(in, "%s", ALOG_DUMP_HEAD_STR);
                in += head_len;
            }

            /* 2. 计算偏移量 */
            count = lineno * ALOG_DUMP_COL_NUM;            

            /* 3. 将信息写入缓存 */
            sprintf(in, "%05d", count);
            in += 5;
            sprintf(in, "(%05x) ", count);        
            in += 8;

            /* >>3.1 16进制打印一行 */
            for(idx=0; (idx<ALOG_DUMP_COL_NUM) && (dump_ptr<dump_end); idx++)
            {
                sprintf(in, "%02x ", *dump_ptr);
                in += 3;
                dump_ptr++;
            }        

            /* >>3.2 最后数据不足一行时，使用空格补上 */
            for(n=0; n<ALOG_DUMP_COL_NUM-idx; n++) 
            {
                sprintf(in, "   ");
                in += 3;
            }            

            sprintf(in, " ");            
            in++;
            dump_ptr -= idx;

            /* >>3.3 以字符方式打印信息 */
            for(n=0; n<idx; n++) 
            {
                if(((unsigned char)(*dump_ptr) <= (var[1])) 
                    &&    ((unsigned char)(*dump_ptr) >= (var[0]))) 
                {
                    *(in++) = '*';
                }     
                else 
                {                    
                    *(in++) = *dump_ptr;
                }                
                dump_ptr++;
            }        

            *(in++) = '\n';
        } /* dump_end of for    */    
    } /* dump_end of while    */    

    return (in - addr);
}

/******************************************************************************
 **函数名称: alog_trclog_sync
 **功    能: 强制同步跟踪日志信息到日志文件
 **输入参数: 
 **     file: 跟踪日志文件信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
int alog_trclog_sync(alog_file_t *file)
{
    size_t fsize = 0;

    /* 1. 执行同步操作 */
    fsize = alog_sync(file, NULL);

    /* 2. 文件是否过大 */
    if(AlogIsTooLarge(fsize))
    {
        alog_rename(file, &file->sync_time);
    }

    return 0;
}

/******************************************************************************
 **函数名称: alog_trclog_sync_ext
 **功    能: 强制同步跟踪日志信息到日志文件
 **输入参数: 
 **     file: 跟踪日志文件信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int alog_trclog_sync_ext(alog_cycle_t *cycle)
{
    size_t fsize = 0;

    /* 1. 执行同步操作 */
    fsize = alog_sync(cycle->file, &cycle->fd);

    /* 2. 文件是否过大 */
    if(AlogIsTooLarge(fsize))
    {
        Close(cycle->fd);
        return alog_rename(cycle->file, &cycle->file->sync_time);
    }

    return 0;
}

/******************************************************************************
 ** Name : alog_sync
 ** Desc : 强制同步业务日志
 ** Input: 
 **	    file: 文件缓存
 **     fd: 文件描述符
 ** Output: NONE
 ** Return: 如果进行同步操作，则返回文件的实际大小!
 ** Process:
 ** Note :
 **     1) 一旦撰写日志失败，需要清除缓存中的日志，防止内存溢出，导致严重后果!
 **     2) 当fd为空指针时，打开的文件需要关闭.
 **     3) 在此函数中不允许调用错误级别的日志函数 可能死锁!
 ** Author: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
static size_t alog_sync(alog_file_t *file, int *fd)
{
    int ret = 0, loc_fd = -1;
    void *addr = NULL;
    struct stat buff;
    size_t n = 0, fsize = 0;

    memset(&buff, 0, sizeof(buff));

    /* 1. 判断是否需要同步 */
    if(file->in_offset == file->out_offset)
    {
        return 0;
    }

    /* 2. 计算同步地址和长度 */
    addr = (void *)(file + 1);
    n = file->in_offset - file->out_offset;
    loc_fd = (NULL != fd)? *fd : ALOG_INVALID_FD;

    /* 撰写日志文件 */
    do
    {
        /* 3. 文件是否存在 */
        ret = lstat(file->path, &buff);
        if(ret < 0)
        {
            if(ENOENT != errno)
            {
                Close(loc_fd);
                alog_exception("errmsg:[%d]%s path:[%s]", errno, strerror(errno), file->path);
                break;
            }
            Close(loc_fd);
            Mkdir2(file->path, ALOG_DIR_MODE);
        }

        /* 4. 是否重新创建文件 */
        if(loc_fd < 0)
        {
            loc_fd = Open(file->path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
            if(loc_fd < 0)
            {
                alog_exception("errmsg:[%d] %s! path:[%s]", errno, strerror(errno), file->path);
                break;
            }
        }

        /* 5. 定位到文件末尾 */
        fsize = lseek(loc_fd, 0, SEEK_END);
        if(-1 == fsize)
        {
            Close(loc_fd);
            alog_exception("errmsg:[%d] %s! path:[%s]", errno, strerror(errno), file->path);
            break;
        }
        
        /* 6. 写入指定日志文件 */
        Writen(loc_fd, addr, n);

        fsize += n;
    }while(0);
    
    /* 7. 标志复位 */
    memset(addr, 0, n);
    file->in_offset = 0;
    file->out_offset = 0;
    ftime(&file->sync_time);

    if(NULL != fd)
    {
        *fd = loc_fd;
    }
    else
    {
        Close(loc_fd);
    }
    return fsize;
}

#if defined(__ALOG_MULT_SHARE__)
/******************************************************************************
 **函数名称: alog_file_check_pid
 **功    能: 判断指定PID是否在文件缓存进程列表中
 **输入参数: 
 **     file: 缓存信息
 **     pid: 进程ID
 **输出参数: NONE
 **返    回: true:在进程列表中  false:不在进程列表中
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.12 #
 ******************************************************************************/
static bool alog_file_check_pid(const alog_file_t *file, pid_t pid)
{
    int idx = 0;

    for(idx=0; idx<ALOG_PID_MAX_NUM; idx++)
    {
        if(pid == file->pid[idx])
        {
            return true;
        }
    }

    return false;
}

/******************************************************************************
 **函数名称: alog_file_add_pid
 **功    能: 将指定PID加入到文件缓存列表中
 **输入参数: 
 **     file: 缓存信息
 **     pid: 进程ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.12 #
 ******************************************************************************/
static int alog_file_add_pid(alog_file_t *file, pid_t pid)
{
    int ret = 0, idx = 0;

    /* 1. 判断PID是否在列表中 */
    if(alog_file_check_pid(file, pid))
    {
        return 0;
    }

    /* 2. 将PID加入到列表中 */
    for(idx=0; idx<ALOG_PID_MAX_NUM; idx++)
    {
        ret = pub_proc_checkpid(file->pid[idx]);
        if(0 != ret)
        {
            file->pid[idx] = pid;
            return 0;
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: alog_file_is_pid_live
 **功    能: 判断是否占用文件缓存的进程中依然有一个或以上在运行
 **输入参数: 
 **     file: 缓存信息
 **输出参数: NONE
 **返    回: true:有一个或以上在运行  false:都不在运行
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.12 #
 ******************************************************************************/
bool alog_file_is_pid_live(const alog_file_t *file)
{
    int ret = 0, idx = 0;

    for(idx=0; idx<ALOG_PID_MAX_NUM; idx++)
    {
        ret = pub_proc_checkpid(file->pid[idx]);
        if(0 != ret)
        {
            continue;
        }
        
        return true;
    }

    return false;
}

/******************************************************************************
 **函数名称: alog_file_has_idle
 **功    能: 判断进程表空间有空闲空间
 **输入参数: 
 **     file: 缓存信息
 **输出参数: NONE
 **返    回: true:在进程列表中  false:不在进程列表中
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.12 #
 ******************************************************************************/
bool alog_file_has_idle(const alog_file_t *file)
{
    int ret = 0, idx = 0;

    for(idx=0; idx<ALOG_PID_MAX_NUM; idx++)
    {
        if(file->pid[idx] <= 0)
        {
            return true;
        }
        
        ret = pub_proc_checkpid(file->pid[idx]);
        if(0 != ret)
        {
            return true;
        }
    }

    return false;
}

/******************************************************************************
 **函数名称: alog_file_has_others
 **功    能: 判断除了pid外，是否还有其他正在运行的进程正在使用文件缓存
 **输入参数: 
 **     file: 缓存信息
 **输出参数: NONE
 **返    回: true:在进程列表中  false:不在进程列表中
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.12 #
 ******************************************************************************/
static bool alog_file_has_others(alog_file_t *file, pid_t pid)
{
    int ret = 0, idx = 0;

    for(idx=0; idx<ALOG_PID_MAX_NUM; idx++)
    {
        if((file->pid[idx] <= 0)
            || (pid == file->pid[idx]))
        {
            continue;
        }
        
        ret = pub_proc_checkpid(file->pid[idx]);
        if(SW_OK != ret)
        {
            file->pid[idx] = ALOG_INVALID_PID;
            continue;
        }
        return false;
    }
    
    return true;
}

/******************************************************************************
 **函数名称: alog_file_reset_pid
 **功    能: 重置文件缓存相关联的PID列表
 **输入参数: 
 **     file: 缓存信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.13 #
 ******************************************************************************/
void alog_file_reset_pid(alog_file_t *file)
{
    int idx = 0;

    for(idx=0; idx<ALOG_PID_MAX_NUM; idx++)
    {
        file->pid[idx] = ALOG_INVALID_PID;
    }
}
#endif /*__ALOG_MULT_SHARE__*/

#if defined(__ASYNC_ULOG__)
/*******************************************************************************
 * 模块: 同步业务日志模块
 * 作用: 负责将业务日志打印到指定的文件中
 * 说明: 
 *      日志目录: 产品名+服务名
 *      日志命名: 服务名_YYYYMMDD_进程索引.log
 * 注意: 
 *      1. 考虑到对性能的要求, 撰写日志的过程中并不加锁, 因此不同的进程不允许将
 *         业务日志信息写入同一个文件中.
 *      2. 在切换日志之前, 只打开日志文件一次
 * 作者: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
ulog_cycle_t g_usr_log = {ALOG_INVALID_FD, -1, -1, -1, "", "", "", {0}, NULL};
#define uLogGetCycle() (&g_usr_log)

/* 业务日志大小 */
static sw_int_t g_ulog_max_size = ULOG_MAX_SIZE;
#define uLogGetMaxSize() (g_ulog_max_size)
#define uLogSetMaxSize(size) (g_ulog_max_size = (size))
#define uLogIsTooLarge(size) ((size) >= g_ulog_max_size)

/* 业务日志级别 */
static int g_uLogLevel = LOG_LEVEL_DEBUG;
#define uLogGetLevel() (g_uLogLevel)
void ulog_set_level(alog_level_e level) { g_uLogLevel = level; }

#define ulog_force_sync(ulog) alog_sync(ulog->file, &ulog->fd)
static int ulog_date_switch(ulog_cycle_t *ulog, const struct tm *dt);
static int ulog_set_path(ulog_cycle_t *ulog, const char *path);
/******************************************************************************
 ** Name : ulog_init
 ** Desc : 初始化业务日志模块
 ** Input: 
 **	    trcno: 流水号
 **     prdt_name: 产品名
 **     svr_name: 服务名
 **     svc_name: SVC名
 **     idx: 进程索引IDX
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 ** Note :
 ** Author: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
sw_int_t ulog_init(
    sw_int64_t trcno,
    const char *prdt_name, const char *svr_name, const char *svc_name, int idx)
{
    int ret = 0;
    time_t ctime = 0;
    char path[FILE_PATH_MAX_LEN] = {0};
    ulog_cycle_t *ulog = uLogGetCycle();

    SetWorkDir();

    log_debug("Init ulog...");

    /* 1.设置业务日志基本信息 */
    ctime = time(NULL);
    localtime_r(&ctime, &ulog->dtime);
    ulog->dtime.tm_year += 1900;
    ulog->dtime.tm_mon += 1;
	
    ulog->trcno = trcno;
    ulog->idx = idx;
    ulog->pid = getpid();
    snprintf(ulog->prdt_name, sizeof(ulog->prdt_name), "%s", prdt_name);
    snprintf(ulog->svr_name, sizeof(ulog->svr_name), "%s", svr_name);
    snprintf(ulog->svc_name, sizeof(ulog->svc_name), "%s", svc_name);

    /* 2.获取日志路径 并创建目录 */
    snprintf(path, sizeof(path),
        "%s/log/%s/%s/%s_%04d%02d%02d_%d.log",
        GetWorkDir(), prdt_name, svr_name, svc_name,
        ulog->dtime.tm_year, ulog->dtime.tm_mon, ulog->dtime.tm_mday, ulog->idx);

    Mkdir2(path, ALOG_DIR_MODE);

    /* 3. 日志同步 并申请日志缓存 */
    ulog_set_path(ulog, path);

    /* 4.打开业务日志文件 */
    ulog->fd = Open(path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
    if(ulog->fd < 0)
    {
        log_error("errmsg:[%d]%s path:[%s]", errno, strerror(errno), path);
        return -1;
    }

    return 0;
}

/******************************************************************************
 ** Name : uLog
 ** Desc : 写业务日志
 ** Input: 
 **	    level: 日志级别
 **	    fmt: 输出格式
 **	    ...: 可变参数
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 **     1. 检查日志文件时间、大小等状态
 **     2. 写日志头部信息
 **     3. 写日志实际内容
 ** Note :
 ** Author: # Qifeng.zou # 2013.07.04 #
 ******************************************************************************/
void uLog(int level, const char *fmt, ...)
{
    int len = 0, left = 0;
    va_list args;
    char *addr = NULL;
    struct tm loctm;
    time_t diff_time;
    struct timeb curr_time;
    const char *levelstr = NULL;
    char msg[ERR_MSG_MAX_LEN] = {0};
    ulog_cycle_t *ulog = uLogGetCycle();
    alog_file_t *file = ulog->file;

    memset(&args, 0, sizeof(args));
    memset(&curr_time, 0, sizeof(curr_time));

    /* 未初始化业务日志时，直接将日志信息打印到系统日志 */
    if(NULL == file)
    {
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        log_core(level, NULL, 0, NULL, 0, msg);
        return;
    }

    ftime(&curr_time);
    localtime_r(&curr_time.time, &loctm);
    loctm.tm_year += 1900;
    loctm.tm_mon += 1;

    levelstr = AlogGetLevelStr(level);
    
#if defined(__ALOG_MULT_SHARE__) || defined(__ALOG_SVR_SYNC__)
    alog_fcache_wrlock(file);      /* 缓存解锁 */
#endif /*__ALOG_MULT_SHARE__ || __ALOG_SVR_SYNC__*/

    addr = (char *)(file + 1) + file->in_offset;
    left = AlogGetDataSize() - file->in_offset;

    /* 1. 是否进行日切 */
    ulog_date_switch(ulog, &loctm);

    /* 2. 组日志头部信息 */
    /* @流水号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
    snprintf(addr, left, "@%012lld|%d|%04d%02d%02d|%02d:%02d:%02d.%0.3d|%s ",
        ulog->trcno, ulog->pid, loctm.tm_year, loctm.tm_mon, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec, curr_time.millitm, levelstr);

    len = strlen(addr);
    left -= len;

    /* 3. 组日志实际信息 */
    va_start(args, fmt);
    vsnprintf(addr+len, left-1, fmt, args); /* 减1是为换行符留空间 */
    va_end(args);

    strcat(addr, "\n");

    file->in_offset += strlen(addr);

    diff_time = curr_time.time - file->sync_time.time;
    
    if(AlogIsForceSync(file) 
    #if defined(__ALOG_ERR_FORCE__)
        || (AlogIsErrLevel(level) && (level != AlogGetLevel()))
    #endif /*__ALOG_ERR_FORCE__*/
        || AlogIsTimeout(diff_time))
    {
        ulog_force_sync_ext(ulog, &loctm);
    }

#if defined(__ALOG_MULT_SHARE__) || defined(__ALOG_SVR_SYNC__)
    alog_fcache_unlock(file);      /* 缓存解锁 */
#endif /*__ALOG_MULT_SHARE__ || __ALOG_SVR_SYNC__*/

    return;
}

/******************************************************************************
 ** Name : ulog_date_switch
 ** Desc : 日志日切
 ** Input: 
 **	    ulog: 业务日志信息
 **     dt: 当前日期和时间信息
 ** Output: NONE
 ** Return: VOID
 ** Process:
 **     1. 检查日志文件日期
 **     2. 同步缓存
 **     3. 重命名日志文件
 **     4. 新建日志
 ** Note :
 ** Author: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
sw_int_t ulog_date_switch(ulog_cycle_t *ulog, const struct tm *dt)
{
    int ret = 0;
    alog_file_t *file = ulog->file;
    char path[FILE_NAME_MAX_LEN] = {0};

    /* 1. 检查日期是否发生变化 */
    if((dt->tm_year == ulog->dtime.tm_year)
        && (dt->tm_mon == ulog->dtime.tm_mon)
        && (dt->tm_mday == ulog->dtime.tm_mday))
    {
        return 0;
    }

    /* 2. 同步缓存 */
    ulog_force_sync(ulog);

    Close(ulog->fd);

    /* 3. 新建日志 */
    memcpy(&ulog->dtime, dt, sizeof(struct tm));
    
    snprintf(file->path, sizeof(file->path),
        "%s/log/%s/%s/%s_%04d%02d%02d_%d.log",
        GetWorkDir(), ulog->prdt_name, ulog->svr_name, ulog->svc_name,
        ulog->dtime.tm_year, ulog->dtime.tm_mon, ulog->dtime.tm_mday, ulog->idx);

    Mkdir2(file->path, ALOG_DIR_MODE);
    
    ulog->fd = Open(file->path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
    if(ulog->fd < 0)
    {
        return -1;
    }

    return 0;
}

/******************************************************************************
 ** Name : ulog_set_path
 ** Desc : 设置业务日志路径
 ** Input: 
 **	    ulog: 业务日志对象
 **     path: 业务日志路径
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 ** Note :
 ** Author: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
static int ulog_set_path(ulog_cycle_t *ulog, const char *path)
{
    void *addr = AlogGetShmAddr();

    alog_trclog_lock();
    
    if(!AlogIsShmAddrValid())
    {
        addr = alog_creat_shm();
        if(NULL == addr)
        {
            alog_trclog_unlock();
            return -1;
        }
        
        AlogSetShmAddr(addr);
    }
    alog_trclog_unlock();

    if(NULL != ulog->file)
    {
        ulog_force_sync(ulog);
        Close(ulog->fd);
        
        snprintf(ulog->file->path, sizeof(ulog->file->path), "%s", path);
        return 0;
    }

    ulog->file = alog_trclog_creat(addr, path);
    if(NULL == ulog->file)
    {
        log_error("Create trclog failed! path:[%s]", path);
        return -1;
    }
    return 0;
}

/******************************************************************************
 ** Name : ulog_force_sync_ext
 ** Desc : 强制同步业务日志, 并判断日志文件是否过大
 ** Input: 
 **	    ulog: 业务日志对象
 **     dt: 当前日期和时间
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 ** Note :
 ** Author: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
int ulog_force_sync_ext(ulog_cycle_t *ulog, const struct tm *dt)
{
    size_t fsize = 0;
    char new_path[FILE_PATH_MAX_LEN] = {0};

    /* 1. 进行同步文件 */
    fsize = ulog_force_sync(ulog);

    /* 2. 文件是否过大 */
    if(uLogIsTooLarge(fsize))
    {
        Close(ulog->fd);
        
        snprintf(new_path, sizeof(new_path),
            "%s/log/%s/%s/%s_%04d%02d%02d%02d%02d%02d_%d.log",
            GetWorkDir(), ulog->prdt_name, ulog->svr_name, ulog->svc_name,
            dt->tm_year, dt->tm_mon, dt->tm_mday, 
            dt->tm_hour, dt->tm_min, dt->tm_sec, ulog->idx);

        return Rename(ulog->file->path, new_path);
    }

    return 0;
}


/******************************************************************************
 ** Name : ulog_release
 ** Desc : 释放日志信息
 ** Input: NONE
 ** Output: NONE
 ** Return: VOID
 ** Process:
 ** Note :
 ** Author: # Qifeng.zou # 2013.07.04 #
 ******************************************************************************/
void ulog_release(void)
{
    ulog_cycle_t *ulog = uLogGetCycle();

    ulog_force_sync(ulog);
    Close(ulog->fd);
    return;
}
#endif /*__ASYNC_ULOG__*/

#define alog_abnormal_path(path, size)  /* 内部错误日志路径 */\
    snprintf(path, sizeof(path), "%s/log/syslog/alog.log", GetWorkDir());

/******************************************************************************
 **函数名称: alog_abnormal
 **功    能: 内部日志模块的错误日志信息(日志模块内部使用)
 **输入参数: 
 **     fname: 文件名
 **     lineno: 文件行号
 **     fmt: 格式化输出
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.09 #
 ******************************************************************************/
void alog_abnormal(const char *fname, int lineno, const char *fmt, ...)
{
    va_list args;
    int ret = 0, fd = -1, len = 0, left = 0;
    struct stat buff;
    struct tm loctm;
    struct timeb curr_time;
    char errmsg[ALOG_MSG_MAX_LEN] = {0},
            path[FILE_PATH_MAX_LEN] = {0},
            newpath[FILE_PATH_MAX_LEN] = {0};

    memset(&buff, 0, sizeof(buff));
    memset(&curr_time, 0, sizeof(curr_time));

    ftime(&curr_time);
    localtime_r(&curr_time.time, &loctm);
    
    /* 拼装日志信息 */
    snprintf(errmsg, sizeof(errmsg), "@%d|%04d%02d%02d|%02d:%02d:%02d.%0.3d|ERR ",
        getpid(), loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec, curr_time.millitm);

    len = strlen(errmsg);
    left = sizeof(errmsg) - len;
    if(left <= 2)
    {
        fprintf(stderr, "[%s][%d] Log is too long!", __FILE__, __LINE__);
        return;
    }
    
    /* 获取错误日志信息 */
    va_start(args, fmt);
    if(NULL != fname)
    {
        snprintf(errmsg+len, left, "[%s][%d] ", fname, lineno);

        len = strlen(errmsg);
        left = sizeof(errmsg) - len;
        if(left <= 2)
        {
            fprintf(stderr, "[%s][%d] Log is too long!", __FILE__, __LINE__);
            return;
        }

        vsnprintf(errmsg+len, left-1, fmt, args);
    }
    else
    {
        vsnprintf(errmsg+len, left-1, fmt, args);
    }
    strcat(errmsg, "\n");
    
    va_end(args);

    /* 判断文件&路径状态 */
    alog_abnormal_path(path, sizeof(path));

    ret = lstat(path, &buff);
    if(0 != ret)
    {
        if(ENOENT != errno)
        {
            fprintf(stderr, "[%s][%d] errmsg:[%d] %s!", __FILE__, __LINE__, errno, strerror(errno));
            return;
        }

        Mkdir2(path, ALOG_DIR_MODE);
    }

    /* 重新创建文件 */
    fd = Open(path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
    if(fd < 0)
    {
        fprintf(stderr, "[%s][%d] errmsg:[%d]%s\n", __FILE__, __LINE__, errno, strerror(errno));
        return;
    }
        
    file_wrlock(fd); /* 加锁 */

    lseek(fd, 0, SEEK_END);

    Writen(fd, errmsg, strlen(errmsg));

    file_unlock(fd); /* 解锁 */

    Close(fd);

    return;
}

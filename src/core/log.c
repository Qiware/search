/*******************************************************************************
 ** 模  块: 异步日志模块 - 客户端代码
 ** 说  明: 
 **     考虑性能要求, 日志首先存储在缓冲区中, 当缓存区中的日志达到一定量、或超
 **     过一定时间未同步、或出现错误级别日志时, 则将日志同步到日志文件中.
 ** 注  意: 
 **     不同的"进程"和"线程"是不能使用的同名的日志文件
 ** 作  者: # Qifeng.zou # 2013.11.07 #
 ******************************************************************************/
#include <sys/shm.h>
#include <sys/types.h>

#include "log.h"
#include "hash.h"
#include "common.h"
#include "xdo_lock.h"
#include "xdo_unistd.h"

#if defined(__ASYNC_LOG__)

/* 共享内存 */
static void *g_log_shm_addr = (void *)-1;  /* 共享内存地址 */

#define log_get_shm_addr() (g_log_shm_addr)              /* 获取共享内存地址 */
#define log_set_shm_addr(addr) (g_log_shm_addr = (addr)) /* 设置共享内存地址 */
#define log_is_shm_addr_valid()    /* 判断共享内存地址是否合法 */ \
            (NULL != g_log_shm_addr && (void *)-1 != g_log_shm_addr)

/* 文件锁 */
static int g_log_lock_fd = -1;             /* 文件锁FD */

#define log_get_lock_fd() (g_log_lock_fd)             /* 获取文件锁FD */
#define log_set_lock_fd(fd) (g_log_lock_fd = (fd))    /* 设置文件锁FD */
#define log_is_lock_fd_valid() (g_log_lock_fd >= 0)   /* 判断文件锁FD是否合法 */
#define log_fcache_wrlock(file) proc_wrlock_b(g_log_lock_fd, (file)->idx+1)  /* 加缓存写锁 */
#define log_fcache_rdlock(file) proc_rdlock_b(g_log_lock_fd, (file)->idx+1)  /* 加缓存写锁 */
#define log_fcache_unlock(file) proc_unlock_b(g_log_lock_fd, (file)->idx+1)  /* 解缓存锁 */
#define log_fcache_all_wrlock() proc_wrlock(g_log_lock_fd)  /* 缓存加写锁(整个都加锁) */
#define log_fcache_all_unlock() proc_unlock(g_log_lock_fd)  /* 缓存解锁锁(整个都解锁) */

static size_t g_log_max_size = LOG_FILE_MAX_SIZE;
#define log_get_max_size() (g_log_max_size)     /* 获取日志大小 */
#define _log_set_max_size(size) (g_log_max_size = (size))
#define log_is_too_large(size) ((size) >= g_log_max_size)

/* 线程同步数据 */
static pthread_key_t g_log_specific_key;        /* 线程私有数据KEY */

/* 日志线程互斥锁: 日志对象和配置信息锁 */
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;  

#define log_mutex_lock() pthread_mutex_lock(&g_log_mutex)
#define log_mutex_trylock() pthread_mutex_trylock(&g_log_mutex)
#define log_mutex_unlock() pthread_mutex_unlock(&g_log_mutex)

/* 日志缓存数据空间大小 */
static const size_t g_log_data_size =  (LOG_FILE_CACHE_SIZE - sizeof(log_file_info_t));
#define log_get_data_size() (g_log_data_size)

#define log_hash(path) (hash_time33(path) % LOG_FILE_MAX_NUM)  /* 异步日志哈希 */
#define log_is_err_level(level) (LOG_LEVEL_ERROR & (level))

/* 函数声明 */
static int _log_init_global(void);
static log_file_info_t *log_creat(void *addr, const char *path);
static void log_release(log_file_info_t *file);
static int log_write(log_cycle_t *log, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *ctm);
static int log_print_dump(char *addr, const void *dump, int dumplen);
static int log_sync_ext(log_cycle_t *log);

static int log_rename(const log_file_info_t *file, const struct timeb *time);
static size_t _log_sync(log_file_info_t *file, int *fd);

/* 是否强制写(注意: 系数必须小于或等于0.8，否则可能出现严重问题) */
static const int g_log_sync_size = 0.8 * LOG_FILE_CACHE_SIZE;
#define log_is_force_sync(file) (((file)->in_offset - (file)->out_offset) > g_log_sync_size)

/******************************************************************************
 **函数名称: log_init
 **功    能: 初始化日志信息
 **输入参数: 
 **     level: 日志级别(其值：LOG_LEVEL_TRACE~LOG_LEVEL_FATAL的或值)
 **     path: 日志路径
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 日志模块初始化
 **     2. 将日志信息写入共享内存
 **注意事项: 
 **     注意: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
log_cycle_t *log_init(const char *level_str, const char *path)
{
    int ret;
    void *addr;
    log_cycle_t *log;

    /* 1. 新建日志对象 */
    log = (log_cycle_t *)calloc(1, sizeof(log_cycle_t));
    if (NULL == log)
    {
        return NULL;
    }

    log->level = log_get_level(level_str);
    log->pid = getpid();
    
    log_mutex_lock();

    do
    {
        /* 2. 初始化全局数据 */
        ret = _log_init_global();
        if(ret < 0)
        {
            fprintf(stderr, "Initialize trace log failed!");
            break;
        }

        /* 3. 完成日志对象的创建 */
        addr = log_get_shm_addr();

        log->file = log_creat(addr, path);
        if(NULL == log->file)
        {
            fprintf(stderr, "Create [%s] failed!", path);
            break;
        }

        /* 4. 新建日志文件 */
        log->fd = Open(path, OPEN_FLAGS, OPEN_MODE);
        if(log->fd < 0)
        {
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
            break;
        }

        log_mutex_unlock();
	    return log;
    } while(0);

    /* 5. 异常处理 */
    if (NULL != log->file)
    {
        log_release(log->file);
    }
    log_mutex_unlock();
    free(log);
    return NULL;
}

/******************************************************************************
 **函数名称: log_core
 **功    能: 日志核心调用
 **输入参数: 
 **     log: 日志对象
 **     level: 日志级别(LOG_LEVEL_TRACE ~ LOG_LEVEL_FATAL)
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
 **     日志级别的判断在函数外进行判断
 **作    者: # Qifeng.zou # 2013.10.24 #
 ******************************************************************************/
void log_core(log_cycle_t *log, int level,
                const char *fname, int lineno,
                const void *dump, int dumplen,
                const char *fmt, ...)
{
    int len;
    va_list args;
    struct timeb ctm;
    char errmsg[LOG_MSG_MAX_LEN];

    va_start(args, fmt);
    len = snprintf(errmsg, sizeof(errmsg), "[%s][%d] ", fname, lineno);
    vsnprintf(errmsg + len, sizeof(errmsg) - len, fmt, args);
    va_end(args);

    ftime(&ctm);
    
    log_write(log, level, dump, dumplen, errmsg, &ctm);
}

/******************************************************************************
 **函数名称: log_destroy
 **功    能: 销毁日志模块
 **输入参数: 
 **     log: 日志对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.02 #
 ******************************************************************************/
void log_destroy(log_cycle_t **log)
{
    log_mutex_lock();

    log_release((*log)->file);
    free(*log);
    *log = NULL;

    log_mutex_unlock();
}

/******************************************************************************
 **函数名称: log_get_cycle
 **功    能: 获取日志对象
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 日志对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.04 #
 ******************************************************************************/
log_cycle_t *log_get_cycle(void)
{
    return (log_cycle_t *)pthread_getspecific(g_log_specific_key);
}

/******************************************************************************
 **函数名称: log_get_level
 **功    能: 获取日志级别
 **输入参数: 
 **     level_str: 日志级别字串
 **输出参数: NONE
 **返    回: 日志级别
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.03 #
 ******************************************************************************/
int log_get_level(const char *level_str)
{
    int level;

    if (!strcasecmp(level_str, "fatal"))
    {
        return LOG_LEVEL_FATAL;
    }

    level = LOG_LEVEL_FATAL;
    if (!strcasecmp(level_str, "error"))
    {
        return level|LOG_LEVEL_ERROR;
    }

    level |= LOG_LEVEL_ERROR;
    if (!strcasecmp(level_str, "warn"))
    {
        return level|LOG_LEVEL_WARN;
    }

    level |= LOG_LEVEL_WARN;
    if (!strcasecmp(level_str, "info"))
    {
        return level|LOG_LEVEL_INFO;
    }

    level |= LOG_LEVEL_INFO;
    if (!strcasecmp(level_str, "debug"))
    {
        return level|LOG_LEVEL_DEBUG;
    }

    level |= LOG_LEVEL_TRACE;
    if (!strcmp(level_str, "TRACE"))
    {
        return level|LOG_LEVEL_TRACE;
    }

    return 0;
}

/******************************************************************************
 **函数名称: log_set_max_size
 **功    能: 设置日志尺寸的最大值
 **输入参数: 
 **     size: 日志尺寸
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.29 #
 ******************************************************************************/
void log_set_max_size(size_t size)
{
    log_mutex_lock();
    
    _log_set_max_size(size);

    log_mutex_unlock();
}

/******************************************************************************
 **函数名称: log_rename
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
static int log_rename(const log_file_info_t *file, const struct timeb *time)
{
    struct tm loctm;
    char newpath[FILE_PATH_MAX_LEN];

    localtime_r(&time->time, &loctm);

    /* FORMAT: *.logYYYYMMDDHHMMSS.bak */
    snprintf(newpath, sizeof(newpath), 
        "%s%04d%02d%02d%02d%02d%02d.bak",
        file->path, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

    return Rename(file->path, newpath);
}

/******************************************************************************
 **函数名称: log_creat_shm
 **功    能: 创建共享内存
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 共享内存地址
 **实现描述: 
 **注意事项: 
 **     注意: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.12.06 #
 ******************************************************************************/
void *log_creat_shm(void)
{
    int shmid;
    void *addr;
    
    shmid = shmget(LOG_SHM_KEY, 0, 0);
    if(shmid < 0)
    {
        shmid = shmget(LOG_SHM_KEY, LOG_SHM_SIZE, IPC_CREAT|0666);
        if(shmid < 0)
        {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            return NULL;
        }
    }

    addr = shmat(shmid, NULL, 0);
    if((void *)-1 == addr)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    return addr;
}

/******************************************************************************
 **函数名称: _log_init_global
 **功    能: 初始化全局数据
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
static int _log_init_global(void)
{
    int fd = 0;
    void *addr = log_get_shm_addr();
    char path[FILE_PATH_MAX_LEN];

    /* 1. 连接共享内存 */
    if(!log_is_shm_addr_valid())
    {
        addr = log_creat_shm();
        if(NULL == addr)
        {
            fprintf(stderr, "Create share-memory failed!");
            return -1;
        }
        
        log_set_shm_addr(addr);
    }

    if(!log_is_lock_fd_valid())
    {
        log_get_lock_path(path, sizeof(path));

        Mkdir2(path, DIR_MODE);
        
        fd = Open(path, OPEN_FLAGS, OPEN_MODE);
        if(fd < 0)
        {
            fprintf(stderr, "errmsg:[%d] %s! [%s]", errno, strerror(errno), path);
            return -1;
        }

        log_set_lock_fd(fd);
    }

    return 0;
}

/******************************************************************************
 **函数名称: log_name_conflict_handler
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
static int log_name_conflict_handler(
            const char *oripath,
            char *newpath, int size, int idx)
{
    int len = 0;
    char *ptr = NULL;
    char suffix[FILE_NAME_MAX_LEN];
    
	memset(newpath, 0, size);
	
    snprintf(newpath, size, "%s", oripath);
    snprintf(suffix, sizeof(suffix), "-[%02d].log", idx);

    len = strlen(newpath) + strlen(suffix) - strlen(LOG_SUFFIX);
    if(len >= size)
    {
        fprintf(stderr, "Not enough memory! newpath:[%s] suffix:[%s] len:[%d]/[%d]",
            newpath, suffix, len, size);
        return -1;  /* Not enough memory */
    }

    ptr = strrchr(newpath, '.');
    if(NULL != ptr)
    {
        if(0 == strcasecmp(ptr, LOG_SUFFIX))
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

/******************************************************************************
 **函数名称: log_creat
 **功    能: 创建日志信息
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
 **        如果不一样, 且对应进程, 依然正在运行, 则提示创建日志失败!
 **     2. 请勿在此函数中调用错误日志函数 - 小心死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static log_file_info_t *log_creat(void *addr, const char *path)
{
    pid_t pid = getpid();
    log_file_info_t *file;
    int ret, idx, hash_idx = 0, repeat = 0, idle_idx = -1;
    char newpath[FILE_NAME_MAX_LEN];
    const char *ptr = path;

    hash_idx = log_hash(path);

    log_fcache_all_wrlock();
    
    for(idx=0; idx<LOG_FILE_MAX_NUM; idx++, hash_idx++)
    {
        hash_idx %= LOG_FILE_MAX_NUM;

        file = (log_file_info_t *)(addr + hash_idx * LOG_FILE_CACHE_SIZE);
        
        /* 1. 判断文件名是否一致 */
        if(!strcmp(file->path, ptr))   /* 文件名出现一致 */
        {
            /* 判断进程是否存在，是否和当前进程ID一致 */
            if(file->pid == pid)
            {
                log_fcache_all_unlock();
                return file;
            }
            
            ret = proc_is_exist(file->pid);
            if(0 != ret)
            {
                file->pid = pid;
                log_sync(file);

                log_fcache_all_unlock();
                return file;
            }

            /* 文件名重复，且其他进程正在运行... */
            ret = log_name_conflict_handler(path, newpath, sizeof(newpath), ++repeat);
            if(ret < 0)
            {
                log_fcache_all_unlock();
                return NULL;
            }
            
            ptr = newpath;
            hash_idx = log_hash(ptr);
            idle_idx = -1;
            idx = -1;
            continue;
        }
        else if(-1 == idle_idx)  /* 当为找到空闲时，则进行后续判断 */
        {
            /* 路径是否为空 */
            if('\0' == file->path[0])
            {
                idle_idx = hash_idx;

                memset(file, 0, sizeof(log_file_info_t));
                file->idx = hash_idx;
                file->pid = INVALID_PID;
                continue;
            }

            /* 进程是否存在 */
            ret = proc_is_exist(file->pid);
            if(0 != ret)
            {
                idle_idx = hash_idx;
                
                memset(file, 0, sizeof(log_file_info_t));
                file->idx = hash_idx;
                file->pid = INVALID_PID;
                continue;
            }
        }
    }

    if(-1 == idle_idx)
    {
        log_fcache_all_unlock();
        return NULL;
    }

    file = (log_file_info_t *)(addr + idle_idx * LOG_FILE_CACHE_SIZE);

    file->pid = pid;
    snprintf(file->path, sizeof(file->path), "%s", ptr);

    log_fcache_all_unlock();

    return file;
}

/******************************************************************************
 **函数名称: log_release
 **功    能: 释放申请的日志缓存
 **输入参数: 
 **     file: 日志对象
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.05 #
 ******************************************************************************/
static void log_release(log_file_info_t *file)
{
    int idx;

    idx = file->idx;
    
    log_fcache_wrlock(file);

    log_sync(file);

    memset(file, 0, sizeof(log_file_info_t));

    file->pid = INVALID_PID;

    file->idx = idx;
    
    log_fcache_unlock(file);
}

/******************************************************************************
 **函数名称: log_write
 **功    能: 将日志信息写入缓存
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
static int log_write(log_cycle_t *log, int level,
    const void *dump, int dumplen, const char *errmsg, const struct timeb *ctm)
{
    int msglen = 0, left = 0;
    char *addr = NULL;
    struct tm loctm;
    time_t diff_time = 0;
    log_file_info_t *file = log->file;

    memset(&loctm, 0, sizeof(loctm));

    localtime_r(&ctm->time, &loctm);      /* 获取当前系统时间 */

    log_fcache_wrlock(file);      /* 缓存加锁 */

    addr = (char *)(file + 1) + file->in_offset;
    left = log_get_data_size() - file->in_offset;

    switch (level)
    {
        case LOG_LEVEL_FATAL:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|FATAL %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_ERROR:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|ERROR %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_WARN:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|WARN %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_INFO:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|INFO %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_DEBUG:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|DEBUG %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_TRACE:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|TRACE %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        default:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            snprintf(addr, left, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|OTHER %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
    } 

    msglen = strlen(addr);
    file->in_offset += msglen;
    addr += msglen;
    left -= msglen;

    /* 打印DUMP数据 */
    if((NULL != dump) && (dumplen > 0) && (left > dumplen)) 
    {
        msglen = log_print_dump(addr, dump, dumplen);

        file->in_offset += msglen;
    }

    /* 判断是否强制写或发送通知 */
    diff_time = ctm->time - file->sync_tm.time;
    if(log_is_force_sync(file)  
    #if defined(__LOG_ERR_FORCE__)
        || log_is_err_level(level)
    #endif /*__LOG_ERR_FORCE__*/
        || log_is_timeout(diff_time))
    {
        memcpy(&file->sync_tm, ctm, sizeof(file->sync_tm));
        
        log_sync_ext(log);
    }

    log_fcache_unlock(file);      /* 缓存解锁 */

    return 0;
}

/******************************************************************************
 **函数名称: log_print_dump
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
static int log_print_dump(char *addr, const void *dump, int dumplen)
{
    char *in = addr;
    const char *dump_ptr = NULL, *dump_end = NULL;    
    unsigned char var[2] = {0, 31};    
    int idx = 0, n = 0, row = 0, count = 0, rows = 0, head_len = 0;


    dump_ptr = (const char *)dump;
    dump_end = dump + dumplen;                  /* 内存结束地址 */
    rows = (dumplen - 1)/LOG_DUMP_COL_NUM;    /* 每页行数 */
    head_len = strlen(LOG_DUMP_HEAD_STR);

    while(dump_ptr < dump_end) 
    {        
        for(row=0; row<=rows; row++) 
        {
            /* 1. 判断是否打印头部字串 */
            if(0 == (row + 1)%LOG_DUMP_PAGE_MAX_ROWS)
            {
                sprintf(in, "%s", LOG_DUMP_HEAD_STR);
                in += head_len;
            }

            /* 2. 计算偏移量 */
            count = row * LOG_DUMP_COL_NUM;            

            /* 3. 将信息写入缓存 */
            sprintf(in, "%05d", count);
            in += 5;
            sprintf(in, "(%05x) ", count);        
            in += 8;

            /* >>3.1 16进制打印一行 */
            for(idx=0; (idx<LOG_DUMP_COL_NUM) && (dump_ptr<dump_end); idx++)
            {
                sprintf(in, "%02x ", *dump_ptr);
                in += 3;
                dump_ptr++;
            }        

            /* >>3.2 最后数据不足一行时，使用空格补上 */
            for(n=0; n<LOG_DUMP_COL_NUM-idx; n++) 
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
 **函数名称: log_sync
 **功    能: 强制同步日志信息到日志文件
 **输入参数: 
 **     file: 日志文件信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
void log_sync(log_file_info_t *file)
{
    size_t fsize;

    /* 1. 执行同步操作 */
    fsize = _log_sync(file, NULL);

    /* 2. 文件是否过大 */
    if(log_is_too_large(fsize))
    {
        log_rename(file, &file->sync_tm);
    }
}

/******************************************************************************
 **函数名称: log_sync_ext
 **功    能: 强制同步日志信息到日志文件
 **输入参数: 
 **     file: 日志文件信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
static int log_sync_ext(log_cycle_t *log)
{
    size_t fsize = 0;

    /* 1. 执行同步操作 */
    fsize = _log_sync(log->file, &log->fd);

    /* 2. 文件是否过大 */
    if(log_is_too_large(fsize))
    {
        Close(log->fd);
        return log_rename(log->file, &log->file->sync_tm);
    }

    return 0;
}

/******************************************************************************
 ** Name : _log_sync
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
static size_t _log_sync(log_file_info_t *file, int *fd)
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
    loc_fd = (NULL != fd)? *fd : INVALID_FD;

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
                fprintf(stderr, "errmsg:[%d]%s path:[%s]",
                        errno, strerror(errno), file->path);
                break;
            }
            Close(loc_fd);
            Mkdir2(file->path, DIR_MODE);
        }

        /* 4. 是否重新创建文件 */
        if(loc_fd < 0)
        {
            loc_fd = Open(file->path, OPEN_FLAGS, OPEN_MODE);
            if(loc_fd < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s! path:[%s]",
                        errno, strerror(errno), file->path);
                break;
            }
        }

        /* 5. 定位到文件末尾 */
        fsize = lseek(loc_fd, 0, SEEK_END);
        if(-1 == fsize)
        {
            Close(loc_fd);
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]",
                    errno, strerror(errno), file->path);
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
    ftime(&file->sync_tm);

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
#endif /*__ASYNC_LOG__*/

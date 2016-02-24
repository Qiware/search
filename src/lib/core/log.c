/*******************************************************************************
 ** 模  块: 异步日志模块 - 客户端代码
 ** 说  明:
 **     考虑性能要求, 日志首先存储在缓冲区中, 当缓存区中的日志达到一定量、或超
 **     过一定时间未同步、或出现错误级别日志时, 则将日志同步到日志文件中.
 ** 注  意:
 **     不同的"进程"和"线程"是不能使用的同名的日志文件
 ** 作  者: # Qifeng.zou # 2013.11.07 #
 ******************************************************************************/
#include "log.h"
#include "comm.h"
#include "lock.h"
#include "redo.h"
#include "shm_opt.h"
#include "hash_alg.h"

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
#define log_fcache_wrlock(lc) proc_spin_wrlock_b(g_log_lock_fd, (lc)->idx+1)  /* 加缓存写锁 */
#define log_fcache_rdlock(lc) proc_spin_rdlock_b(g_log_lock_fd, (lc)->idx+1)  /* 加缓存写锁 */
#define log_fcache_unlock(lc) proc_unlock_b(g_log_lock_fd, (lc)->idx+1)  /* 解缓存锁 */
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
static const size_t g_log_data_size =  (LOG_FILE_CACHE_SIZE - sizeof(log_cache_t));
#define log_get_data_size() (g_log_data_size)

#define log_hash(path) (hash_time33(path) % LOG_FILE_MAX_NUM)  /* 异步日志哈希 */
#define log_is_err_level(level) \
    (LOG_LEVEL_ERROR & (level) || LOG_LEVEL_FATAL & (level))

/* 函数声明 */
static int _log_init_global(const char *key_path);
static log_cache_t *log_alloc(void *addr, const char *path);
static void log_dealloc(log_cache_t *lc);
static int log_write(log_cycle_t *log, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *ctm);
static int log_print_dump(char *addr, const void *dump, int dumplen);
static int log_sync_ext(log_cycle_t *log);

static int log_rename(const log_cache_t *lc, const struct timeb *time);
static size_t _log_sync(log_cache_t *lc, int *fd);

/* 是否强制写(注意: 系数必须小于或等于0.8，否则可能出现严重问题) */
static const size_t g_log_sync_size = 0.8 * LOG_FILE_CACHE_SIZE;
#define log_is_over_limit(lc) (((lc)->ioff - (lc)->ooff) > g_log_sync_size)

/******************************************************************************
 **函数名称: log_init
 **功    能: 初始化日志信息
 **输入参数:
 **     level: 日志级别(其值：LOG_LEVEL_TRACE~LOG_LEVEL_FATAL的"或"值)
 **     path: 日志路径
 **     key_path: 键值路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 日志模块初始化
 **     2. 将日志信息写入共享内存
 **注意事项: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
log_cycle_t *log_init(int level, const char *path, const char *key_path)
{
    int ret;
    void *addr;
    log_cycle_t *log;

    Mkdir2(path, DIR_MODE);

    /* 1. 新建日志对象 */
    log = (log_cycle_t *)calloc(1, sizeof(log_cycle_t));
    if (NULL == log) {
        return NULL;
    }

    log->level = level;
    log->pid = getpid();
    pthread_mutex_init(&log->lock, NULL);

    log_mutex_lock();

    do {
        /* 2. 初始化全局数据 */
        ret = _log_init_global(key_path);
        if (ret < 0) {
            fprintf(stderr, "Initialize trace log failed!");
            break;
        }

        /* 3. 完成日志对象的创建 */
        addr = log_get_shm_addr();

        log->lc = log_alloc(addr, path);
        if (NULL == log->lc) {
            fprintf(stderr, "Create [%s] failed!", path);
            break;
        }

        /* 4. 新建日志文件 */
        log->fd = Open(path, OPEN_FLAGS, OPEN_MODE);
        if (log->fd < 0) {
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
            break;
        }

        log_mutex_unlock();
	    return log;
    } while (0);

    /* 5. 异常处理 */
    if (NULL != log->lc) { log_dealloc(log->lc); }
    log_mutex_unlock();
    pthread_mutex_destroy(&log->lock);
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
 **注意事项: 日志级别的判断在函数外进行判断
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

    log_dealloc((*log)->lc);
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
 **注意事项: 当级别匹配失败时，默认情况下将开启所有日志级别.
 **作    者: # Qifeng.zou # 2014.09.03 #
 ******************************************************************************/
int log_get_level(const char *level_str)
{
    if (!strcasecmp(level_str, LOG_LEVEL_FATAL_STR)) {
        return LOG_LEVEL_FATAL;
    }
    else if (!strcasecmp(level_str, LOG_LEVEL_ERROR_STR)) {
        return LOG_LEVEL_ERROR;
    }
    else if (!strcasecmp(level_str, LOG_LEVEL_WARN_STR)) {
        return LOG_LEVEL_WARN;
    }
    else if (!strcasecmp(level_str, LOG_LEVEL_INFO_STR)) {
        return LOG_LEVEL_INFO;
    }
    else if (!strcasecmp(level_str, LOG_LEVEL_DEBUG_STR)) {
        return LOG_LEVEL_DEBUG;
    }
    else if (!strcasecmp(level_str, LOG_LEVEL_TRACE_STR)) {
        return LOG_LEVEL_TRACE;
    }

    return LOG_LEVEL_TRACE;
}

/******************************************************************************
 **函数名称: log_get_str
 **功    能: 获取日志级别字串
 **输入参数:
 **     level: 日志级别
 **输出参数: NONE
 **返    回: 日志级别字串
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
const char *log_get_str(int level)
{
    switch (level) {
        case LOG_LEVEL_FATAL:
        {
            return LOG_LEVEL_FATAL_STR;
        }
        case LOG_LEVEL_ERROR:
        {
            return LOG_LEVEL_ERROR_STR;
        }
        case LOG_LEVEL_WARN:
        {
            return LOG_LEVEL_WARN_STR;
        }
        case LOG_LEVEL_INFO:
        {
            return LOG_LEVEL_INFO_STR;
        }
        case LOG_LEVEL_DEBUG:
        {
            return LOG_LEVEL_DEBUG_STR;
        }
        case LOG_LEVEL_TRACE:
        {
            return LOG_LEVEL_TRACE_STR;
        }
        default:
        {
            return LOG_LEVEL_UNKNOWN_STR;
        }
    }
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
 **     lc: 文件信息
 **     time: 当前时间
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int log_rename(const log_cache_t *lc, const struct timeb *time)
{
    struct tm loctm;
    char newpath[FILE_PATH_MAX_LEN];

    local_time(&time->time, &loctm);

    /* FORMAT: *.logYYYYMMDDHHMMSS.bak */
    snprintf(newpath, sizeof(newpath),
        "%s%04d%02d%02d%02d%02d%02d.bak",
        lc->path, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

    return rename(lc->path, newpath);
}

/******************************************************************************
 **函数名称: _log_init_global
 **功    能: 初始化全局数据
 **输入参数:
 **     key_path: 键值路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 连接共享内存
 **     2. 打开消息队列
 **注意事项: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int _log_init_global(const char *key_path)
{
    int fd = 0;
    void *addr = log_get_shm_addr();
    char path[FILE_PATH_MAX_LEN];

    /* 1. 连接共享内存 */
    if (!log_is_shm_addr_valid()) {
        addr = shm_creat(key_path, LOG_SHM_SIZE);
        if (NULL == addr) {
            fprintf(stderr, "[%s][%d] errmsg:[%d] %s!\n",
                    __FILE__, __LINE__, errno, strerror(errno));
            return -1;
        }

        log_set_shm_addr(addr);
    }

    if (!log_is_lock_fd_valid()) {
        log_get_lock_path(path, sizeof(path), key_path);

        Mkdir2(path, DIR_MODE);

        fd = Open(path, OPEN_FLAGS, OPEN_MODE);
        if (fd < 0) {
            fprintf(stderr, "[%s][%d] errmsg:[%d] %s! [%s]\n",
                    __FILE__, __LINE__, errno, strerror(errno), path);
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
 **返    回: 0:成功 !0:失败
 **实现描述: 发生冲突时, 则日志名上追加序列号
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.05 #
 ******************************************************************************/
static int log_name_conflict_handler(const char *oripath, char *newpath, int size, int idx)
{
    int len;
    char *ptr;
    char suffix[FILE_NAME_MAX_LEN];

    snprintf(newpath, size, "%s", oripath);
    snprintf(suffix, sizeof(suffix), "-%03d.log", idx);

    len = strlen(newpath) + strlen(suffix) - strlen(LOG_SUFFIX);
    if (len >= size) {
        fprintf(stderr, "Not enough memory! newpath:[%s] suffix:[%s] len:[%d]/[%d]",
            newpath, suffix, len, size);
        return -1;  /* Not enough memory */
    }

    ptr = strrchr(newpath, '.');
    if (NULL == ptr) {
        strcat(newpath, suffix);
        return 0;
    }

    if (!strcasecmp(ptr, LOG_SUFFIX)) {
        sprintf(ptr, "%s", suffix);
    }
    else {
        strcat(ptr, suffix);
    }

    return 0;
}

/******************************************************************************
 **函数名称: log_alloc
 **功    能: 创建日志信息
 **输入参数:
 **     addr: 共享内存首地址
 **     path: 日志绝对路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 判断当前功能内存中是否有文件名一致的日志
 **     2. 选择合适的日志缓存，并返回
 **注意事项:
 **     1. 当存在日志名一致的日志时, 判断进程ID是否一样.
 **        如果不一样, 且对应进程, 依然正在运行, 则进行日志命名冲突处理!
 **     2. 请勿在此函数中调用错误日志函数 - 小心死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static log_cache_t *log_alloc(void *addr, const char *path)
{
    pid_t pid = getpid();
    log_cache_t *lc;
    const char *ptr = path;
    char newpath[FILE_NAME_MAX_LEN];
    int idx, repeat = 0, free_idx = -1;

    log_fcache_all_wrlock();

    for (idx=0; idx<LOG_FILE_MAX_NUM; ++idx) {
        lc = (log_cache_t *)(addr + idx * LOG_FILE_CACHE_SIZE);

        /* 1. 判断文件名是否一致 */
        if (!strcmp(lc->path, ptr)) { /* 文件名出现一致 */
            /* 判断进程是否存在，是否和当前进程ID一致 */
            if (lc->pid == pid) {
                log_fcache_all_unlock();
                return lc;
            }
            else if (!proc_is_exist(lc->pid)) {
                lc->pid = pid;
                log_sync(lc);

                log_fcache_all_unlock();
                return lc;
            }

            /* 文件名重复，且其他进程正在运行... */
            if (log_name_conflict_handler(path, newpath, sizeof(newpath), ++repeat)) {
                log_fcache_all_unlock();
                return NULL;
            }

            ptr = newpath;
            free_idx = -1;
            idx = -1;
            continue;
        }
        else if (-1 == free_idx) {/* 当未找到空闲时，则进行后续判断 */
            /* 判断该缓存是否真的被占用
             * 1. 路径是否异常
             * 2. 进程是否存在 */
            if ('\0' == lc->path[0]
                || !proc_is_exist(lc->pid))
            {
                free_idx = idx;

                memset(lc, 0, sizeof(log_cache_t));
                lc->idx = idx;
                lc->pid = INVALID_PID;
                continue;
            }
        }
    }

    if (-1 == free_idx) {
        log_fcache_all_unlock();
        return NULL;
    }

    lc = (log_cache_t *)(addr + free_idx * LOG_FILE_CACHE_SIZE);

    lc->pid = pid;
    snprintf(lc->path, sizeof(lc->path), "%s", ptr);

    log_fcache_all_unlock();

    return lc;
}

/******************************************************************************
 **函数名称: log_dealloc
 **功    能: 释放申请的日志缓存
 **输入参数:
 **     lc: 日志对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.11.05 #
 ******************************************************************************/
static void log_dealloc(log_cache_t *lc)
{
    int idx;

    idx = lc->idx;

    log_fcache_wrlock(lc);

    log_sync(lc);

    memset(lc, 0, sizeof(log_cache_t));
    lc->pid = INVALID_PID;
    lc->idx = idx;

    log_fcache_unlock(lc);
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
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int log_write(log_cycle_t *log, int level,
    const void *dump, int dumplen, const char *errmsg, const struct timeb *ctm)
{
    int msglen, left;
    char *addr;
    struct tm loctm;
    time_t difftm;
    log_cache_t *lc = log->lc;

    local_time(&ctm->time, &loctm);        /* 获取当前系统时间 */

    log_fcache_wrlock(lc);                /* 缓存加锁 */
    pthread_mutex_lock(&log->lock);

    addr = (char *)(lc + 1) + lc->ioff;
    left = log_get_data_size() - lc->ioff;

    switch (level) {
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
    lc->ioff += msglen;
    addr += msglen;
    left -= msglen;

    /* 打印DUMP数据 */
    if ((NULL != dump) && (dumplen > 0) && (left > dumplen)) {
        msglen = log_print_dump(addr, dump, dumplen);

        lc->ioff += msglen;
    }

    /* 判断是否强制写或发送通知 */
    difftm = ctm->time - lc->sync_tm.time;
    if (log_is_over_limit(lc)
        || log_is_timeout(difftm))
    {
        memcpy(&lc->sync_tm, ctm, sizeof(lc->sync_tm));

        log_sync_ext(log);
    }

    pthread_mutex_unlock(&log->lock);
    log_fcache_unlock(lc);      /* 缓存解锁 */

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
    const char *dump_ptr, *dump_end;
    unsigned char var[2] = {0, 31};
    int idx, n, row, count, rows, head_len;

    dump_ptr = (const char *)dump;
    dump_end = dump + dumplen;              /* 内存结束地址 */
    rows = (dumplen - 1)/LOG_DUMP_COL_NUM;  /* 每页行数 */
    head_len = strlen(LOG_DUMP_HEAD_STR);

    while (dump_ptr < dump_end) {
        for (row=0; row<=rows; row++) {
            /* 1. 判断是否打印头部字串 */
            if (0 == (row + 1)%LOG_DUMP_PAGE_MAX_ROWS) {
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
            for (idx=0; (idx<LOG_DUMP_COL_NUM) && (dump_ptr<dump_end); idx++) {
                sprintf(in, "%02x ", *dump_ptr);
                in += 3;
                dump_ptr++;
            }

            /* >>3.2 最后数据不足一行时，使用空格补上 */
            for (n=0; n<LOG_DUMP_COL_NUM-idx; n++) {
                sprintf(in, "   ");
                in += 3;
            }

            sprintf(in, " ");
            in++;
            dump_ptr -= idx;

            /* >>3.3 以字符方式打印信息 */
            for (n=0; n<idx; n++) {
                if (((unsigned char)(*dump_ptr) <= (var[1]))
                    &&    ((unsigned char)(*dump_ptr) >= (var[0])))
                {
                    *(in++) = '*';
                }
                else {
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
 **     lc: 日志文件信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
void log_sync(log_cache_t *lc)
{
    size_t fsize;

    /* 1. 执行同步操作 */
    fsize = _log_sync(lc, NULL);

    /* 2. 文件是否过大 */
    if (log_is_too_large(fsize)) {
        log_rename(lc, &lc->sync_tm);
    }
}

/******************************************************************************
 **函数名称: log_sync_ext
 **功    能: 强制同步日志信息到日志文件
 **输入参数:
 **     lc: 日志文件信息
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
    fsize = _log_sync(log->lc, &log->fd);

    /* 2. 文件是否过大 */
    if (log_is_too_large(fsize)) {
        CLOSE(log->fd);
        return log_rename(log->lc, &log->lc->sync_tm);
    }

    return 0;
}

/******************************************************************************
 **函数名称: _log_sync
 **功    能: 强制同步业务日志
 **输入参数:
 **     lc: 文件缓存
 **输出参数:
 **     fd: 文件描述符
 **返    回: 如果进行同步操作，则返回文件的实际大小!
 **实现描述:
 **注意事项:
 **     1) 一旦撰写日志失败，需要清除缓存中的日志，防止内存溢出，导致严重后果!
 **     2) 当fd为空指针时，打开的文件需要关闭.
 **     3) 在此函数中不允许调用错误级别的日志函数 可能死锁!
 **作    者: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
static size_t _log_sync(log_cache_t *lc, int *_fd)
{
    void *addr;
    struct stat fstat;
    int n, fd = -1, fsize = 0;

    memset(&fstat, 0, sizeof(fstat));

    /* 1. 判断是否需要同步 */
    if (lc->ioff == lc->ooff) {
        return 0;
    }

    /* 2. 计算同步地址和长度 */
    addr = (void *)(lc + 1);
    n = lc->ioff - lc->ooff;
    fd = (NULL != _fd)? *_fd : INVALID_FD;

    /* 撰写日志文件 */
    do {
        /* 3. 文件是否存在 */
        if (lstat(lc->path, &fstat) < 0) {
            if (ENOENT != errno) {
                CLOSE(fd);
                fprintf(stderr, "errmsg:[%d]%s path:[%s]\n", errno, strerror(errno), lc->path);
                break;
            }
            CLOSE(fd);
            Mkdir2(lc->path, DIR_MODE);
        }

        /* 4. 是否重新创建文件 */
        if (fd < 0) {
            fd = Open(lc->path, OPEN_FLAGS, OPEN_MODE);
            if (fd < 0) {
                fprintf(stderr, "errmsg:[%d] %s! path:[%s]\n", errno, strerror(errno), lc->path);
                break;
            }
        }

        /* 5. 定位到文件末尾 */
        fsize = lseek(fd, 0, SEEK_END);
        if (-1 == fsize) {
            CLOSE(fd);
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]\n", errno, strerror(errno), lc->path);
            break;
        }

        /* 6. 写入指定日志文件 */
        Writen(fd, addr, n);

        fsize += n;
    } while(0);

    /* 7. 标志复位 */
    memset(addr, 0, n);
    lc->ioff = 0;
    lc->ooff = 0;
    ftime(&lc->sync_tm);

    if (NULL != _fd) {
        *_fd = fd;
    }
    else {
        CLOSE(fd);
    }
    return fsize;
}

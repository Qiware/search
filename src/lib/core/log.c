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
#include "redo.h"

static size_t g_log_max_size = LOG_FILE_MAX_SIZE;
#define _log_set_max_size(size) (g_log_max_size = (size))
#define log_is_too_large(size) ((size) >= g_log_max_size)

/* 日志缓存数据空间大小 */
static const size_t g_log_data_size =  (LOG_FILE_CACHE_SIZE - sizeof(log_cache_t));
#define log_get_data_size() (g_log_data_size)

/* 函数声明 */
static void *log_sync_proc(void *_ctx);
static log_cache_t *log_alloc(const char *path);
static void log_dealloc(log_cache_t *lc);
static int log_write(log_cycle_t *log, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *ctm);
static int log_print_dump(char *addr, const void *dump, int dumplen);

static int log_rename(const log_cache_t *lc, const struct timeb *time);
static size_t _log_sync(log_cache_t *lc, int *fd);
static int _log_sync_proc(log_cycle_t *log, void *args);

static int log_insert_cycle(log_cntx_t *ctx, log_cycle_t *log);

/* 是否强制写(注意: 系数必须小于或等于0.8，否则可能出现严重问题) */
static const size_t g_log_sync_size = 0.8 * LOG_FILE_CACHE_SIZE;
#define log_is_over_limit(lc) (((lc)->ioff - (lc)->ooff) > g_log_sync_size)

/******************************************************************************
 **函数名称: log_creat
 **功    能: 初始化日志信息
 **输入参数:
 **     ctx: 日志服务
 **     level: 日志级别(其值：LOG_LEVEL_TRACE~LOG_LEVEL_FATAL的"或"值)
 **     path: 日志路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 日志模块初始化
 **     2. 将日志信息写入共享内存
 **注意事项: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
log_cycle_t *log_creat(log_cntx_t *ctx, int level, const char *path)
{
    log_cache_t *lc;
    log_cycle_t *log;

    Mkdir2(path, DIR_MODE);

    /* > 新建日志对象 */
    log = (log_cycle_t *)calloc(1, sizeof(log_cycle_t));
    if (NULL == log) {
        return NULL;
    }

    log->owner = ctx;
    log->level = level;
    log->pid = getpid();
    pthread_mutex_init(&log->lock, NULL);

    do {
        /* > 创建日志缓存 */
        lc = (log_cache_t *)calloc(1, LOG_FILE_CACHE_SIZE);
        if (NULL == lc) {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        lc->pid = getpid();
        snprintf(lc->path, sizeof(lc->path), "%s", path);
        log->lc = lc;

        /* > 打开日志文件 */
        log->fd = Open(path, OPEN_FLAGS, OPEN_MODE);
        if (log->fd < 0) {
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
            break;
        }

        log_insert_cycle(ctx, log);

	    return log;
    } while (0);

    /* 5. 异常处理 */
    if (NULL != log->lc) { free(log->lc); }
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
    _log_set_max_size(size);
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
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static log_cache_t *log_alloc(const char *path)
{
    log_cache_t *lc;

    lc = (log_cache_t *)calloc(1, LOG_FILE_CACHE_SIZE);
    if (NULL == lc) {
        return NULL;
    }

    lc->pid = getpid();
    snprintf(lc->path, sizeof(lc->path), "%s", path);

    return lc;
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

        log_sync(log);
    }

    pthread_mutex_unlock(&log->lock);

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
int log_sync(log_cycle_t *log)
{
    size_t fsize;

    pthread_mutex_lock(&log->lock);

    /* 1. 执行同步操作 */
    fsize = _log_sync(log->lc, &log->fd);

    /* 2. 文件是否过大 */
    if (log_is_too_large(fsize)) {
        CLOSE(log->fd);
        pthread_mutex_unlock(&log->lock);
        return log_rename(log->lc, &log->lc->sync_tm);
    }

    pthread_mutex_unlock(&log->lock);

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
    struct stat st;
    int n, fd = -1, fsize = 0;

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
        if (lstat(lc->path, &st) < 0) {
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

/******************************************************************************
 **函数名称: log_insert_cycle
 **功    能: 添加日志对象
 **输入参数:
 **     ctx: 日志服务
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.03.19 #
 ******************************************************************************/
static int log_insert_cycle(log_cntx_t *ctx, log_cycle_t *log)
{
    pthread_mutex_lock(&ctx->lock);
    avl_insert(ctx->logs, (void *)log, sizeof(log), (void *)log);
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

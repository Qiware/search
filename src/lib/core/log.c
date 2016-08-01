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

size_t g_log_max_size = LOG_MAX_SIZE;

/* 函数声明 */
static int log_write(log_cycle_t *log, int level,
        const void *dump, int dumplen, const char *msg, const struct timeb *ctm);
static int log_print_dump(char *addr, const void *dump, int dumplen);

/******************************************************************************
 **函数名称: log_init
 **功    能: 初始化日志信息
 **输入参数:
 **     level: 日志级别(其值：LOG_LEVEL_TRACE~LOG_LEVEL_FATAL的"或"值)
 **     path: 日志路径
 **输出参数: NONE
 **返    回: 日志对象
 **实现描述:
 **注意事项: 此函数中不能调用错误日志函数 - 可能死锁!
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
log_cycle_t *log_init(int level, const char *path)
{
    log_svr_t *lsvr;
    log_cycle_t *log;

    Mkdir2(path, DIR_MODE);

    /* > 新建日志服务 */
    lsvr = log_svr_init();
    if (NULL == lsvr) {
        return NULL;
    }

    /* > 新建日志对象 */
    log = (log_cycle_t *)calloc(1, sizeof(log_cycle_t));
    if (NULL == log) {
        return NULL;
    }

    log->owner = lsvr;
    log->level = level;
    log->pid = getpid();
    pthread_mutex_init(&log->lock, NULL);

    do {
        /* > 创建日志缓存 */
        log->text = (char *)calloc(1, LOG_MEM_SIZE);
        if (NULL == log->text) {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        log->pid = getpid();
        log->size = LOG_MEM_SIZE;
        snprintf(log->path, sizeof(log->path), "%s", path);

        /* > 打开日志文件 */
        log->fd = Open(path, OPEN_FLAGS, OPEN_MODE);
        if (log->fd < 0) {
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
            break;
        }

        log_insert(lsvr, log);

	    return log;
    } while (0);

    /* 5. 异常处理 */
    FREE(log->text);
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
                const char *fname, int lineno, const char *func,
                const void *dump, int dumplen,
                const char *fmt, ...)
{
    int len;
    va_list args;
    struct timeb ctm;
    char errmsg[LOG_MSG_MAX_LEN];

    va_start(args, fmt);
    len = snprintf(errmsg, sizeof(errmsg), "[%s][%d]%s() ", fname, lineno, func);
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

    local_time(&ctm->time, &loctm);        /* 获取当前系统时间 */

    pthread_mutex_lock(&log->lock);

    addr = log->text + log->inoff;
    left = log->size - log->inoff;

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
    log->inoff += msglen;
    addr += msglen;
    left -= msglen;

    /* 打印DUMP数据 */
    if ((NULL != dump) && (dumplen > 0) && (left > dumplen)) {
        msglen = log_print_dump(addr, dump, dumplen);
        log->inoff += msglen;
    }

    /* 判断是否强制同步 */
    if (left <= 0.8 * log->size) {
        memcpy(&log->sync_tm, ctm, sizeof(log->sync_tm));
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

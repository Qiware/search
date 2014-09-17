/******************************************************************************
 * Copyright(C) 2014-2024 Xundao All Right Reserved 
 * FileName: log.h
 * Version:  1. 0
 * Description: 打印日志信息
 * Author:   # Qifeng.zou # 2014.08.27 #
 ******************************************************************************/
#if !defined(__LOG_H__)
#define __LOG_H__

#include <sys/timeb.h>
#include "common.h"

/* 宏定义 */

/* 日志级别 */
typedef enum
{
    LOG_LEVEL_FATAL                         /* 严重级别 */
    , LOG_LEVEL_ERROR                       /* 错误级别 */
    , LOG_LEVEL_WARN                        /* 警告级别 */
    , LOG_LEVEL_INFO                        /* 信息级别 */
    , LOG_LEVEL_DEBUG                       /* 调试级别 */
    , LOG_LEVEL_TRACE                       /* 跟踪级别 */

    , LOG_LEVEL_TOTAL                       /* 级别总数 */
} log_level_e;

#define LOG_LEVEL_FATAL_STR     "fatal"     /* 严重级别字串 */
#define LOG_LEVEL_ERROR_STR     "error"     /* 错误级别字串 */
#define LOG_LEVEL_WARN_STR      "warn"      /* 警告级别字串 */
#define LOG_LEVEL_INFO_STR      "info"      /* 信息级别字串 */
#define LOG_LEVEL_DEBUG_STR     "debug"     /* 调试级别字串 */
#define LOG_LEVEL_TRACE_STR     "trace"     /* 跟踪级别字串 */
#define LOG_DEF_LEVEL_STR       LOG_LEVEL_TRACE_STR /* 默认日志级别字串 */

#define LOG_FILE_MAX_NUM        (32)        /* 日志文件最大个数 */
#define LOG_FILE_CACHE_SIZE     (1 * MB)    /* 日志文件的缓存SIZE */
#define LOG_SHM_SIZE                        /* 日志共享内存SIZE */\
    (LOG_FILE_MAX_NUM * LOG_FILE_CACHE_SIZE)
#define LOG_SYNC_TIMEOUT        (5)         /* 日志超时同步时间 */
#define LOG_LEVEL_MAX_LEN       (16)        /* 日志级别字串的长度 */

#define LOG_SHM_KEY             (0x32313123)/* 共享内存KEY */

#define LOG_MSG_MAX_LEN         (2048)      /* 日志行最大长度 */
#define LOG_SVR_THREAD_NUM      (1)         /* 服务线程数 */
#define LOG_FILE_MAX_SIZE       (128 * MB)  /* 单个日志文件的最大SIZE */

#define LOG_SUFFIX              ".log"      /* 日志文件后缀 */
#define LOG_DEFAULT_TRCLOG      "trc.log"   /* 默认日志名 */
#define log_default_path(path, size)        /* 默认日志文件路径 */ \
    snprintf(path, size, "../logs/%s", LOG_DEFAULT_TRCLOG);
#define LOG_LOCK_FILE           "log_cache.lck" /* 日志锁 */
#define log_get_lock_path(path, size)       /* 日志锁路径 */ \
    snprintf(path, size, "../tmp/log/%s", LOG_LOCK_FILE)

/* DUMP设置 */
#define LOG_DUMP_COL_NUM        (16)        /* DUMP列数 */
#define LOG_DUMP_PAGE_MAX_ROWS  (20)        /* DUMP页最大行数 */
#define LOG_DUMP_PAGE_MAX_SIZE  (2048)      /* DUMP页最大SIZE */
#define LOG_DUMP_HEAD_STR     \
    "\nDisplacement -1--2--3--4--5--6--7--8-Hex-0--1--2--3--4--5--6  --ASCII Value--\n"

#define log_is_timeout(diff_time) (diff_time >= LOG_SYNC_TIMEOUT)

/* 文件缓存信息 */
typedef struct
{
    int idx;                                /* 索引号 */
    char path[FILE_NAME_MAX_LEN];           /* 日志文件绝对路径 */
    size_t in_offset;                       /* 写入偏移 */
    size_t out_offset;                      /* 同步偏移 */
    pid_t pid;                              /* 使用日志缓存的进程ID */
    struct timeb sync_tm;                   /* 上次同步的时间 */
} log_file_info_t;

/* 日志共享内存信息 */
typedef struct
{
    int max_num;                            /* 日志缓存数 */
    char path[FILE_NAME_MAX_LEN];           /* 日志文件绝对路径 */
    size_t in_offset;                       /* 写入偏移 */
    size_t out_offset;                      /* 同步偏移 */
    pid_t pid;                              /* 使用日志缓存的进程ID */
    struct timeb sync_tm;                   /* 上次同步的时间 */
} log_shm_t;

/* 内部接口 */
int log_set_path(const char *path);
void log_sync(log_file_info_t *file);
void log_set_max_size(size_t size);

/* 日志对象 */
typedef struct _log_cycle_t
{
    int level;                              /* 日志级别 */

    int fd;                                 /* 文件描述符 */
    pid_t pid;                              /* 进程PID */
    log_file_info_t *file;                  /* 文件信息 */
} log_cycle_t;

/* 外部接口 */
int log_get_level(const char *level_str);
log_cycle_t *log_init(int level, const char *path);
void log_core(log_cycle_t *log, int level,
                const char *fname, int lineno,
                const void *dump, int dumplen,
                const char *fmt, ...);
#define log_get_path(path, size, name) \
            snprintf(path, size, "../log/%s.log", name)
log_cycle_t *log_get_cycle(void);
void log_destroy(log_cycle_t **log);

/* 日志模块接口 */
#define log_fatal(log, ...) /* 撰写FATAL级别日志 */\
    if ((log)->level <= LOG_LEVEL_FATAL) \
        log_core(log, LOG_LEVEL_FATAL, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log_error(log, ...) /* 撰写ERROR级别日志 */\
    if ((log)->level <= LOG_LEVEL_ERROR) \
        log_core(log, LOG_LEVEL_ERROR, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log_warn(log, ...)  /* 撰写WARN级别日志 */\
    if ((log)->level <= LOG_LEVEL_WARN) \
        log_core(log, LOG_LEVEL_WARN, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log_info(log, ...)  /* 撰写INFO级别日志 */\
    if ((log)->level <= LOG_LEVEL_INFO) \
        log_core(log, LOG_LEVEL_INFO, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log_debug(log, ...) /* 撰写DEBUG级别日志 */\
    if ((log)->level <= LOG_LEVEL_DEBUG) \
        log_core(log, LOG_LEVEL_DEBUG, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log_trace(log, ...) /* 撰写TRACE级别日志 */\
    if ((log)->level <= LOG_LEVEL_TRACE) \
        log_core(log, LOG_LEVEL_TRACE, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log_bin(log, addr, len, ...)   /* 撰写MEM-DUMP日志 */\
    if ((log)->level <= LOG_LEVEL_INFO) \
        log_core(log, LOG_LEVEL_INFO, __FILE__, __LINE__, addr, len, __VA_ARGS__)

#define log_fatal_ex(...)
#define log_error_ex(...)
#define log_warn_ex(...)
#define log_info_ex(...)
#define log_debug_ex(...)
#define log_bin_ex(...)


/* 日志对象 */
typedef struct
{
    FILE *fp;       /* 文件指针 */
    int level;      /* 进程级别 */
    pid_t pid;      /* 进程ID */
} log2_cycle_t;

extern log2_cycle_t g_log2;

/* 系统日志函数(无句柄) */
int log2_init(const char *level, const char *path);
void log2_core(int level, const char *fname, int lineno,
            const void *dump, int dumplen, const char *fmt, ...);
int log2_get_level(const char *level_str);
void log2_destroy(void);

#define log2_fatal(...) /* 撰写FATAL级别日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_FATAL, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log2_error(...) /* 撰写ERROR级别日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_ERROR, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log2_warn(...)  /* 撰写WARN级别日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_WARN, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log2_info(...)  /* 撰写INFO级别日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_INFO, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log2_debug(...) /* 撰写DEBUG级别日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_DEBUG, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log2_trace(...) /* 撰写TRACE级别日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_TRACE, __FILE__, __LINE__, NULL, 0, __VA_ARGS__)
#define log2_bin(addr, len, ...)   /* 撰写MEM-DUMP日志 */\
    if (g_log2.level <= LOG_LEVEL_FATAL) \
        log2_core(LOG_LEVEL_INFO, __FILE__, __LINE__, addr, len, __VA_ARGS__)

#endif /*__LOG_H__*/

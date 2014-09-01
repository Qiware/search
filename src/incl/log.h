/******************************************************************************
 * Copyright(C) 2013-2014 Xundao All Right Reserved 
 * FileName: xdt_log.h
 * Author:   # Qifeng.zou # 2014.08.27 #
 * Version:  1. 0
 * Description:  撰写系统各模块的日志信息
 ******************************************************************************/
#if !defined(__LOG_H__)
#define __LOG_H__

#include "common.h"

#include <sys/timeb.h>

/* 错误级别: 依次递减 */
#define LOG_LEVEL_FATAL     (0x0001)    /* 严重级别 */
#define LOG_LEVEL_ERROR     (0x0002)    /* 错误级别 */
#define LOG_LEVEL_WARN      (0x0004)    /* 警告级别 */
#define LOG_LEVEL_INFO      (0x0008)    /* 信息级别 */
#define LOG_LEVEL_DEBUG     (0x0010)    /* 调试级别 */
#define LOG_LEVEL_TRACE     (0x0020)    /* 跟踪级别 */
#define LOG_LEVEL_TOTAL     (6)         /* 日志级别总数 */

#define LOG_INVALID_PID     (-1)        /* 非法进程ID */
#define LOG_INVALID_FD      (-1)        /* 非法文件描述符 */

/* 文件缓存信息 */
typedef struct
{
    int idx;                            /* 索引号 */
    char path[FILE_NAME_MAX_LEN];       /* 日志文件绝对路径 */
    size_t in_offset;                   /* 写入偏移 */
    size_t out_offset;                  /* 同步偏移 */
    pid_t pid;                          /* 使用日志缓存的进程ID */
    struct timeb sync_time;             /* 上次同步的时间 */
}log_file_t;

/* 日志对象 */
typedef struct
{
    int fd;                             /* 描述符 */
    int level;                          /* 日志级别: LOG_LEVEL_TRACE ~ LOG_LEVEL_FATAL */
    char name[FILE_NAME_MAX_LEN];       /* 文件名 */
    char path[FILE_NAME_MAX_LEN];       /* 文件路径 */
    size_t max_size;                    /* 日志文件的最大尺寸 */
    log_file_t *file;                   /* 日志缓存 */
}log_cycle_t;

/* 日志接口 */
log_cycle_t *log_init(int level, const char *fname, size_t max_size);
void log_core(
        log_cycle_t *log,
        int level,
        const char *fname, int lineno,
        const char *fmt, ...);

#if 0
/* 请使用如下接口 */
#define log_fatal(log, ...) /* 撰写严重日志 */\
    if ((log)->level & XDR_LOG_LEVEL_FATAL) \
        log_core(XDR_LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(log, ...) /* 撰写错误日志 */\
    if ((log)->level & XDR_LOG_LEVEL_ERROR) \
        log_core(XDR_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(log, ...)  /* 撰写警告日志 */\
    if ((log)->level & XDR_LOG_LEVEL_WARN) \
        log_core(XDR_LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(log, ...)  /* 撰写信息日志 */\
    if ((log)->level & XDR_LOG_LEVEL_INFO) \
        log_core(XDR_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(log, ...) /* 撰写调试日志 */\
    if ((log)->level & XDR_LOG_LEVEL_DEBUG) \
        log_core(XDR_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_trace(log, ...) /* 撰写跟踪日志 */\
    if ((log)->level & XDR_LOG_LEVEL_TRACE) \
        log_core(XDR_LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
#define log_fatal(...)
#define log_error(...)
#define log_warn(...)
#define log_info(...)
#define log_debug(...)
#endif
#endif /*__LOG_H__*/

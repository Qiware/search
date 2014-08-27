/******************************************************************************
 * Copyright(C) 2013-2014 Xundao All Right Reserved 
 * FileName: xdt_log.h
 * Author:   # Qifeng.zou # 2014.08.27 #
 * Version:  1. 0
 * Description:  撰写系统各模块的日志信息
 ******************************************************************************/
#if !defined(__XDT_LOG_H__)
#define __XDT_LOG_H__

/* 错误级别: 依次递减 */
#define XDT_LOG_LEVEL_FATAL     (0x0001)    /* 严重级别 */
#define XDT_LOG_LEVEL_ERROR     (0x0002)    /* 错误级别 */
#define XDT_LOG_LEVEL_WARN      (0x0004)    /* 警告级别 */
#define XDT_LOG_LEVEL_INFO      (0x0008)    /* 信息级别 */
#define XDT_LOG_LEVEL_DEBUG     (0x0010)    /* 调试级别 */
#define XDT_LOG_LEVEL_TRACE     (0x0020)    /* 跟踪级别 */

/* 日志对象 */
typedef struct
{
    uint32_t level;         /* 日志级别: 其中由各日志级别通过或操作得来 */
    char name[FILE_NAME_MAX_LEN];   /* 文件名 */
    char path[FILE_NAME_MAX_LEN];   /* 文件路径 */
    uint16_t max_index;     /* 日志最大索引号 */
    size_t max_size;        /* 日志文件的最大尺寸 */
}xdt_log_t;

/* 日志接口 */
void xdt_log_core(
        xdt_log_t *log,
        int level,
        const char *fname, int lineno,
        const char *fmt, ...);

/* 请使用如下接口 */
#define log_fatal(log, ...) /* 撰写严重日志 */\
    if ((log)->level & XDR_LOG_LEVEL_FATAL) \
        xdt_log_core(XDR_LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(log, ...) /* 撰写错误日志 */\
    if ((log)->level & XDR_LOG_LEVEL_ERROR) \
        xdt_log_core(XDR_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(log, ...)  /* 撰写警告日志 */\
    if ((log)->level & XDR_LOG_LEVEL_WARN) \
        xdt_log_core(XDR_LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(log, ...)  /* 撰写信息日志 */\
    if ((log)->level & XDR_LOG_LEVEL_INFO) \
        xdt_log_core(XDR_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(log, ...) /* 撰写调试日志 */\
    if ((log)->level & XDR_LOG_LEVEL_DEBUG) \
        xdt_log_core(XDR_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_trace(log, ...) /* 撰写跟踪日志 */\
    if ((log)->level & XDR_LOG_LEVEL_TRACE) \
        xdt_log_core(XDR_LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

#endif /*__XDT_LOG_H__*/

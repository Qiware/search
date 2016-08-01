#if !defined(__LOG_H__)
#define __LOG_H__

#include "comm.h"
#include "avl_tree.h"
#include "thread_pool.h"

/* 日志级别 */
typedef enum
{
    LOG_LEVEL_TRACE                         /* 跟踪级别 */
    , LOG_LEVEL_DEBUG                       /* 调试级别 */
    , LOG_LEVEL_INFO                        /* 信息级别 */
    , LOG_LEVEL_WARN                        /* 警告级别 */
    , LOG_LEVEL_ERROR                       /* 错误级别 */
    , LOG_LEVEL_FATAL                       /* 严重级别 */

    , LOG_LEVEL_TOTAL                       /* 级别总数 */
} log_level_e;

/* 宏定义 */
#define LOG_LEVEL_TRACE_STR     "trace"     /* 跟踪级别字串 */
#define LOG_LEVEL_DEBUG_STR     "debug"     /* 调试级别字串 */
#define LOG_LEVEL_INFO_STR      "info"      /* 信息级别字串 */
#define LOG_LEVEL_WARN_STR      "warn"      /* 警告级别字串 */
#define LOG_LEVEL_ERROR_STR     "error"     /* 错误级别字串 */
#define LOG_LEVEL_FATAL_STR     "fatal"     /* 严重级别字串 */
#define LOG_LEVEL_UNKNOWN_STR   "unknown"   /* 未知级别字串 */
#define LOG_DEF_LEVEL_STR       LOG_LEVEL_TRACE_STR /* 默认日志级别字串 */

#define LOG_MEM_SIZE            (1 * MB)    /* 日志缓存SIZE */
#define LOG_SYNC_TIMEOUT        (1)         /* 日志超时同步时间 */

#define LOG_MSG_MAX_LEN         (2048)      /* 日志行最大长度 */
#define LOG_MAX_SIZE            (128 * MB)  /* 单个日志文件的最大SIZE */

#define LOG_SUFFIX              ".log"      /* 日志文件后缀 */

/* DUMP设置 */
#define LOG_DUMP_COL_NUM        (16)        /* DUMP列数 */
#define LOG_DUMP_PAGE_MAX_ROWS  (20)        /* DUMP页最大行数 */
#define LOG_DUMP_PAGE_MAX_SIZE  (2048)      /* DUMP页最大SIZE */
#define LOG_DUMP_HEAD_STR                   /* 显示头 */\
    "\nDisplacement -1--2--3--4--5--6--7--8-Hex-0--1--2--3--4--5--6  --ASCII Value--\n"

#define log_is_timeout(diff_time) (diff_time >= LOG_SYNC_TIMEOUT)

/* 日志服务 */
typedef struct
{
    thread_pool_t *tp;                      /* 线程池 */
    int timeout;                            /* 同步超时 */

    avl_tree_t *logs;                       /* 日志列表(管理log_cycle_t对象) */
    pthread_mutex_t lock;                   /* 锁 */
} log_svr_t;

/* 日志对象 */
typedef struct _log_cycle_t
{
    int level;                              /* 日志级别 */
    log_svr_t *owner;                       /* 所属服务 */
    char path[FILE_NAME_MAX_LEN];           /* 日志文件绝对路径 */

    pid_t pid;                              /* 进程PID */

    struct {
        int fd;                             /* 文件描述符 */
        size_t size;                        /* 缓存SIZE */
        size_t inoff;                       /* 写入偏移 */
        size_t outoff;                      /* 同步偏移 */
        char *text;                         /* 日志缓存首地址 */
        struct timeb sync_tm;               /* 上次同步的时间 */
        pthread_mutex_t lock;               /* 线程互斥锁 */
    };
} log_cycle_t;

/* 外部接口 */
log_svr_t *log_svr_init(void);
int log_get_level(const char *level_str);
const char *log_get_str(int level);
log_cycle_t *log_init(int level, const char *path);
#define log_set_level(log, _level) { (log)->level = (_level); }
void log_core(log_cycle_t *log, int level,
                const char *fname, int lineno, const char *func,
                const void *dump, int dumplen,
                const char *fmt, ...);
#define log_get_path(path, size, name) \
            snprintf(path, size, "../log/%s.log", name)

/* 日志模块接口 */
#define log_fatal(log, ...) /* 撰写FATAL级别日志 */\
    if (NULL != (log) && LOG_LEVEL_FATAL >= (log)->level) \
        log_core(log, LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, NULL, 0, __VA_ARGS__)
#define log_error(log, ...) /* 撰写ERROR级别日志 */\
    if (NULL != (log) && LOG_LEVEL_ERROR >= (log)->level) \
        log_core(log, LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, NULL, 0, __VA_ARGS__)
#define log_warn(log, ...)  /* 撰写WARN级别日志 */\
    if (NULL != (log) && LOG_LEVEL_WARN >= (log)->level) \
        log_core(log, LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, NULL, 0, __VA_ARGS__)
#define log_info(log, ...)  /* 撰写INFO级别日志 */\
    if (NULL != (log) && LOG_LEVEL_INFO >= (log)->level) \
        log_core(log, LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, NULL, 0, __VA_ARGS__)
#define log_debug(log, ...) /* 撰写DEBUG级别日志 */\
    if (NULL != (log) && LOG_LEVEL_DEBUG >= (log)->level) \
        log_core(log, LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, NULL, 0, __VA_ARGS__)
#define log_trace(log, ...) /* 撰写TRACE级别日志 */\
    if (NULL != (log) && LOG_LEVEL_TRACE >= (log)->level) \
        log_core(log, LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, NULL, 0, __VA_ARGS__)
#define log_bin(log, addr, len, ...)   /* 撰写MEM-DUMP日志 */\
    if (NULL != (log) && LOG_LEVEL_TRACE >= (log)->level) \
        log_core(log, LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, addr, len, __VA_ARGS__)

/* 内部接口 */
int log_insert(log_svr_t *lsvr, log_cycle_t *log);
int log_sync(log_cycle_t *log);

extern size_t g_log_max_size;
#define _log_set_max_size(size) (g_log_max_size = (size))
#define log_is_too_large(size) ((size) >= g_log_max_size)

void log_set_max_size(size_t size);

#endif /*__LOG_H__*/

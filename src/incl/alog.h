#if !defined(__ALOG_H__)
#define __ALOG_H__

#if defined(__ASYNC_LOG__)

#include <sys/timeb.h>

#include "common.h"
#include "thread_pool.h"

/* 功能宏开关 */
#define ALOG_ERR_FORCE      (__ON__)    /* 开关：出现错误日志时，强制同步对应缓存数据 */
#define ALOG_SVR_SYNC       (__OFF__)   /* 开关：允许服务进程同时进行日志同步 */

#if defined(ALOG_ERR_FORCE) && (__ON__ == ALOG_ERR_FORCE)
    #define __ALOG_ERR_FORCE__          /* 出现错误日志时，强制同步对应缓存数据 */
#endif
#if defined(ALOG_SVR_SYNC) && (__ON__ == ALOG_SVR_SYNC)
    #define __ALOG_SVR_SYNC__           /* 允许服务进程同时进行日志同步 */
#endif

/* 同步机制配置信息 */
#define ALOG_FILE_MAX_NUM       (512)           /* 日志缓存最大个数 */
#define ALOG_FILE_CACHE_SIZE    (64*1024)       /* 文件缓存尺寸(单个) */
#define ALOG_SHM_SIZE           (ALOG_FILE_MAX_NUM * ALOG_FILE_CACHE_SIZE)  /* 日志共享内存大小 */
#define ALOG_SYNC_TIMEOUT       (3)             /* 同步超时时间 */

#define ALOG_SHM_KEY            (0x32313123)    /* 共享内存KEY */

#define ALOG_OPEN_MODE          (0666)          /* 设置日志权限 */
#define ALOG_OPEN_FLAGS         (O_CREAT|O_WRONLY|O_APPEND) /* 打开日志标识 */
#define ALOG_DIR_MODE           (0777)          /* 设置目录权限 */

#define ALOG_MSG_MAX_LEN        (2048)          /* 日志最大长度(单条) */
#define ALOG_SVR_THREAD_NUM     (1)             /* 日志线程数目 */
#define ALOG_FILE_MAX_SIZE      (8*1024*1024)   /* 日志文件最大尺寸 */

#define ALOG_SUFFIX             ".log"          /* 日志文件后缀 */
#define ALOG_DEFAULT_ERRLOG     "error.log"     /* 默认错误日志 */
#define ALOG_DEFAULT_TRCLOG     "trc.log"       /* 默认跟踪日志 */
#define AlogErrlogDefPath(path, size)           /* 默认错误日志路径 */ \
    snprintf(path, size, "../logs/syslog/%s", ALOG_DEFAULT_ERRLOG);
#define AlogTrclogDefPath(path, size)           /* 默认跟踪日志路径 */ \
    snprintf(path, size, "../logs/syslog/%s", ALOG_DEFAULT_TRCLOG);
#define ALOG_LOCK_FILE          ".alog.lck"     /* 锁文件名 */
#define AlogGetLockPath(path, size)             /* 锁文件件的路径 */ \
    snprintf(path, size, "../tmp/alog/%s", ALOG_LOCK_FILE)

#define ALOG_INVALID_PID            (-1)        /* 非法进程ID */
#define ALOG_INVALID_FD             (-1)        /* 非法文件描述符 */

/* 打印指定内存的数据 */
#define ALOG_DUMP_COL_NUM           (16)        /* 16进制: 每行列数 */
#define ALOG_DUMP_LINE_MAX_SIZE     (512)       /* 16进制: 每行大小 */
#define ALOG_DUMP_PAGE_MAX_LINE     (20)        /* 16进制: 每页行数 */
#define ALOG_DUMP_PAGE_MAX_SIZE     (2048)      /* 16进制: 每页大小 */
#define ALOG_DUMP_HEAD_STR     \
    "\nDisplacement -1--2--3--4--5--6--7--8-Hex-0--1--2--3--4--5--6  --ASCII Value--\n"

#define AlogIsTimeout(diff_time) (diff_time >= ALOG_SYNC_TIMEOUT)

/* 日志级别 */
typedef enum
{
    LOG_LEVEL_DEBUG,                /* 调试级别 */
    LOG_LEVEL_INFO,                 /* 信息级别 */
    LOG_LEVEL_WARNING,              /* 警告级别 */
    LOG_LEVEL_ERROR,                /* 错误级别 */

    LOG_LEVEL_UNKNOWN,              /* 未知级别 */
    LOG_LEVEL_TOTAL
}alog_level_e;

/* 日志命令类型 */
typedef enum
{
    ALOG_CMD_INVALID,               /* 非法命令 */
    ALOG_CMD_SYNC,                  /* 同步命令 */

    ALOG_CMD_UNKNOWN,               /* 未知命令 */
    ALOG_CMD_TOTAL = ALOG_CMD_UNKNOWN /* 命令数目 */
}alog_cmd_e;

/* 日志参数信息 */
typedef enum
{
    ALOG_PARAM_ERR,                 /* 错误日志路径参数 */
    ALOG_PARAM_TRC,                 /* 跟踪日志路径参数 */
    ALOG_PARAM_SIZE,                /* 日志大小参数 */

    ALOG_PARAM_UNKNOWN,             /* 未知参数 */
    ALOG_PARAM_TOTAL = ALOG_PARAM_UNKNOWN
}alog_param_e;

/* 日志命令结构 */
typedef struct
{
    int idx;                        /* 缓存索引 */
    alog_cmd_e type;                /* 日志命令类型 */
}alog_cmd_t;

/* 文件缓存信息 */
typedef struct
{
    int idx;                        /* 索引号 */
    char path[FILE_NAME_MAX_LEN];   /* 日志文件绝对路径 */
    size_t in_offset;               /* 写入偏移 */
    size_t out_offset;              /* 同步偏移 */
    pid_t pid;                      /* 使用日志缓存的进程ID */
    struct timeb sync_time;         /* 上次同步的时间 */
}alog_file_t;

/* 日志生命周期 */
typedef struct _alog_cycle_t
{
    int fd;                         /* 日志描述符 */
    alog_file_t *file;              /* 文件缓存 */
    pid_t pid;                      /* 当前进程ID */
    int (*action)(struct _alog_cycle_t *cycle, int level, /* 日志行为 */
            const void *dump, int dumplen, const char *msg, const struct timeb *curr_time);
}alog_cycle_t;

/* 日志服务进程 */
typedef struct
{
    int fd;                         /* 文件锁FD */
    void *addr;                     /* 共享内存首地址 */
    thread_pool_t *pool;            /* 线程池 */
}alog_svr_t;

/* 对外接口 */
extern void alog_set_level(alog_level_e level);
extern void log_core(int level, const char *fname, int lineno, const void *dump, int dumplen, const char *fmt, ...);

/* 内部接口 */
int alog_set_path(int type, const char *path);
extern int alog_trclog_sync(alog_file_t *file);
extern void alog_set_max_size(size_t size);
void alog_abnormal(const char *fname, int lineno, const char *fmt, ...);

#if defined(__ALOG_SVR_SYNC__)
#define alog_file_is_pid_live(file) (0 == proc_is_exist((file)->pid))
#endif /*__ALOG_SVR_SYNC__*/
#define alog_file_reset_pid(file) ((file)->pid = ALOG_INVALID_PID)

#if 1 /* 接口适配 */

#define log_error(...) log_core(LOG_LEVEL_ERROR, NULL, 0, NULL, 0, __VA_ARGS__)
#define log_debug(...) log_core(LOG_LEVEL_DEBUG, NULL, 0, NULL, 0, __VA_ARGS__)
#define log_info(...) log_core(LOG_LEVEL_INFO, NULL, 0, NULL, 0, __VA_ARGS__)
#define log_bin(level, addr, len, ...) log_core(level, NULL, 0, addr, len, __VA_ARGS__)

#define log_set_syslog_level(level) alog_set_level(level)
#define log_set_applog_level(level) ulog_set_level(level)
#endif

#endif /*__ASYNC_LOG__*/
#endif /*__ALOG_H__*/

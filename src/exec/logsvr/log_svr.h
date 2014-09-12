/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: log_svr.h
 ** 版本号: 1.0
 ** 描  述: 异步日志模块 - 服务端代码
 **         1. 负责共享内存的初始化
 **         2. 负责把共享内存中的日志同步到指定文件。
 ** 作  者: # Qifeng.zou # 2013.11.07 #
 ******************************************************************************/
#if !defined(__LOG_SVR_H__)
#define __LOG_SVR_H__

#include "lock.h"
#include "thread_pool.h"

/* 服务进程互斥锁路径 */
#define LOG_SVR_PROC_LOCK  "log_svr.lck"
#define log_svr_proc_lock_path(path, size) \
    snprintf(path, size, "../tmp/%s",  LOG_SVR_PROC_LOCK)
#define log_svr_proc_trylock(fd) proc_try_wrlock(fd)

/* 服务进程日志文件路径 */
#define LOG_SVR_LOG_NAME   "log_svr.log"
#define log_svr_log_path(path, size) \
    snprintf(path, size, "../logs/%s", LOG_SVR_LOG_NAME)

/* 日志服务 */
typedef struct
{
    int fd;                         /* 文件描述符 */
    void *addr;                     /* 共享内存首地址 */
    thread_pool_t *pool;            /* 内存池对象 */
}log_svr_t;

#endif /*__LOG_SVR_H__*/

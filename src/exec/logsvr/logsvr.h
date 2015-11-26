/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
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
#include "slab.h"
#include "thread_pool.h"

#define LOGD_THREAD_NUM     (1)     /* 服务线程数 */

/* 服务进程互斥锁路径 */
#define logd_proc_trylock(fd) proc_try_wrlock(fd)

/* 服务进程日志文件路径 */
#define LOGD_LOG_NAME   "log_svr.log"
#define logd_log_path(path, size) snprintf(path, size, "../log/%s", LOGD_LOG_NAME)

/* 输入选项 */
typedef struct
{
    bool isdaemon;                  /* 后台运行 */
    char *key_path;                 /* 键值路径 */
} logd_opt_t;

/* 日志服务 */
typedef struct
{
    int fd;                         /* 文件描述符 */
    void *addr;                     /* 共享内存首地址 */
#define LOGD_SLAB_SIZE   (1 * MB)
    slab_pool_t *slab;              /* 内存池 */
    thread_pool_t *pool;            /* 内存池对象 */
} logd_cntx_t;

#endif /*__LOG_SVR_H__*/

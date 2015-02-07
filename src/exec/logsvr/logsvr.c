/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: logsvr.c
 ** 版本号: 1.0
 ** 描  述: 异步日志模块 - 服务端代码
 **         1. 负责共享内存的初始化
 **         2. 负责把共享内存中的日志同步到指定文件。
 ** 作  者: # Qifeng.zou # 2013.11.07 #
 ******************************************************************************/
#include <sys/shm.h>
#include <sys/types.h>

#include "log.h"
#include "lock.h"
#include "common.h"
#include "logsvr.h"
#include "shm_opt.h"
#include "syscall.h"
#include "thread_pool.h"

#define LOG_SVR_LOG2_PATH   "../log/logsvr.syslog"

static int logsvr_init(logsvr_t *logsvr);
static void *logsvr_timeout_routine(void *args);
int logsvr_sync_work(int idx, logsvr_t *logsvr);
static int logsvr_proc_lock(void);

static char *logsvr_creat_shm(int fd);

/******************************************************************************
 **函数名称: main 
 **功    能: 日志服务主程序
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 完成日志模块的初始化
 **     2. 启动命令接收和超时处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.28 #
 ******************************************************************************/
int main(void)
{
    int ret;
    logsvr_t logsvr;

    memset(&logsvr, 0, sizeof(logsvr_t));

    daemon(1, 0);

    /* 1. 初始化系统日志 */
    ret = syslog_init(LOG_LEVEL_DEBUG, LOG_SVR_LOG2_PATH);
    if (0 != ret)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    /* 2. 初始化日志服务 */
    ret = logsvr_init(&logsvr);
    if(ret < 0)
    {
        syslog_error("Init log failed!");
        return -1;
    }

    /* 3. 启动超时扫描线程 */
    thread_pool_add_worker(logsvr.pool, logsvr_timeout_routine, &logsvr);

    while(1){ pause(); }
    return 0;
}

/******************************************************************************
 **函数名称: logsvr_proc_lock
 **功    能: 日志服务进程锁，防止同时启动两个服务进程
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.06 #
 ******************************************************************************/
int logsvr_proc_lock(void)
{
    int ret = 0, fd = 0;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取服务进程锁文件路径 */
    logsvr_proc_lock_path(path, sizeof(path));

    Mkdir2(path, DIR_MODE);

    /* 2. 打开服务进程锁文件 */
    fd = Open(path, OPEN_FLAGS, OPEN_MODE);
    if(fd < 0)
    {
        syslog_error("errmsg:[%d]%s! path:[%s]", errno, strerror(errno), path);
        return -1;
    }

    /* 3. 尝试加锁 */
    ret = logsvr_proc_trylock(fd);
    if(ret < 0)
    {
        syslog_error("errmsg:[%d]%s! path:[%s]", errno, strerror(errno), path);
        Close(fd);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: logsvr_init
 **功    能: 初始化处理
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.28 #
 ******************************************************************************/
static int logsvr_init(logsvr_t *logsvr)
{
    int ret;
    char path[FILE_PATH_MAX_LEN];

    /* 设置跟踪日志路径 */
    logsvr_log_path(path, sizeof(path));

    /* 1. 加服务进程锁 */
    ret = logsvr_proc_lock();
    if(ret < 0)
    {
        syslog_error("Log server is already running...");
        return -1;    /* 日志服务进程正在运行... */
    }

    /* 2. 打开文件缓存锁 */
    log_get_lock_path(path, sizeof(path));

    Mkdir2(path, DIR_MODE);

    logsvr->fd = Open(path, OPEN_FLAGS, OPEN_MODE);
    if(logsvr->fd < 0)
    {
        syslog_error("errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
        return -1;
    }

    /* 3. 创建/连接共享内存 */
    logsvr->addr = logsvr_creat_shm(logsvr->fd);
    if(NULL == logsvr->addr)
    {
        syslog_error("Create SHM failed!");
        return -1;
    }

    /* 4. 启动多个线程 */
    logsvr->pool = thread_pool_init(LOG_SVR_THREAD_NUM, 0);
    if(NULL == logsvr->pool)
    {
        thread_pool_destroy(logsvr->pool);
        logsvr->pool = NULL;
        syslog_error("errmsg:[%d]%s!", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: logsvr_creat_shm
 **功    能: 创建或连接共享内存
 **输入参数: 
 **输出参数: 
 **     cfg: 日志配置信息
 **返    回: Address of SHM
 **实现描述: 
 **     1. 创建共享内存
 **     2. 连接共享内存
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.28 #
 ******************************************************************************/
static char *logsvr_creat_shm(int fd)
{
    key_t key;
    int idx, shmid;
    void *addr = NULL, *p = NULL;
    log_file_info_t *file = NULL;

    key = shm_ftok(LOG_KEY_PATH, 0);
    if (-1 == key)
    {
        return NULL;
    }

    /* 1. 创建共享内存 */
    /* 1.1 判断是否已经创建 */
    shmid = shmget(key, 0, 0666);
    if(shmid >= 0)
    {
        return shmat(shmid, NULL, 0);  /* 已创建 */
    }

    /* 1.2 异常，则退出处理 */
    if(ENOENT != errno)
    {
        return NULL;
    }

    /* 1.3 创建共享内存 */
    shmid = shmget(key, LOG_SHM_SIZE, IPC_CREAT|0666);
    if(shmid < 0)
    {
        return NULL;
    }

    /* 2. ATTACH共享内存 */
    addr = (void *)shmat(shmid, NULL, 0);
    if((void *)-1 == addr)
    {
        syslog_error("Attach shm failed! shmid:[%d] key:[0x%x]", shmid, key);
        return NULL;
    }

    /* 3. 初始化共享内存 */
    p = addr;
    for(idx=0; idx<LOG_FILE_MAX_NUM; idx++)
    {
        file = (log_file_info_t *)(p + idx * LOG_FILE_CACHE_SIZE);

        proc_spin_wrlock_b(fd, idx+1);
        
        file->idx = idx;
        file->pid = INVALID_PID;

        proc_unlock_b(fd, idx+1);
    }

    return addr;
}

/******************************************************************************
 **函数名称: logsvr_timeout_routine
 **功    能: 日志超时处理
 **输入参数: 
 **     args: 参数
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 睡眠指定时间
 **     2. 依次遍历日志缓存
 **     3. 进行超时判断
 **     4. 进行缓存同步处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.25 #
 ******************************************************************************/
static void *logsvr_timeout_routine(void *args)
{
    int idx;
    struct timeb ctm;
    log_file_info_t *file = NULL;
    logsvr_t *logsvr = (logsvr_t *)args;


    while(1)
    {
        memset(&ctm, 0, sizeof(ctm));

        ftime(&ctm);

        for(idx=0; idx<LOG_FILE_MAX_NUM; idx++)
        {
            /* 1. 尝试加锁 */
            proc_spin_wrlock_b(logsvr->fd, idx+1);

            /* 2. 路径为空，则不用同步 */
            file = (log_file_info_t *)(logsvr->addr + idx*LOG_FILE_CACHE_SIZE);
            if('\0' == file->path[0])
            {
                proc_unlock_b(logsvr->fd, idx+1);
                continue;
            }

            log_sync(file);
        
            /* 判断文件是否还有运行的进程正在使用文件缓存 */
            if(!proc_is_exist(file->pid))
            {
                memset(file, 0, sizeof(log_file_info_t));

                file->pid = INVALID_PID;
            }
            file->idx = idx;
            
            proc_unlock_b(logsvr->fd, idx+1);
        }
        
        Sleep(LOG_SYNC_TIMEOUT);
    }

    return (void *)-1;
}

/******************************************************************************
 **函数名称: logsvr_sync_work
 **功    能: 日志同步处理
 **输入参数: 
 **     idx: 缓存索引
 **     logsvr: 日志服务对象
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 缓存加锁
 **     2. 写入文件
 **     3. 缓存解锁
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.28 #
 ******************************************************************************/
int logsvr_sync_work(int idx, logsvr_t *logsvr)
{
    log_file_info_t *file = NULL;

    file = (log_file_info_t *)(logsvr->addr + idx*LOG_FILE_CACHE_SIZE);
    
    log_sync(file);
    
    return 0;
}

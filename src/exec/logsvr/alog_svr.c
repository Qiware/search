/*******************************************************************************
 * 模块: 异步日志模块 - 服务端代码
 * 作用: 
 *      1) 负责共享内存的初始化
 *      2) 负责把死亡的客户端进程的在缓存中日志同步到文件
 * 作者: # Qifeng.zou # 2013.11.07 #
 ******************************************************************************/
#include <sys/shm.h>
#include <sys/types.h>

#include "alog.h"
#include "common.h"

#if defined(__ASYNC_LOG__) || defined(__ASYNC_ULOG__)

/* 服务进程互斥锁路径 */
#define ALOG_SVR_PROC_LOCK  ".alogsvr.lck"
#define AlogSvrGetProcLockPath(path, size) \
    snprintf(path, size, "../tmp/%s",  ALOG_SVR_PROC_LOCK)
#define AlogSvrTryLockProc(fd) proc_try_wrlock(fd)

/* 服务进程日志文件路径 */
#define ALOG_SVR_LOG_NAME   "swalogsvr.log"
#define AlogSvrGetLogPath(path, size) \
    snprintf(path, size, "../logs/%s", ALOG_SVR_LOG_NAME)

static alog_svr_t g_logsvr;     /* 日志服务对象 */

static int alog_svr_init(alog_svr_t *logsvr);
static void *alog_svr_timeout_routine(void *args);
int alog_svr_sync_work(int idx, alog_svr_t *logsvr);
static int alog_svr_proc_lock(void);

static char *alog_svr_creat_shm(int fd);

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
    alog_svr_t *logsvr = &g_logsvr;

    memset(logsvr, 0, sizeof(alog_svr_t));

    daemon(0, 0);

    /* 1. 初始化日志系统 */
    ret = alog_svr_init(logsvr);
    if(ret < 0)
    {
        fprintf(stderr, "Init log failed!");
        return -1;
    }

    thread_pool_add_worker(logsvr->pool, alog_svr_timeout_routine, logsvr);

    while(1){ pause(); }
    return 0;
}

/******************************************************************************
 **函数名称: alog_svr_proc_lock
 **功    能: 日志服务进程锁，防止同时启动两个服务进程
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.11.06 #
 ******************************************************************************/
int alog_svr_proc_lock(void)
{
    int ret = 0, fd = 0;
    char path[FILE_PATH_MAX_LEN] = {0};

    /* 1. 获取服务进程锁文件路径 */
    AlogSvrGetProcLockPath(path, sizeof(path));

    Mkdir2(path, ALOG_DIR_MODE);

    /* 2. 打开服务进程锁文件 */
    fd = Open(path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
    if(fd < 0)
    {
        fprintf(stderr, "errmsg:[%d]%s! path:[%s]", errno, strerror(errno), path);
        return -1;
    }

    /* 3. 尝试加锁 */
    ret = AlogSvrTryLockProc(fd);
    if(ret < 0)
    {
        fprintf(stderr, "errmsg:[%d]%s! path:[%s]", errno, strerror(errno), path);
        Close(fd);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: alog_svr_init
 **功    能: 初始化处理
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.10.28 #
 ******************************************************************************/
static int alog_svr_init(alog_svr_t *logsvr)
{
    int ret = 0;
    char path[FILE_PATH_MAX_LEN] = {0};

    /* 设置跟踪日志路径 */
    AlogSvrGetLogPath(path, sizeof(path));

    alog_set_path(ALOG_PARAM_TRC, path);
    
    /* 1. 加服务进程锁 */
    ret = alog_svr_proc_lock();
    if(ret < 0)
    {
        fprintf(stderr, "Alog server is already running...");
        return -1;    /* 日志服务进程正在运行... */
    }

    /* 2. 打开文件缓存锁 */
    AlogGetLockPath(path, sizeof(path));

    Mkdir2(path, ALOG_DIR_MODE);

    logsvr->fd = Open(path, ALOG_OPEN_FLAGS, ALOG_OPEN_MODE);
    if(logsvr->fd < 0)
    {
        fprintf(stderr, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), path);
        return -1;
    }

    /* 3. 创建/连接共享内存 */
    logsvr->addr = alog_svr_creat_shm(logsvr->fd);
    if(NULL == logsvr->addr)
    {
        fprintf(stderr, "Create SHM failed!");
        return -1;
    }

    /* 4. 启动多个线程 */
    ret = thread_pool_init(&logsvr->pool, ALOG_SVR_THREAD_NUM);
    if(ret < 0)
    {
        thread_pool_destroy(logsvr->pool);
        logsvr->pool = NULL;
        fprintf(stderr, "errmsg:[%d]%s!", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: alog_svr_creat_shm
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
static char *alog_svr_creat_shm(int fd)
{
    int idx, shmid;
    void *addr = NULL, *p = NULL;
    alog_file_t *file = NULL;

    /* 1. 创建共享内存 */
    /* 1.1 判断是否已经创建 */
    shmid = shmget(ALOG_SHM_KEY, 0, 0666);
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
    shmid = shmget(ALOG_SHM_KEY, ALOG_SHM_SIZE, IPC_CREAT|0660);
    if(shmid < 0)
    {
        return NULL;
    }

    /* 2. ATTACH共享内存 */
    addr = (void *)shmat(shmid, NULL, 0);
    if((void *)-1 == addr)
    {
        fprintf(stderr, "Attach shm failed! shmid:[%d] key:[0x%x]", shmid, ALOG_SHM_KEY);
        return NULL;
    }

    /* 3. 初始化共享内存 */
    p = addr;
    for(idx=0; idx<ALOG_FILE_MAX_NUM; idx++)
    {
        file = (alog_file_t *)(p + idx * ALOG_FILE_CACHE_SIZE);

        proc_wrlock_b(fd, idx+1);
        
        file->idx = idx;
        file->pid = ALOG_INVALID_PID;

        proc_unlock_b(fd, idx+1);
    }

    return addr;
}

/******************************************************************************
 **函数名称: alog_svr_timeout_routine
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
static void *alog_svr_timeout_routine(void *args)
{
    int ret = 0, idx = 0;
    struct timeb curr_time;
    alog_file_t *file = NULL;
    alog_svr_t *logsvr = (alog_svr_t *)args;


    while(1)
    {
        memset(&curr_time, 0, sizeof(curr_time));

        ftime(&curr_time);

        for(idx=0; idx<ALOG_FILE_MAX_NUM; idx++)
        {
            /* 1. 尝试加锁 */
            ret = proc_wrlock_b(logsvr->fd, idx+1);
            if(ret < 0)
            {
                continue;
            }

            /* 2. 路径为空，则不用同步 */
            file = (alog_file_t *)(logsvr->addr + idx*ALOG_FILE_CACHE_SIZE);
            if('\0' == file->path[0])
            {
                proc_unlock_b(logsvr->fd, idx+1);
                continue;
            }

        #if !defined(__ALOG_SVR_SYNC__)
            /* 3. 申请日志的进程依然在运行，则不用同步 */
            if((file->pid <= 0)
                || (0 == proc_is_exist(file->pid)))
            {
                proc_unlock_b(logsvr->fd, idx+1);
                continue;
            }
        #endif /*!__ALOG_SVR_SYNC__*/

            alog_trclog_sync(file);
        
        #if defined(__ALOG_SVR_SYNC__)
            /* 判断文件是否还有运行的进程正在使用文件缓存 */
            if(!alog_file_is_pid_live(file))
         #endif /*__ALOG_SVR_SYNC__*/
            {
                memset(file, 0, sizeof(alog_file_t));
                alog_file_reset_pid(file);
            }
            file->idx = idx;
            
            proc_unlock_b(logsvr->fd, idx+1);
        }
        
        Sleep(ALOG_SYNC_TIMEOUT);
    }

    return (void *)-1;
}

/******************************************************************************
 **函数名称: alog_svr_sync_work
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
int alog_svr_sync_work(int idx, alog_svr_t *logsvr)
{
    alog_file_t *file = NULL;

    file = (alog_file_t *)(logsvr->addr + idx*ALOG_FILE_CACHE_SIZE);
    
    alog_trclog_sync(file);
    
    return 0;
}
#endif /*__ASYNC_LOG__ || __ASYNC_ULOG__*/

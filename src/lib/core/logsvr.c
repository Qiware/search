/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: logsvr.c
 ** 版本号: 1.0
 ** 描  述:
 ** 作  者: # Qifeng.zou # 2016年03月19日 星期六 19时56分51秒 #
 ******************************************************************************/
#include "log.h"
#include "redo.h"
#include "comm.h"

static log_svr_t *g_log_svr = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static size_t log_sync_to_disk(log_cycle_t *log);
static int log_rename(const log_cycle_t *log, const struct timeb *time);
static void *log_sync_proc(void *_lsvr);
static int _log_sync_proc(log_cycle_t *log, void *args);

/******************************************************************************
 **函数名称: log_svr_cmp_cb
 **功    能: 比较回调函数
 **输入参数: 
 **     log1: 日志对象1
 **     log2: 日志对象2
 **输出参数: NONE
 **返    回: 0:相等 <0:小于 >0:大于
 **实现描述: 比较函数指针是否等
 **注意事项: 无需比较文件路径
 **作    者: # Qifeng.zou # 2016.08.09 11:16:21 #
 ******************************************************************************/
static int log_svr_cmp_cb(const log_cycle_t *log1, const log_cycle_t *log2)
{
    return ((uint64_t)log1 - (uint64_t)log2);
}

/******************************************************************************
 **函数名称: log_svr_init
 **功    能: 初始化日志服务
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 日志服务
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.03.19 18:06:45 #
 ******************************************************************************/
log_svr_t *log_svr_init(void)
{
    log_svr_t *lsvr;

    if (NULL != g_log_svr) {
        return g_log_svr;
    }

    pthread_mutex_lock(&g_log_mutex);

    if (NULL != g_log_svr) {
        pthread_mutex_unlock(&g_log_mutex);
        return g_log_svr;
    }

    do {
        /* > 创建日志服务对象 */
        lsvr = (log_svr_t *)calloc(1, sizeof(log_svr_t));
        if (NULL == lsvr) {
            break;
        }

        g_log_svr = lsvr;
        lsvr->timeout = 1;

        lsvr->logs = avl_creat(NULL, (cmp_cb_t)log_svr_cmp_cb);
        if (NULL == lsvr->logs) {
            break;
        }

        lsvr->tp = thread_pool_init(1, NULL, (void *)lsvr);
        if (NULL == lsvr->tp) {
            break;
        }

        /* > 执行同步操作 */
        thread_pool_add_worker(lsvr->tp, log_sync_proc, (void *)lsvr);

        pthread_mutex_unlock(&lsvr->lock);
        return lsvr;
    } while (0);

    pthread_mutex_unlock(&lsvr->lock);
    return lsvr;
}

/******************************************************************************
 **函数名称: log_sync_proc
 **功    能: 同步日志
 **输入参数:
 **     lsvr: 全局对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static void *log_sync_proc(void *_lsvr)
{
    log_svr_t *lsvr = (log_svr_t *)_lsvr;

    while (1) {
        pthread_mutex_lock(&lsvr->lock);
        avl_trav(lsvr->logs, (trav_cb_t)_log_sync_proc, NULL);
        pthread_mutex_unlock(&lsvr->lock);
        Sleep(lsvr->timeout);
    }
    return (void *)NULL;
}

/******************************************************************************
 **函数名称: _log_sync_proc
 **功    能: 同步日志
 **输入参数:
 **     log: 日志对象
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.03.19 #
 ******************************************************************************/
static int _log_sync_proc(log_cycle_t *log, void *args)
{
    pthread_mutex_lock(&log->lock);
    log_sync(log);
    pthread_mutex_unlock(&log->lock);
    return 0;
}

/******************************************************************************
 **函数名称: log_sync
 **功    能: 强制同步日志信息到日志文件
 **输入参数:
 **     log: 日志对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项: 请在函数外部加锁
 **作    者: # Qifeng.zou # 2013.10.30 #
 ******************************************************************************/
int log_sync(log_cycle_t *log)
{
    size_t sz;

    /* 1. 执行同步操作 */
    sz = log_sync_to_disk(log);

    /* 2. 文件是否过大 */
    if (log_is_too_large(sz)) {
        CLOSE(log->fd);
        return log_rename(log, &log->sync_tm);
    }

    return 0;
}

/******************************************************************************
 **函数名称: log_sync_to_disk
 **功    能: 强制日志到磁盘
 **输入参数:
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 如果进行同步操作，则返回文件的实际大小!
 **实现描述:
 **注意事项:
 **     1) 一旦撰写日志失败，需要清除缓存中的日志，防止内存溢出，导致严重后果!
 **     2) 当fd为空指针时，打开的文件需要关闭.
 **     3) 在此函数中不允许调用错误级别的日志函数 可能死锁!
 **作    者: # Qifeng.zou # 2013.11.08 #
 ******************************************************************************/
static size_t log_sync_to_disk(log_cycle_t *log)
{
    void *addr;
    struct stat st;
    int n, sz = 0;

    /* 1. 判断是否需要同步 */
    if (log->inoff == log->outoff) {
        return 0;
    }

    /* 2. 计算同步地址和长度 */
    addr = log->text;
    n = log->inoff - log->outoff;

    /* 撰写日志文件 */
    do {
        /* 3. 文件是否存在 */
        if (lstat(log->path, &st) < 0) {
            if (ENOENT != errno) {
                fprintf(stderr, "errmsg:[%d] %s! path:[%s]\n", errno, strerror(errno), log->path);
                CLOSE(log->fd);
                break;
            }
            CLOSE(log->fd);
            Mkdir2(log->path, DIR_MODE);
        }

        /* 4. 是否重新创建文件 */
        if (log->fd < 0) {
            log->fd = Open(log->path, OPEN_FLAGS, OPEN_MODE);
            if (log->fd < 0) {
                fprintf(stderr, "errmsg:[%d] %s! path:[%s]\n", errno, strerror(errno), log->path);
                break;
            }
        }

        /* 5. 定位到文件末尾 */
        sz = lseek(log->fd, 0, SEEK_END);
        if (-1 == sz) {
            CLOSE(log->fd);
            fprintf(stderr, "errmsg:[%d] %s! path:[%s]\n", errno, strerror(errno), log->path);
            break;
        }

        /* 6. 写入指定日志文件 */
        Writen(log->fd, addr, n);

        sz += n;
    } while(0);

    /* 7. 标志复位 */
    memset(addr, 0, n);
    log->inoff = 0;
    log->outoff = 0;
    ftime(&log->sync_tm);

    return sz;
}

/******************************************************************************
 **函数名称: log_rename
 **功    能: 获取备份日志文件名
 **输入参数:
 **     log: 日志对象
 **     time: 当前时间
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.10.31 #
 ******************************************************************************/
static int log_rename(const log_cycle_t *log, const struct timeb *time)
{
    struct tm loctm;
    char newpath[FILE_PATH_MAX_LEN];

    local_time(&time->time, &loctm);

    /* FORMAT: *.logYYYYMMDDHHMMSS.bak */
    snprintf(newpath, sizeof(newpath),
        "%s%04d%02d%02d%02d%02d%02d.bak",
        log->path, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
        loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

    return rename(log->path, newpath);
}

/******************************************************************************
 **函数名称: log_insert
 **功    能: 添加日志对象
 **输入参数:
 **     lsvr: 日志服务
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.03.19 #
 ******************************************************************************/
int log_insert(log_svr_t *lsvr, log_cycle_t *log)
{
    pthread_mutex_lock(&lsvr->lock);
    avl_insert(lsvr->logs, (void *)log);
    pthread_mutex_unlock(&lsvr->lock);
    return 0;
}

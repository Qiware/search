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

static log_svr_t *g_log_svr = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *log_sync_proc(void *_lsvr);
static int _log_sync_proc(log_cycle_t *log, void *args);

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

        lsvr->logs = avl_creat(NULL, (key_cb_t)key_cb_ptr, (cmp_cb_t)cmp_cb_ptr);
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

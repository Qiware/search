/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: flt_sched.c
 ** 版本号: 1.0
 ** 描  述: 任务调度
 **         负责将要处理的文件放入到工作队列中
 ** 作  者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
#include <dirent.h>
#include <libgen.h>

#include "log.h"
#include "comm.h"
#include "redo.h"
#include "filter.h"
#include "flt_conf.h"

/******************************************************************************
 **函数名称: flwtr_sched_routine
 **功    能: 将网页索引文件放入任务队列
 **输入参数:
 **     _ctx: 全局对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
void *flt_sched_routine(void *_ctx)
{
    DIR *dir;
    struct stat st;
    flt_task_t *task;
    struct dirent *item;
    char path[FILE_PATH_MAX_LEN],
         fpath[FILE_PATH_MAX_LEN];
    flt_cntx_t *ctx = (flt_cntx_t *)_ctx;
    flt_conf_t *conf = ctx->conf;

    snprintf(path, sizeof(path), "%s/wpi", conf->download.path);

    while (1) {
        /* > 当队列中无数据时, 才往队列中放数据
         *  原因: 防止文件名被重复放入队列中, 造成异常情况 */
        if (0 != sig_queue_used(ctx->taskq)) {
            Sleep(1);
            continue;
        }

        /* > 打开目录 */
        dir = opendir(path);
        if (NULL == dir) {
            log_error(ctx->log, "errmsg:[%d] %s! path:%s",
                errno, strerror(errno), path);
            Sleep(1);
            continue;
        }

        /* > 遍历文件 */
        while (NULL != (item = readdir(dir))) {
            snprintf(fpath, sizeof(fpath), "%s/%s", path, item->d_name);
            /* > 判断文件类型 */
            stat(fpath, &st);
            if (!S_ISREG(st.st_mode)) {
                continue;
            }

            /* > 放入TASK队列 */
            task = sig_queue_malloc(ctx->taskq, sizeof(flt_task_t));
            if (NULL == task) {
                log_error(ctx->log, "Alloc from queue failed! len:%d/%d",
                    sizeof(flt_crwl_t), sig_queue_size(ctx->crwlq));
                break;
            }

            snprintf(task->fpath, sizeof(task->fpath), "%s", fpath);
            snprintf(task->fname, sizeof(task->fname), "%s", item->d_name);

            sig_queue_push(ctx->taskq, task);
        }

        /* > 关闭目录 */
        closedir(dir);
        Sleep(5);
    }

    return (void *)-1;
}

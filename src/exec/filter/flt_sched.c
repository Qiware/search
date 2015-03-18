/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: flt_sched.c
 ** 版本号: 1.0
 ** 描  述: 任务调度
 **         负责将要处理的文件放入到工作队列中
 ** 作  者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "filter.h"
#include "common.h"
#include "syscall.h"
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
         fname[FILE_PATH_MAX_LEN];
    flt_cntx_t *ctx = (flt_cntx_t *)_ctx;
    flt_conf_t *conf = ctx->conf;

    snprintf(path, sizeof(path), "%s/wpi", conf->download.path);

    while (1)
    {
        Mkdir(path, 0777);

        /* > 当队列中无数据时, 才往队列中放数据
         *  原因: 防止文件名被重复放入队列中, 造成异常情况 */
        if (0 != queue_used(ctx->taskq))
        {
            Sleep(1);
            continue;
        }

        /* > 打开目录 */
        dir = opendir(path);
        if (NULL == dir)
        {
            continue;
        }

        /* > 遍历文件 */
        while (NULL != (item = readdir(dir)))
        {
            snprintf(fname, sizeof(fname), "%s/%s", path, item->d_name); 

            /* 2. 判断文件类型 */
            stat(fname, &st);
            if (!S_ISREG(st.st_mode))
            {
                continue;
            }

            /* 3. 放入TASK队列 */
            task = queue_malloc(ctx->taskq);
            if (NULL == task)
            {
                Sleep(1);
                continue;
            }

            snprintf(task->path, sizeof(task->path), "%s", fname); 

            if (queue_push(ctx->taskq, task))
            {
                queue_dealloc(ctx->taskq, task);
            }
        }

        /* > 关闭目录 */
        closedir(dir);

        Mkdir(conf->filter.store.path, 0777);
        Sleep(5);
    }

    return (void *)-1;
}

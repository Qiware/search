/******************************************************************************
 ** Copyright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mmexec.c
 ** 版本号: 1.0
 ** 描  述: 负责共享内存的创建
 **         因存在2个进程相互依赖对象的共享内存, 这样将导致进程无法正常启动, 通过
 **         第3方进行统一的共享内存创建将有效的解决以上问题的存在!
 ** 作  者: # Qifeng.zou # Thu 04 Jun 2015 04:58:32 PM CST #
 ******************************************************************************/

#include "conf.h"
#include "shm_queue.h"
#include "lsnd_conf.h"

#define MEM_LOG_PATH "../log/mmexec.log"

static int lsnd_mem_creat(sys_conf_t *conf, log_cycle_t *log);

/* 主函数 */
int main(int argc, char *argv[])
{
    sys_conf_t conf;
    log_cycle_t *log;

    memset(&conf, 0, sizeof(conf));

    daemon(1, 1); /* 切后台运行 */

    umask(0);

    /* > 初始化日志模块 */
    log = log_init(LOG_LEVEL_TRACE, MEM_LOG_PATH);
    if (NULL == log)
    {
        fprintf(stderr, "Initialize log cycle failed!\n");
        return -1;
    }

    /* > 加载系统配置 */
    if (conf_load_system(SYS_CONF_DEF_PATH, &conf))
    {
        fprintf(stderr, "Load system configuration failed!\n");
        log_error(log, "Load system configuration failed!");
        return -1;
    }

    /* > 创建侦听服务内存 */
    if (lsnd_mem_creat(&conf, log))
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    while (1) { pause(); }

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_mem_creat
 **功    能: 创建侦听服务内存
 **输入参数:
 **     cf: 系统配置
 **     log: 日志服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 通过配置文件创建内存资源
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-05 10:09:34 #
 ******************************************************************************/
static int lsnd_mem_creat(sys_conf_t *cf, log_cycle_t *log)
{
    int idx;
    conf_map_t map;
    lsnd_conf_t conf;
    shm_queue_t *shmq;
    char path[FILE_PATH_MAX_LEN];

    /* > 加载侦听配置 */
    if (conf_get_listen(cf, "SearchEngineListend", &map))
    {
        log_error(log, "Get SearchEngineListed configuration failed!");
        return -1;
    }

    if (lsnd_load_conf(map.path, &conf, log))
    {
        log_error(log, "Load listen configuration failed!");
        return -1;
    }

    /* > 创建共享内存队列 */
    for (idx=0; idx<conf.distq.num; ++idx)
    {
        LSND_GET_DISTQ_PATH(path, sizeof(path), conf.wdir, idx);

        shmq = shm_queue_creat(path, conf.distq.max, conf.distq.size);
        if (NULL == shmq)
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
    }

    return 0;
}

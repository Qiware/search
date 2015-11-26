/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
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

typedef struct
{
    int log_level;      /* 日志级别 */
    char *log_key_path; /* 日志键值路径 */
} mem_opt_t;

static int mem_getopt(int argc, char **argv, mem_opt_t *opt);
static int mem_usage(const char *exec);
static int lsnd_mem_creat(log_cycle_t *log);

/* 主函数 */
int main(int argc, char *argv[])
{
    mem_opt_t opt;
    log_cycle_t *log;

    /* > 获取输入参数 */
    if (mem_getopt(argc, argv, &opt))
    {
        return mem_usage(argv[0]);
    }

    daemon(1, 1);
    umask(0);

    /* > 初始化日志模块 */
    log = log_init(opt.log_level, MEM_LOG_PATH, opt.log_key_path);
    if (NULL == log)
    {
        fprintf(stderr, "Initialize log cycle failed!\n");
        return -1;
    }

    /* > 加载系统配置 */
    if (conf_load_system(SYS_CONF_DEF_PATH, log))
    {
        fprintf(stderr, "Load system configuration failed!\n");
        log_error(log, "Load system configuration failed!");
        return -1;
    }

    /* > 创建侦听服务内存 */
    if (lsnd_mem_creat(log))
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    while (1) { pause(); }

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 解析输入参数
 **     2. 验证输入参数
 **注意事项: 
 **     N: 服务名 - 根据服务名可找到配置路径
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int mem_getopt(int argc, char **argv, mem_opt_t *opt)
{
    int ch;
    const struct option opts[] = {
        {"log-key",   required_argument,  NULL, 'k'}
        , {"log-level", required_argument,  NULL, 'l'}
        , {"help",      no_argument,        NULL, 'h'}
        , {NULL,        0,                  NULL, 0}
    };

    memset(opt, 0, sizeof(mem_opt_t));

    opt->log_level = LOG_LEVEL_TRACE;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt_long(argc, argv, "l:n:k:hd", opts, NULL)))
    {
        switch (ch)
        {
            case 'l':   /* 日志级别 */
            {
                opt->log_level = log_get_level(optarg);
                break;
            }
            case 'k':   /* 日志键值路径 */
            {
                opt->log_key_path = optarg;
                break;
            }
            case 'h':   /* 显示帮助信息 */
            default:
            {
                return -1;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    return 0;
}

/* 显示帮助信息 */
static int mem_usage(const char *exec)
{
    printf("\nUsage: %s [-h] -l <log level> -k <log key path>\n", exec);
    printf("\t-h: Show help\n"
           "\t-l: Log level\n"
           "\t-k: Log key path\n\n");
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
static int lsnd_mem_creat(log_cycle_t *log)
{
    int idx;
    conf_map_t map;
    lsnd_conf_t conf;
    shm_queue_t *shmq;
    char path[FILE_PATH_MAX_LEN];

    /* > 加载侦听配置 */
    if (conf_get_listen("SearchEngineListend", &map))
    {
        log_error(log, "Get SearchEngineListed configuration failed!");
        return -1;
    }

    if (lsnd_load_conf(map.name, map.path, &conf, log))
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

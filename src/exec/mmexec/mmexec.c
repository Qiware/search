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

#include "shm_queue.h"

#define MEM_LOG_PATH "../log/mmexec.log"

typedef struct
{
    bool isdaemon;      /* 后台运行 */
    int log_level;      /* 日志级别 */
    char *log_key_path; /* 日志键值路径 */
} mem_opt_t;

static int mem_getopt(int argc, char **argv, mem_opt_t *opt);
static int mem_usage(const char *exec);

/* 主函数 */
int main(int argc, char *argv[])
{
    mem_opt_t opt;
    log_cycle_t *log;

    /* > 获取输入参数 */
    if (mem_getopt(argc, argv, &opt)) {
        return mem_usage(argv[0]);
    }
    else if (opt.isdaemon) {
        daemon(1, 1);
    }

    umask(0);

    /* > 初始化日志模块 */
    log = log_init(opt.log_level, MEM_LOG_PATH, opt.log_key_path);
    if (NULL == log) {
        fprintf(stderr, "Initialize log cycle failed!\n");
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
 **     d: 后台运行
 **     l: 日志级别
 **     k: 日志键值路径
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int mem_getopt(int argc, char **argv, mem_opt_t *opt)
{
    int ch;
    const struct option opts[] = {
        {"help",            no_argument,        NULL, 'h'}
        , {"isdaemon",      required_argument,  NULL, 'd'}
        , {"log level",     required_argument,  NULL, 'l'}
        , {"log key path",  required_argument,  NULL, 'L'}
        , {NULL,            0,                  NULL, 0}
    };

    memset(opt, 0, sizeof(mem_opt_t));

    opt->isdaemon = false;
    opt->log_level = LOG_LEVEL_TRACE;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt_long(argc, argv, "l:n:L:hd", opts, NULL)))
    {
        switch (ch)
        {
            case 'd':   /* 后台运行 */
            {
                opt->isdaemon = true;
                break;
            }
            case 'l':   /* 日志级别 */
            {
                opt->log_level = log_get_level(optarg);
                break;
            }
            case 'L':   /* 日志键值路径 */
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

    if (NULL == opt->log_key_path) {
        return -1;
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

#include "cmd.h"
#include "listend.h"

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
int lsnd_getopt(int argc, char **argv, lsnd_opt_t *opt)
{
    int ch;
    const struct option opts[] = {
        {"help",                    no_argument,        NULL, 'h'}
        , {"daemon",                no_argument,        NULL, 'd'}
        , {"log-level",             required_argument,  NULL, 'l'}
        , {"configuration path",    required_argument,  NULL, 'c'}
        , {NULL,                    0,                  NULL, 0}
    };

    memset(opt, 0, sizeof(lsnd_opt_t));

    opt->isdaemon = false;
    opt->log_level = LOG_LEVEL_TRACE;
    opt->conf_path = "../conf/listend.xml";

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt_long(argc, argv, "l:c:hd", opts, NULL))) {
        switch (ch) {
            case 'c':   /* 配置路径 */
            {
                opt->conf_path = optarg;
                break;
            }
            case 'l':   /* 日志级别 */
            {
                opt->log_level = log_get_level(optarg);
                break;
            }
            case 'd':
            {
                opt->isdaemon = true;
                break;
            }
            case 'h':   /* 显示帮助信息 */
            default:
            {
                return LSND_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (NULL == opt->conf_path) {
        return LSND_SHOW_HELP;
    }

    return 0;
}

/* 显示启动参数帮助信息 */
int lsnd_usage(const char *exec)
{
    printf("\nUsage: %s -l <log level> -L <log key path> -n <node name> [-h] [-d]\n", exec);
    printf("\t-l: Log level\n"
            "\t-n: Node name\n"
            "\t-d: Run as daemon\n"
            "\t-h: Show help\n\n");
    return 0;
}

/* 初始化日志模块 */
log_cycle_t *lsnd_init_log(char *fname)
{
    char path[FILE_NAME_MAX_LEN];

    log_get_path(path, sizeof(path), basename(fname));

    return log_init(LOG_LEVEL_ERROR, path);
}

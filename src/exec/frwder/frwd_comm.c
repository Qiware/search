/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: frwd_comm.c
 ** 版本号: 1.0
 ** 描  述: 通用函数定义
 ** 作  者: # Qifeng.zou # Wed 10 Jun 2015 12:14:26 PM CST #
 ******************************************************************************/

#include "comm.h"
#include "frwd.h"
#include "mesg.h"
#include "agent.h"
#include "command.h"

/******************************************************************************
 **函数名称: frwd_getopt
 **功    能: 解析输入参数
 **输入参数:
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 解析和验证输入参数
 **注意事项:
 **     n: 转发服务名 - 根据服务名, 便可找到对应的配置文件
 **     h: 帮助手册
 **     l: 日志级别
 **     k: 日志键值路径
 **     d: 以精灵进程运行
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_getopt(int argc, char **argv, frwd_opt_t *opt)
{
    int ch;
    const struct option opts[] = {
        {"help",                    no_argument,        NULL, 'h'}
        , {"daemon",                no_argument,        NULL, 'd'}
        , {"log-level",             required_argument,  NULL, 'l'}
        , {"log key path",          required_argument,  NULL, 'L'}
        , {"configuartion path",    required_argument,  NULL, 'c'}
        , {NULL,                    0,                  NULL, 0}
    };

    memset(opt, 0, sizeof(frwd_opt_t));

    opt->isdaemon = false;
    opt->log_level = LOG_LEVEL_TRACE;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt_long(argc, argv, "n:l:L:hd", opts, NULL)))
    {
        switch (ch)
        {
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
            case 'L':   /* 日志键值路径 */
            {
                opt->log_key_path = optarg;
                break;
            }
            case 'd':   /* 是否后台运行 */
            {
                opt->isdaemon = true;
                break;
            }
            case 'h':   /* 显示帮助信息 */
            default:
            {
                return FRWD_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (NULL == opt->conf_path) {
        return FRWD_SHOW_HELP;
    }

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     exec: 程序名
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -n <name>\n", exec);
    printf("\t-h\tShow help\n"
           "\t-n\tNode name\n\n");
    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_init
 **功    能: 初始化转发服务
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
frwd_cntx_t *frwd_init(const frwd_conf_t *conf, log_cycle_t *log)
{
    frwd_cntx_t *frwd;
    char path[FILE_PATH_MAX_LEN];

    frwd = (frwd_cntx_t *)calloc(1, sizeof(frwd_cntx_t));
    if (NULL == frwd) {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    frwd->log = log;
    memcpy(&frwd->conf, conf, sizeof(frwd_conf_t));

    do {
        /* > 创建命令套接字 */
        snprintf(path, sizeof(path), "../temp/frwder/cmd.usck");

        frwd->cmd_sck_id = unix_udp_creat(path);
        if (frwd->cmd_sck_id < 0) {
            fprintf(stderr, "Create unix udp failed! path:%s\n", path);
            break;
        }

        /* > 初始化发送服务 */
        frwd->rtmq = rtsd_init(&conf->conn_invtd, frwd->log);
        if (NULL == frwd->rtmq) {
            log_fatal(frwd->log, "Initialize send-server failed!");
            break;
        }

        return frwd;
    } while (0);

    free(frwd);
    return NULL;
}

/******************************************************************************
 **函数名称: frwd_launch
 **功    能: 初始化转发服务
 **输入参数:
 **     frwd: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_launch(frwd_cntx_t *frwd)
{
    if (rtsd_launch(frwd->rtmq)) {
        log_fatal(frwd->log, "Start up send-server failed!");
        return FRWD_ERR;
    }

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_init_log
 **功    能: 初始化日志模块
 **输入参数:
 **     pname: 进程名
 **     log_level: 日志级别
 **输出参数: NONE
 **返    回: 日志对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015-06-10 #
 ******************************************************************************/
log_cycle_t *frwd_init_log(const char *pname, int log_level, const char *log_key_path)
{
    char path[FILE_PATH_MAX_LEN];

    snprintf(path, sizeof(path), "../log/%s.log", pname);

    return log_init(log_level, path, log_key_path);
}

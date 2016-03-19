/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: frwder.c
 ** 版本号: 1.0
 ** 描  述: 转发器
 **         转发器负责数据的转发
 ** 作  者: # Qifeng.zou # 2015.06.08 #
 ******************************************************************************/
#include "comm.h"
#include "mesg.h"
#include "frwd.h"
#include "agent.h"
#include "frwd_conf.h"
#include "agent_mesg.h"

/* 主函数 */
int main(int argc, char *argv[])
{
    frwd_opt_t opt;
    frwd_conf_t conf;
    log_cntx_t *lsvr;
    log_cycle_t *log;
    frwd_cntx_t *frwd;

    memset(&conf, 0, sizeof(conf));

    /* > 获取输入参数 */
    if (frwd_getopt(argc, argv, &opt)) {
        return frwd_usage(basename(argv[0]));
    }
    else if (opt.isdaemon) {
        daemon(1, 1);   /* 后台运行 */
    }

    umask(0);

    /* > 初始化日志 */
    lsvr = log_init();
    if (NULL == lsvr) {
        fprintf(stderr, "Initialize log server failed!\n");
        return -1;
    }
    log = frwd_init_log(lsvr, argv[0], opt.log_level);
    if (NULL == log) {
        fprintf(stderr, "Initialize log failed!\n");
        return -1;
    }

    /* > 加载配置信息 */
    if (frwd_load_conf(opt.conf_path, &conf, log)) {
        fprintf(stderr, "Load configuration failed!\n");
        return FRWD_ERR;
    }

    /* > 初始化服务 */
    frwd = frwd_init(&conf, log);
    if (NULL == frwd) {
        fprintf(stderr, "Initialize frwder failed!\n");
        return FRWD_ERR;
    }

    /* > 注册处理回调 */
    if (frwd_set_reg(frwd)) {
        log_fatal(frwd->log, "Register callback failed!");
        return FRWD_ERR;
    }

    /* > 启动转发服务 */
    if (frwd_launch(frwd)) {
        log_fatal(frwd->log, "Startup frwder failed!");
        return FRWD_ERR;
    }

    while (1) { pause(); }

    log_fatal(frwd->log, "Exit frwder server!");

    return 0;
}

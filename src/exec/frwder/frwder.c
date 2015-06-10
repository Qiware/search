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
    frwd_cntx_t *frwd;

    memset(&frwd, 0, sizeof(frwd));

    /* > 获取输入参数 */
    if (frwd_getopt(argc, argv, &opt))
    {
        return frwd_usage(basename(argv[0]));
    }
    else if (opt.isdaemon)
    {
        daemon(1, 1);   /* 后台运行 */
    }

    /* 加载配置信息 */
    if (frwd_load_conf(opt.conf_path, &conf))
    {
        fprintf(stderr, "Load configuration failed!\n");
        return FRWD_ERR;
    }

    /* 初始化服务 */
    frwd = frwd_init(&conf);
    if (NULL == frwd)
    {
        fprintf(stderr, "Initialize frwder failed!\n");
        return FRWD_ERR;
    }

    /* > 注册处理回调 */
    if (frwd_set_reg(frwd))
    {
        log_fatal(frwd->log, "Register callback failed!");
        return FRWD_ERR;
    }

    /* > 启动转发服务 */
    if (frwd_startup(frwd))
    {
        log_fatal(frwd->log, "Startup frwder failed!");
        return FRWD_ERR;
    }

    while (1) { pause(); }

    log_fatal(frwd->log, "Exit frwder server!");

    return 0;
}

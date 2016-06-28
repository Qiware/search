/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invertd.c
 ** 版本号: 1.0
 ** 描  述: 倒排服务程序
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 10:22:56 PM CST #
 ******************************************************************************/

#include "redo.h"
#include "invertd.h"
#include "invtd_priv.h"
#include "invtd_conf.h"

#define INVTD_LOG_PATH      "../log/invertd.log"

/******************************************************************************
 **函数名称: main 
 **功    能: 倒排服务主程序
 **输入参数:
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    invtd_opt_t opt;
    log_cycle_t *log;
    invtd_cntx_t *ctx;
    invtd_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    /* > 获取参数 */
    if (invtd_getopt(argc, argv, &opt)) {
        return invtd_usage(basename(argv[0])); /* 显示帮助 */
    }
    else if (opt.isdaemon) {
        /* int daemon(int nochdir, int noclose);
         *  1. daemon()函数主要用于希望脱离控制台, 以守护进程形式在后台运行的程序.
         *  2. 当nochdir为0时, daemon将更改进城的根目录为root(“/”).
         *  3. 当noclose为0时, daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    umask(0);

    /* > 初始化日志 */
    log = log_init(opt.log_level, INVTD_LOG_PATH);
    if (NULL == log) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return INVT_ERR;
    }

    do {
        /* > 加载配置信息 */
        if (invtd_conf_load(opt.conf_path, &conf, log)) {
            log_fatal(log, "Load configuration failed! path:%s", opt.conf_path);
            break;
        }

        /* > 服务初始化 */
        ctx = invtd_init(&conf, log);
        if (NULL == ctx) {
            log_fatal(log, "Init invertd failed!");
            break;
        }

        /* > 启动服务 */
        if (invtd_launch(ctx)) {
            log_fatal(ctx->log, "Startup invertd failed!");
            break;
        }

        while (1) { pause(); }
    } while(0);

    Sleep(2);

    return 0;
}

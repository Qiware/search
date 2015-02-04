/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "lock.h"
#include "hash.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "sck_api.h"
#include "crwl_priv.h"
#include "crwl_sched.h"
#include "crwl_worker.h"

/******************************************************************************
 **函数名称: main 
 **功    能: 爬虫主程序
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载爬虫配置
 **     1. 初始化爬虫信息
 **     2. 启动爬虫功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int ret;
    crwl_opt_t opt;
    crwl_cntx_t *ctx;

    memset(&opt, 0, sizeof(opt));

    /* 1. 解析输入参数 */
    ret = crwl_getopt(argc, argv, &opt);
    if (CRWL_OK != ret)
    {
        return crwl_usage(argv[0]);
    }

    if (opt.isdaemon)
    {
        /* int daemon(int nochdir, int noclose);
         *  1. daemon()函数主要用于希望脱离控制台, 以守护进程形式在后台运行的程序.
         *  2. 当nochdir为0时, daemon将更改进城的根目录为root(“/”).
         *  3. 当noclose为0是, daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    crwl_set_signal();

    /* 2. 初始化全局信息 */
    ctx = crwl_cntx_init(argv[0], opt.conf_path);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize crawler failed!");
        return CRWL_ERR;
    }

    /* 3. 启动爬虫服务 */
    ret = crwl_startup(ctx);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Startup crawler failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    crwl_cntx_destroy(ctx);

    return CRWL_ERR;
}

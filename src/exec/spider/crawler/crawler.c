/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: crawler.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#include "sck.h"
#include "comm.h"
#include "lock.h"
#include "crawler.h"
#include "syscall.h"
#include "mem_ref.h"
#include "crwl_man.h"
#include "hash_alg.h"
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
 **     1. 接收输入参数
 **     2. 初始化模块
 **     3. 启动爬虫功能
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    crwl_opt_t opt;
    crwl_cntx_t *ctx;

    /* 1. 解析输入参数 */
    if (crwl_getopt(argc, argv, &opt)) {
        return crwl_usage(argv[0]);
    }
    else if (opt.isdaemon) {
        /* int daemon(int nochdir, int noclose);
         *  1. daemon()函数主要用于希望脱离控制台, 以守护进程形式在后台运行的程序.
         *  2. 当nochdir为0时, daemon将更改进城的根目录为root(“/”).
         *  3. 当noclose为0时, daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    umask(0);
    mem_ref_init();
    crwl_set_signal();

    /* 2. 初始化处理 */
    ctx = crwl_init(argv[0], &opt);
    if (NULL == ctx) {
        fprintf(stderr, "Initialize crawler failed!");
        return CRWL_ERR;
    }

    /* 3. 启动爬虫服务 */
    if (crwl_launch(ctx)) {
        log_error(ctx->log, "Startup crawler failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    crwl_destroy(ctx);

    return CRWL_ERR;
}

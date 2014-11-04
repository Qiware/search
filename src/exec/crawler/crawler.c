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
#include "xd_socket.h"
#include "crwl_sched.h"
#include "crwl_worker.h"
#include "crwl_parser.h"

#define CRWL_PROC_LOCK_PATH "../temp/crwl/crwl.lck"

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
    log_cycle_t *log;

    memset(&opt, 0, sizeof(opt));

    /* 1. 解析输入参数 */
    ret = crwl_getopt(argc, argv, &opt);
    if (CRWL_OK != ret)
    {
        return crwl_usage(argv[0]);
    }

    /* 2. 初始化日志模块 */
    log = crwl_init_log(argv[0]);
    if (NULL == log)
    {
        return CRWL_ERR;
    }

    daemon(1, 0);

    /* 3. 初始化全局信息 */
    ctx = crwl_cntx_init(opt.conf_path, log);
    if (NULL == ctx)
    {
        log_error(log, "Initialize crawler failed!");
        goto ERROR;
    }

    /* 4. 启动爬虫服务 */
    ret = crwl_cntx_startup(ctx);
    if (CRWL_OK != ret)
    {
        log_error(log, "Startup crawler failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    log2_destroy();

    return CRWL_ERR;
}

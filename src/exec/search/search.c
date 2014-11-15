/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: search.c
 ** 版本号: 1.0
 ** 描  述: 搜索引擎
 **         负责接受搜索请求，并将搜索结果返回给客户端
 ** 作  者: # Qifeng.zou # 2014.11.15 #
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
#include "search.h"
#include "syscall.h"
#include "xd_socket.h"
#include "srch_worker.h"

#define SRCH_PROC_LOCK_PATH "../temp/crwl/crwl.lck"

/******************************************************************************
 **函数名称: main 
 **功    能: 搜索引擎主程序
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载搜索引擎配置
 **     1. 初始化搜索引擎信息
 **     2. 启动搜索引擎功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int ret;
    srch_opt_t opt;
    srch_cntx_t *ctx;
    log_cycle_t *log;

    memset(&opt, 0, sizeof(opt));

    /* 1. 解析输入参数 */
    ret = srch_getopt(argc, argv, &opt);
    if (SRCH_OK != ret)
    {
        return srch_usage(argv[0]);
    }

    if (opt.isdaemon)
    {
        daemon(1, 0);
    }
 
    /* 2. 初始化日志模块 */
    log = srch_init_log(argv[0]);
    if (NULL == log)
    {
        fprintf(stderr, "Initialize log failed!");
        return SRCH_ERR;
    }

    /* 3. 初始化全局信息 */
    ctx = srch_init(opt.conf_path, log);
    if (NULL == ctx)
    {
        log_error(log, "Initialize crawler failed!");
        goto ERROR;
    }

    /* 4. 启动爬虫服务 */
    ret = srch_startup(ctx);
    if (SRCH_OK != ret)
    {
        log_error(log, "Startup crawler failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    log2_destroy();

    return SRCH_ERR;
}

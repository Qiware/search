/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: filter.c
 ** 版本号: 1.0
 ** 描  述: 超链接的提取程序
 **         从爬取的网页中提取超链接
 ** 作  者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "str.h"
#include "list.h"
#include "hash.h"
#include "http.h"
#include "redis.h"
#include "filter.h"
#include "common.h"
#include "syscall.h"
#include "sck_api.h"
#include "xml_tree.h"

/* 网页分析过滤模块主函数 */
int main(int argc, char *argv[])
{
    flt_opt_t opt;
    flt_cntx_t *ctx;

    memset(&opt, 0, sizeof(opt));

    /* > 解析输入参数 */
    if (flt_getopt(argc, argv, &opt))
    {
        return flt_usage(argv[0]);
    }

    if (opt.isdaemon)
    {
        /* int daemon(int nochdir, int noclose);
         *  1. daemon()函数主要用于希望脱离控制台, 以守护进程形式在后台运行的程序.
         *  2. 当nochdir为0时, daemon将更改进城的根目录为root(“/”).
         *  3. 当noclose为0时, daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    /* > 初始化过滤模块 */
    ctx = flt_init(argv[0], opt.conf_path);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize filter failed!");
        return FLT_ERR;
    }

    /* > 启动过滤模块 */
    if (flt_startup(ctx))
    {
        fprintf(stderr, "Startup filterfailed!");
        return FLT_ERR;
    }

    while (1) { pause(); }

    /* > 释放GUMBO对象 */
    flt_destroy(ctx);

    return FLT_OK;
}

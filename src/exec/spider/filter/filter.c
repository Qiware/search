/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: filter.c
 ** 版本号: 1.0
 ** 描  述: 超链接的提取程序
 **         从爬取的网页中提取超链接
 ** 作  者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/

#include "log.h"
#include "str.h"
#include "comm.h"
#include "filter.h"
#include "syscall.h"
#include "mem_ref.h"

/******************************************************************************
 **函数名称: main
 **功    能: 网页分析过滤模块主函数
 **输入参数:
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 接收输入参数
 **     2. 初始化模块
 **     3. 启动过滤功能
 **注意事项:
 **作    者: # Qifeng.zou # 2014.03.11 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    flt_opt_t opt;
    flt_cntx_t *ctx;

     /* > 解析输入参数 */
    if (flt_getopt(argc, argv, &opt)) {
        return flt_usage(argv[0]);
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

    /* > 初始化过滤模块 */
    ctx = flt_init(argv[0], &opt);
    if (NULL == ctx) {
        fprintf(stderr, "Initialize filter failed!\n");
        return FLT_ERR;
    }

    /* > 启动过滤模块 */
    if (flt_launch(ctx)) {
        fprintf(stderr, "Startup filterfailed!\n");
        return FLT_ERR;
    }

    while (1) { pause(); }

    /* > 释放GUMBO对象 */
    flt_destroy(ctx);

    return FLT_OK;
}

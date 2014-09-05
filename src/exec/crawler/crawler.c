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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "common.h"
#include "crawler.h"
#include "crwl_comm.h"

/* 程序输入参数信息 */
typedef struct
{
    bool is_daemon;                     /* 是否后台运行 */
    char conf_path[FILE_NAME_MAX_LEN];  /* 配置文件路径 */
}crawler_opt_t;

static int crawler_getopt(int argc, char **argv, crawler_opt_t *opt);
static int crawler_usage(const char *exec);

/******************************************************************************
 **函数名称: main 
 **功    能: 爬虫服务主程序
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载爬虫配置
 **     2. 启动爬虫服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int ret;
    crawler_opt_t opt;
    crawler_conf_t conf;

    memset(&opt, 0, sizeof(opt));
    memset(&conf, 0, sizeof(conf));

    /* 1. 解析输入参数 */
    ret = crawler_getopt(argc, argv, &opt);
    if (CRWL_OK != ret)
    {
        return crawler_usage(argv[0]);
    }



    /* 2. 加载配置文件 */
    ret = crwl_load_conf(&conf, opt.conf_path);
    if (CRWL_OK != ret)
    {
        fprintf(stderr, "Load crawler configuration failed!");
        return CRWL_FAIL;
    }

    /* 3. 启动爬虫服务 */
    ret = crwl_start_work(&conf);
    if (CRWL_OK != ret)
    {
        fprintf(stderr, "Start crawler server failed!");
        return CRWL_FAIL;
    }

    while (1) { pause(); }

    return 0;
}

/******************************************************************************
 **函数名称: crawler_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 解析输入参数
 **     2. 验证输入参数
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static int crawler_getopt(int argc, char **argv, crawler_opt_t *opt)
{
    int _opt;

    /* 1. 解析输入参数 */
    while (-1 != (_opt = getopt(argc, argv, "c:bh")))
    {
        switch (_opt)
        {
            case 'b':
            {
                opt->is_daemon = true;
                break;
            }
            case 'c':
            {
                snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", optarg);
                break;
            }
            case 'h':
            default:
            {
                return CRWL_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        return CRWL_SHOW_HELP;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crawler_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     name: 程序名
 **输出参数: NULL
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.11 #
 ******************************************************************************/
static int crawler_usage(const char *exec)
{
    printf("\nusage: %s [-h] [-b] -c config_file\n", exec);
    printf("\t-h\tShow help\n"
           "\t-b\tRun as daemon\n"
           "\t-c\tConfiguration path\n\n");

    return CRWL_OK;
}

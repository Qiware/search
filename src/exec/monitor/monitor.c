/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: monitor.c
 ** 版本号: 1.0
 ** 描  述: 监控模块
 **         获取平台运行的详细数据信息.
 ** 注  意: 请勿显示中文，否则将会出现对齐异常!
 ** 作  者: # Qifeng.zou # 2014.12.26 #
 ******************************************************************************/
#include "common.h"
#include "syscall.h"
#include "monitor.h"
#include "mon_conf.h"

static mon_cntx_t *mon_cntx_init(const char *path);
static menu_cntx_t *mon_menu_init(mon_cntx_t *ctx);

/******************************************************************************
 **函数名称: mon_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 解析输入参数
 **     2. 验证输入参数
 **注意事项: 
 **     c: 配置文件路径
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2015.03.16 #
 ******************************************************************************/
static int mon_getopt(int argc, char **argv, mon_opt_t *opt)
{
    int ch;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt(argc, argv, "c:hd")))
    {
        switch (ch)
        {
            case 'c':   /* 指定配置文件 */
            {
                snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", optarg);
                break;
            }
            case 'h':   /* 显示帮助信息 */
            default:
            {
                return -1;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", MON_DEF_CONF_PATH);
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     name: 程序名
 **输出参数: NULL
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.16 #
 ******************************************************************************/
int mon_usage(const char *exec)
{
    printf("\nUsage: %s [-h] -c <config file> [-l log_level]\n", exec);
    printf("\t-h\tShow help\n"
           "\t-c\tConfiguration path\n\n");
    return 0;
}

/******************************************************************************
 **函数名称: main 
 **功    能: 监控主程序
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    mon_opt_t opt;
    mon_cntx_t *ctx;

    memset(&opt, 0, sizeof(opt));

    set_fd_limit(65535);

    /* > 解析输入参数 */
    if (mon_getopt(argc, argv, &opt))
    {
        return mon_usage(argv[0]);
    }

    /* > 初始化全局信息 */
    ctx = mon_cntx_init(opt.conf_path);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize monitor failed!\n");
        return -1;
    }

    /* > 启动菜单模块 */
    if (menu_startup(ctx->menu))
    {
        fprintf(stderr, "Startup menu failed!\n");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_cntx_init
 **功    能: 初始化全局对象
 **输入参数:
 **     path: 配置路径
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.16 #
 ******************************************************************************/
static mon_cntx_t *mon_cntx_init(const char *path)
{
    mon_cntx_t *ctx;
    char log_path[FILE_NAME_MAX_LEN];

    syslog_get_path(log_path, sizeof(log_path), "monitor");

    if (syslog_init(LOG_LEVEL_DEBUG, log_path))
    {
        fprintf(stderr, "Init syslog failed!");
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (mon_cntx_t *)calloc(1, sizeof(mon_cntx_t));
    if (NULL == ctx)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 加载配置信息 */
    ctx->conf = mon_conf_load(path);
    if (NULL == ctx->conf)
    {
        fprintf(stderr, "Load configuration failed!\n");
        free(ctx);
        return NULL;
    }

    /* > 加载子菜单 */
    ctx->menu = mon_menu_init(ctx);
    if (NULL == ctx->menu)
    {
        fprintf(stderr, "Load menu failed!\n");
        return NULL;
    }

    /* > 创建通信套接字 */
    ctx->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->fd < 0)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        free(ctx);
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: mon_menu_init
 **功    能: 加载子菜单
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static menu_cntx_t *mon_menu_init(mon_cntx_t *ctx)
{
    menu_item_t *child;
    menu_cntx_t *menu_ctx;

    /* > 初始化菜单 */
    menu_ctx = menu_cntx_init("Monitor System");
    if (NULL == menu_ctx)
    {
        fprintf(stderr, "Init menu context failed!\n");
        return NULL;
    }

    /* > 加载搜索引擎菜单 */
    child = mon_srch_menu(menu_ctx, ctx);

    menu_add(menu_ctx->menu, child);

    /* > 加载爬虫系统菜单 */
    child = mon_crwl_menu(menu_ctx, ctx);

    menu_add(menu_ctx->menu, child);

    /* > 加载爬虫系统菜单 */
    child = mon_flt_menu(menu_ctx, ctx);

    menu_add(menu_ctx->menu, child);

    return menu_ctx;
}

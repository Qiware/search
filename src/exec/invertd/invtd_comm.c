/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invtd_comm.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 07:14:11 PM CST #
 ******************************************************************************/

#include "invtab.h"
#include "invertd.h"
#include "invtd_priv.h"

#define INVTD_LOG_PATH      "../log/invertd.log"
#define INVTD_PLOG_PATH     "../log/invertd.plog"

/******************************************************************************
 **函数名称: invtd_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 解析和验证输入参数
 **注意事项: 
 **     c: 配置文件路径
 **     h: 帮助手册
 **     d: 以精灵进程运行
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_getopt(int argc, char **argv, invtd_opt_t *opt)
{
    int ch;

    memset(opt, 0, sizeof(invtd_opt_t));

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
            case 'd':
            {
                opt->isdaemon = true;
                break;
            }
            case 'h':   /* 显示帮助信息 */
            default:
            {
                return INVT_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", INVTD_DEF_CONF_PATH);
    }

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     exec: 程序名
 **输出参数: NULL
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -c <config file> [-l log_level]\n", exec);
    printf("\t-h\tShow help\n"
           "\t-c\tConfiguration path\n\n");
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_init 
 **功    能: 初始化倒排服务
 **输入参数:
 **     conf_path: 配置路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次创建所需要的资源(日志 SDTP服务 倒排表等)
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
invtd_cntx_t *invtd_init(const char *conf_path)
{
    log_cycle_t *log;
    invtd_cntx_t *ctx;

    /* > 初始化日志 */
    log = log_init(LOG_LEVEL_TRACE, INVTD_LOG_PATH);
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 创建倒排对象 */
    ctx = (invtd_cntx_t *)calloc(1, sizeof(invtd_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    do
    {
        /* > 加载配置信息 */
        if (invtd_conf_load(conf_path, &ctx->conf))
        {
            log_error(log, "Load configuration failed! path:%s", conf_path);
            break;
        }

        /* > 创建倒排表 */
        ctx->tab = invtab_creat(ctx->conf.invt_tab_max, log);
        if (NULL == ctx->tab)
        {
            log_error(log, "Create invert table failed!");
            break;
        }

        /* > 初始化SDTP服务 */
        ctx->rtrd = rtrd_init(&ctx->conf.rtrd, log);
        if (NULL == ctx->rtrd)
        {
            log_error(log, "Init sdtp failed!");
            break;
        }

        ctx->rtrd_cli = rtrd_cli_init(&ctx->conf.rtrd);
        if (NULL == ctx->rtrd_cli)
        {
            log_error(log, "Init sdtp-rcli failed!");
            break;
        }

        return ctx;
    } while(0);

    free(ctx);
    return NULL;
}

#if defined(__INVTD_DEBUG__)
/******************************************************************************
 **函数名称: invtd_insert_word
 **功    能: 插入倒排关键字
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.11 #
 ******************************************************************************/
static int invtd_insert_word(invtd_cntx_t *ctx)
{
#define INVERT_INSERT(ctx, word, url, freq) \
    if (invtab_insert(ctx->tab, word, url, freq)) \
    { \
        return INVT_ERR; \
    }

    INVERT_INSERT(ctx, "CSDN", "www.csdn.net", 5);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu.com", 5);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu2.com", 4);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu3.com", 2);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu4.com", 3);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu5.com", 10);
    INVERT_INSERT(ctx, "凤凰网", "www.ifeng.com", 10);
    INVERT_INSERT(ctx, "爱我中华", "www.zhonghua.com", 10);
    INVERT_INSERT(ctx, "QQ", "www.qq.com", 10);
    INVERT_INSERT(ctx, "SINA", "www.sina.com", 6);
    INVERT_INSERT(ctx, "搜狐", "www.sohu.com", 7);

    return INVT_OK;
}
#endif /*__INVTD_DEBUG__*/

/******************************************************************************
 **函数名称: invtd_startup
 **功    能: 启动服务
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_startup(invtd_cntx_t *ctx)
{
    /* 启动SDTP */
    if (invtd_start_rttp(ctx))
    {
        log_fatal(ctx->log, "Startup sdtp failed!");
        return INVT_ERR;
    }

#if defined(__INVTD_DEBUG__)
    /* 插入关键字 */
    if (invtd_insert_word(ctx))
    {
        log_fatal(ctx->log, "Insert key-word failed!");
        return INVT_ERR;
    }
#endif /*__INVTD_DEBUG__*/

    return INVT_OK;
}

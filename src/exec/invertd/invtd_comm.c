#include "invtab.h"
#include "invertd.h"
#include "invtd_priv.h"

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
    const struct option opts[] = {
        {"conf",            required_argument,  NULL, 'c'}
        , {"help",          no_argument,        NULL, 'h'}
        , {"daemon",        no_argument,        NULL, 'd'}
        , {"log level",     required_argument,  NULL, 'l'}
        , {"log key path",  required_argument,  NULL, 'L'}
        , {NULL,            0,                  NULL, 0}
    };

    memset(opt, 0, sizeof(invtd_opt_t));

    opt->isdaemon = false;
    opt->log_level = LOG_LEVEL_TRACE;
    opt->conf_path = INVTD_DEF_CONF_PATH;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt_long(argc, argv, "c:l:L:hd", opts, NULL))) {
        switch (ch) {
            case 'c':   /* 指定配置文件 */
            {
                opt->conf_path = optarg;
                break;
            }
            case 'l':   /* 日志级别 */
            {
                opt->log_level = log_get_level(optarg);
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
    printf("\nUsage: %s -l <log level> -L <log key path> -c <conf path> [-h] [-d]\n", exec);
    printf("\t-l: Log level\n"
            "\t-L: Log key path\n"
            "\t-c: Configuration path\n"
            "\t-d: Run as daemon\n"
            "\t-h: Show help\n\n");
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_init 
 **功    能: 初始化倒排服务
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次创建所需要的资源(日志 SDTP服务 倒排表等)
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
invtd_cntx_t *invtd_init(const invtd_conf_t *conf, log_cycle_t *log)
{
    invtd_cntx_t *ctx;

    /* > 创建倒排对象 */
    ctx = (invtd_cntx_t *)calloc(1, sizeof(invtd_cntx_t));
    if (NULL == ctx) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    memcpy(&ctx->conf, conf, sizeof(ctx->conf));

    do {
        /* > 创建倒排表 */
        ctx->invtab = invtab_creat(ctx->conf.invt_tab_max, log);
        if (NULL == ctx->invtab) {
            log_error(log, "Create invert table failed!");
            break;
        }

        pthread_rwlock_init(&ctx->invtab_lock, NULL);

        /* > 初始化SDTP服务 */
        ctx->rtrd = rtrd_init(&ctx->conf.rtrd, log);
        if (NULL == ctx->rtrd) {
            log_error(log, "Init sdtp failed!");
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
    pthread_rwlock_wrlock(&ctx->invtab_lock); \
    if (invtab_insert(ctx->invtab, word, url, freq)) { \
        pthread_rwlock_unlock(&ctx->invtab_lock); \
        return INVT_ERR; \
    } \
    pthread_rwlock_unlock(&ctx->invtab_lock); \


    INVERT_INSERT(ctx, "CSDN", "www.csdn.net", 5);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu.com", 5);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu2.com", 4);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu3.com", 2);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu3.com", 1);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu4.com", 3);
    INVERT_INSERT(ctx, "BAIDU", "www.baidu5.com", 10);
    INVERT_INSERT(ctx, "QQ", "www.qq.com", 10);
    INVERT_INSERT(ctx, "SINA", "www.sina.com", 6);
    INVERT_INSERT(ctx, "ifeng", "www.ifeng.com", 10);
    INVERT_INSERT(ctx, "zhonghua", "www.zhonghua.com", 10);
    INVERT_INSERT(ctx, "sohu", "www.sohu.com", 7);

    return INVT_OK;
}
#endif /*__INVTD_DEBUG__*/

/******************************************************************************
 **函数名称: invtd_launch
 **功    能: 启动服务
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_launch(invtd_cntx_t *ctx)
{
    /* 启动RTMQ */
    if (invtd_start_rtmq(ctx)) {
        log_fatal(ctx->log, "Startup sdtp failed!");
        return INVT_ERR;
    }

#if defined(__INVTD_DEBUG__)
    /* 插入关键字 */
    if (invtd_insert_word(ctx)) {
        log_fatal(ctx->log, "Insert key-word failed!");
        return INVT_ERR;
    }
#endif /*__INVTD_DEBUG__*/

    return INVT_OK;
}

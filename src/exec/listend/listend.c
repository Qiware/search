/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: listend.c
 ** 版本号: 1.0
 ** 描  述: 代理服务
 **         负责接受外界请求，并将处理结果返回给外界
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/

#include "sck.h"
#include "lock.h"
#include "mesg.h"
#include "redo.h"
#include "agent.h"
#include "listend.h"
#include "hash_alg.h"
#include "lsnd_mesg.h"

static lsnd_cntx_t *lsnd_init(lsnd_conf_t *conf, log_cycle_t *log);
static int lsnd_launch(lsnd_cntx_t *ctx);
static int lsnd_set_reg(lsnd_cntx_t *ctx);

/******************************************************************************
 **函数名称: main 
 **功    能: 代理服务
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 加载配置，再通过配置启动各模块
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    lsnd_opt_t opt;
    lsnd_conf_t conf;
    log_cntx_t *lsvr;
    log_cycle_t *log;
    lsnd_cntx_t *ctx = NULL;
    char path[FILE_PATH_MAX_LEN];

    /* > 解析输入参数 */
    if (lsnd_getopt(argc, argv, &opt)) {
        return lsnd_usage(argv[0]);
    }
    else if (opt.isdaemon) {
        /* int daemon(int nochdir, int noclose);
         *  1． daemon()函数主要用于希望脱离控制台,以守护进程形式在后台运行的程序.
         *  2． 当nochdir为0时,daemon将更改进城的根目录为root(“/”).
         *  3． 当noclose为0是,daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    umask(0);

    /* > 初始化日志 */
    log_get_path(path, sizeof(path), basename(argv[0]));

    lsvr = log_init();
    if (NULL == lsvr) {
        fprintf(stderr, "Initialize log server failed!\n");
        goto LSND_INIT_ERR;
    }

    log = log_creat(lsvr, opt.log_level, path);
    if (NULL == log) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        goto LSND_INIT_ERR;
    }

    /* > 加载配置信息 */
    if (lsnd_load_conf(opt.conf_path, &conf, log)) {
        fprintf(stderr, "Load configuration failed!\n");
        goto LSND_INIT_ERR;
    }

    /* > 初始化侦听 */
    ctx = lsnd_init(&conf, log);
    if (NULL == ctx) {
        fprintf(stderr, "Initialize lsnd failed!\n");
        goto LSND_INIT_ERR;
    }
 
    /* > 注册回调函数 */
    if (lsnd_set_reg(ctx)) {
        fprintf(stderr, "Set register callback failed!\n");
        goto LSND_INIT_ERR;
    }

    /* > 启动侦听服务 */
    if (lsnd_launch(ctx)) {
        fprintf(stderr, "Startup search-engine failed!\n");
        goto LSND_INIT_ERR;
    }

    while (1) { pause(); }

LSND_INIT_ERR:

    return -1;
}

/******************************************************************************
 **函数名称: lsnd_proc_lock
 **功    能: 代理服务进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 使用文件锁
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int lsnd_proc_lock(lsnd_conf_t *conf)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "../temp/listend/%s/lsnd.lck", conf->name);

    Mkdir2(path, DIR_MODE);

    /* 2. 打开文件 */
    fd = Open(path, OPEN_FLAGS, OPEN_MODE);
    if (fd < 0) {
        return -1;
    }

    /* 3. 尝试加锁 */
    if (proc_try_wrlock(fd) < 0) {
        CLOSE(fd);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_init
 **功    能: 初始化进程
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static lsnd_cntx_t *lsnd_init(lsnd_conf_t *conf, log_cycle_t *log)
{
    lsnd_cntx_t *ctx;

    /* > 加进程锁 */
    if (lsnd_proc_lock(conf)) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (lsnd_cntx_t *)calloc(1, sizeof(lsnd_cntx_t));
    if (NULL == ctx) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    memcpy(&ctx->conf, conf, sizeof(lsnd_conf_t));  /* 拷贝配置信息 */

    do {
        /* > 初始化代理信息 */
        ctx->agent = agent_init(&conf->agent, log);
        if (NULL == ctx->agent) {
            log_error(log, "Initialize agent failed!");
            break;
        }

        /* > 初始化RTMQ信息 */
        ctx->invtd_upstrm = rtsd_init(&conf->invtd_conf, log);
        if (NULL == ctx->invtd_upstrm) {
            log_error(log, "Initialize real-time-transport-protocol failed!");
            break;
        }

        return ctx;
    } while (0);

    FREE(ctx);
    return NULL;
}

/******************************************************************************
 **函数名称: lsnd_set_reg
 **功    能: 设置注册函数
 **输入参数:
 **     ctx: 全局信息
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int lsnd_set_reg(lsnd_cntx_t *ctx)
{
#define LSND_AGT_REG_CB(ctx, type, proc, args) /* 注册代理数据回调 */\
    if (agent_register((ctx)->agent, type, (agent_reg_cb_t)proc, (void *)args)) { \
        return LSND_ERR; \
    }

    LSND_AGT_REG_CB(ctx, MSG_SEARCH_WORD_REQ, lsnd_search_word_req_hdl, ctx);
    LSND_AGT_REG_CB(ctx, MSG_INSERT_WORD_REQ, lsnd_insert_word_req_hdl, ctx);

#define LSND_RTQ_REG_CB(lsnd, type, proc, args) /* 注册队列数据回调 */\
    if (rtsd_register((lsnd)->invtd_upstrm, type, (rtmq_reg_cb_t)proc, (void *)args)) { \
        log_error((lsnd)->log, "Register type [%d] failed!", type); \
        return LSND_ERR; \
    }

    LSND_RTQ_REG_CB(ctx, MSG_SEARCH_WORD_RSP, lsnd_search_word_rsp_hdl, ctx);
    LSND_RTQ_REG_CB(ctx, MSG_INSERT_WORD_RSP, lsnd_insert_word_rsp_hdl, ctx);

    return LSND_OK;
}

/******************************************************************************
 **函数名称: lsnd_launch
 **功    能: 启动侦听服务
 **输入参数:
 **     ctx: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.20 22:58:16 #
 ******************************************************************************/
static int lsnd_launch(lsnd_cntx_t *ctx)
{
    /* > 启动代理服务 */
    if (agent_launch(ctx->agent)) {
        log_error(ctx->log, "Startup agent failed!");
        return LSND_ERR;
    }

    /* > 启动代理服务 */
    if (rtsd_launch(ctx->invtd_upstrm)) {
        log_error(ctx->log, "Startup invertd upstream failed!");
        return LSND_ERR;
    }

    return LSND_OK;
}

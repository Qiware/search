/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: lwsd.c
 ** 版本号: 1.0
 ** 描  述: 代理服务
 **         负责接受外界请求，并将处理结果返回给外界
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
#include <syslog.h>

#include "ev.h"
#include "sck.h"
#include "lock.h"
#include "mesg.h"
#include "redo.h"
#include "lwsd.h"
#include "mem_ref.h"
#include "hash_alg.h"
#include "lwsd_mesg.h"
#include "lwsd_search.h"
#include "libwebsockets.h"

static lwsd_cntx_t *lwsd_init(const lwsd_opt_t *opt, const lwsd_conf_t *conf, log_cycle_t *log);
static int lwsd_launch(lwsd_cntx_t *ctx);

static struct libwebsocket_context *lwsd_lws_init(
        const lwsd_opt_t *opt, const lwsd_conf_t *conf, log_cycle_t *log);

static int lwsd_lws_get_attr(const lwsd_opt_t *opt,
        const lwsd_conf_t *conf, struct lws_context_creation_info *info);
static int lwsd_lws_launch(lwsd_cntx_t *ctx);
static int lwsd_set_reg(lwsd_cntx_t *ctx);

/* 服务支持的协议, 以及对应的回调 */
struct libwebsocket_protocols g_lwsd_protocols[] =
{
    {
        "search",                                       /* 协议名 */
        lwsd_callback_search_hdl,                       /* 回调函数 */
        sizeof(lwsd_search_user_data_t),                /* 自定义数据空间大小 */
        0,                                              /* Max frame size / rx buffer */
    },
    { NULL, NULL, 0, 0 }                                /* 结束标识 */
};

/* 全局对象 */
lwsd_cntx_t *g_lwsd_ctx = NULL;

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
    lwsd_opt_t opt;
    lwsd_conf_t conf;
    log_cycle_t *log;
    lwsd_cntx_t *ctx;
    char path[FILE_PATH_MAX_LEN];

    /* > 解析输入参数 */
    if (lwsd_getopt(argc, argv, &opt)) {
        return lwsd_usage(argv[0]);
    } else if (opt.isdaemon) {
        /* int daemon(int nochdir, int noclose);
         *  1． daemon()函数主要用于希望脱离控制台,以守护进程形式在后台运行的程序.
         *  2． 当nochdir为0时,daemon将更改进城的根目录为root(“/”).
         *  3． 当noclose为0是,daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    umask(0);
    mem_ref_init();
    signal(SIGPIPE, SIG_IGN);

    do {
        /* > 初始化日志 */
        log_get_path(path, sizeof(path), basename(argv[0]));

        log = log_init(opt.log_level, path);
        if (NULL == log) {
            fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
            goto LWSD_INIT_ERR;
        }

        /* > 加载配置信息 */
        if (lwsd_load_conf(opt.conf_path, &conf, log)) {
            fprintf(stderr, "Load configuration failed!\n");
            goto LWSD_INIT_ERR;
        }

        /* > 初始化侦听 */
        ctx = lwsd_init(&opt, &conf, log);
        if (NULL == ctx) {
            fprintf(stderr, "Initialize lsnd failed!\n");
            goto LWSD_INIT_ERR;
        }

        LWSD_SET_CTX(ctx);

        /* > 注册回调函数 */
        if (lwsd_set_reg(ctx)) {
            fprintf(stderr, "Register callback failed!\n");
            goto LWSD_INIT_ERR;
        }

        /* > 启动LWS服务 */
        if (lwsd_launch(ctx)) {
            fprintf(stderr, "Startup search-engine failed!\n");
            goto LWSD_INIT_ERR;
        }

        while (1) { pause(); }
    } while(0);

LWSD_INIT_ERR:
    Sleep(2);
    return -1;
}

/******************************************************************************
 **函数名称: lwsd_proc_lock
 **功    能: 代理服务进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 使用文件锁
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int lwsd_proc_lock(const lwsd_conf_t *conf)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s/lsnd.lck", conf->wdir);

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
 **函数名称: lwsd_reg_cmp_cb
 **功    能: 注册比较回调
 **输入参数:
 **     reg1: 注册回调项1
 **     reg2: 注册回调项2
 **输出参数: NONE
 **返    回: =0:相等 <0:小于 >0:大于
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.08.09 10:59:58 #
 ******************************************************************************/
static int lwsd_reg_cmp_cb(const lws_reg_t *reg1, const lws_reg_t *reg2)
{
    return (reg1->type - reg2->type);
}

/******************************************************************************
 **函数名称: lwsd_wsi_map_cmp_cb
 **功    能: 注册比较回调
 **输入参数:
 **     item1: WSI项1
 **     item2: WSI项2
 **输出参数: NONE
 **返    回: =0:相等 <0:小于 >0:大于
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.08.09 10:59:58 #
 ******************************************************************************/
static int lwsd_wsi_map_cmp_cb(lwsd_wsi_item_t *item1, lwsd_wsi_item_t *item2)
{
    return (item1->sid - item2->sid);
}

/******************************************************************************
 **函数名称: lwsd_init
 **功    能: 初始化进程
 **输入参数:
 **     opt: 输入选项
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static lwsd_cntx_t *lwsd_init(const lwsd_opt_t *opt, const lwsd_conf_t *conf, log_cycle_t *log)
{
    lwsd_cntx_t *ctx;

    /* > 加进程锁 */
    if (lwsd_proc_lock(conf)) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (lwsd_cntx_t *)calloc(1, sizeof(lwsd_cntx_t));
    if (NULL == ctx) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    memcpy(&ctx->conf, conf, sizeof(lwsd_conf_t));  /* 拷贝配置信息 */

    do {
        /* > 创建LWS REG表 */
        ctx->lws_reg = avl_creat(NULL, (cmp_cb_t)lwsd_reg_cmp_cb);
        if (NULL == ctx->lws_reg) {
            log_error(log, "Create avl failed!");
            break;
        }

        /* > 创建LWS WSI表 */
        ctx->wsi_map = rbt_creat(NULL, (cmp_cb_t)lwsd_wsi_map_cmp_cb);
        if (NULL == ctx->wsi_map) {
            log_error(log, "Create rbt failed!");
            break;
        }

        /* > 初始化LWS对象 */
        ctx->lws = (struct libwebsocket_context *)lwsd_lws_init(opt, conf, log);
        if (NULL == ctx->lws) {
            log_error(log, "Init lws failed!");
            break;
        }

        /* > 初始化RTMQ信息 */
        ctx->frwder = rtmq_proxy_init(&conf->frwder, log);
        if (NULL == ctx->frwder) {
            log_error(log, "Initialize real-time-transport-protocol failed!");
            break;
        }

        return ctx;
    } while (0);

    FREE(ctx);
    return NULL;
}

/******************************************************************************
 **函数名称: lwsd_set_reg
 **功    能: 设置注册函数
 **输入参数:
 **     ctx: 全局信息
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int lwsd_set_reg(lwsd_cntx_t *ctx)
{
#define LWSD_LWS_REG_CB(lsnd, type, proc, args) /* 注册队列数据回调 */\
    if (lwsd_search_reg_add((lsnd), type, (lws_reg_cb_t)proc, (void *)args)) { \
        log_error((lsnd)->log, "Register type [%d] failed!", type); \
        return LWSD_ERR; \
    }

    LWSD_LWS_REG_CB(ctx, MSG_SEARCH_REQ, lwsd_search_req_hdl, ctx);
    LWSD_LWS_REG_CB(ctx, MSG_INSERT_WORD_REQ, lwsd_insert_word_req_hdl, ctx);

#define LWSD_RTQ_REG_CB(lsnd, type, proc, args) /* 注册队列数据回调 */\
    if (rtmq_proxy_reg_add((lsnd)->frwder, type, (rtmq_reg_cb_t)proc, (void *)args)) { \
        log_error((lsnd)->log, "Register type [%d] failed!", type); \
        return LWSD_ERR; \
    }

    LWSD_RTQ_REG_CB(ctx, MSG_SEARCH_RSP, lwsd_search_rsp_hdl, ctx);
    LWSD_RTQ_REG_CB(ctx, MSG_INSERT_WORD_RSP, lwsd_insert_word_rsp_hdl, ctx);

    return LWSD_OK;
}

/******************************************************************************
 **函数名称: lwsd_launch
 **功    能: 启动侦听服务
 **输入参数:
 **     ctx: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.20 22:58:16 #
 ******************************************************************************/
static int lwsd_launch(lwsd_cntx_t *ctx)
{
    /* > 启动RTMQ服务 */
    if (rtmq_proxy_launch(ctx->frwder)) {
        log_error(ctx->log, "Startup invertd upstream failed!");
        return LWSD_ERR;
    }

    /* > 启动LWS服务 */
    return lwsd_lws_launch(ctx);
}

/******************************************************************************
 **函数名称: lwsd_lws_launch
 **功    能: 启动LWS服务
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.05.28 13:40:53 #
 ******************************************************************************/
static int lwsd_lws_launch(lwsd_cntx_t *ctx)
{
    int n;
    struct libwebsocket_context *lws = ctx->lws;

    n = 0;
    while (n >= 0) {
        /*
         * If libwebsockets sockets are all we care about,
         * you can use this api which takes care of the poll()
         * and looping through finding who needed service.
         *
         * If no socket needs service, it'll return anyway after
         * the number of ms in the second argument.
         */
        n = libwebsocket_service(lws, 50);
    }

    libwebsocket_context_destroy(lws);
    lwsl_notice("lws-access exited cleanly\n");
    closelog();
    return 0;
}

/******************************************************************************
 **函数名称: lwsd_lws_init
 **功    能: 初始化LWS环境
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: LWS对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.06 20:07:45 #
 ******************************************************************************/
static struct libwebsocket_context *lwsd_lws_init(
        const lwsd_opt_t *opt, const lwsd_conf_t *conf, log_cycle_t *log)
{
    struct ev_loop *loop;
    struct libwebsocket_context *lws;
    struct lws_context_creation_info info;

    memset(&info, 0, sizeof info);

    /* 获取LWS属性 */
    if (lwsd_lws_get_attr(opt, conf, &info)) {
        return NULL;
    }

    /* 创建LWS对想 */
    lws = libwebsocket_create_context(&info);
    if (NULL == lws) {
        return NULL;
    }

    loop = ev_loop_new(EVFLAG_AUTO);
    if (NULL == loop) {
        return NULL;
    }

    libwebsocket_initloop(lws, loop);

    return lws;
}

/******************************************************************************
 **函数名称: lwsd_lws_get_attr
 **功    能: 获取LWS属性
 **输入参数:
 **     opt: 输入选项
 **     conf: 配置信息
 **     info: LWS属性
 **输出参数: NONE
 **返    回: LWS对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.06 20:14:52 #
 ******************************************************************************/
static int lwsd_lws_get_attr(const lwsd_opt_t *opt,
        const lwsd_conf_t *lcf, struct lws_context_creation_info *info)
{
    const lws_conf_t *conf = &lcf->lws;

    /* 开启日志 */
    setlogmask(LOG_UPTO (LOG_DEBUG));

    openlog("lwsts", LOG_PID|LOG_PERROR, LOG_DAEMON);

    lws_set_log_level(opt->log_level, lwsl_emit_syslog);

    /* 设置参数 */
    info->port = conf->connections.port;
    info->iface = conf->iface;
    info->protocols = g_lwsd_protocols; // 设置协议回调
#if !defined(LWS_NO_EXTENSIONS)
    info->extensions = libwebsocket_get_internal_extensions();
#endif /*LWS_NO_EXTENSIONS*/

    if (!conf->is_use_ssl) {
        info->ssl_cert_filepath = NULL;
        info->ssl_private_key_filepath = NULL;
    } else {
        info->ssl_cert_filepath = conf->cert_path;
        info->ssl_private_key_filepath = conf->key_path;
    }

    info->gid = -1;
    info->uid = -1;
    info->options = LWS_SERVER_OPTION_LIBEV;

    return 0;
}

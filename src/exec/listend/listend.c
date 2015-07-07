/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: listend.c
 ** 版本号: 1.0
 ** 描  述: 代理服务
 **         负责接受外界请求，并将处理结果返回给外界
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/

#include "sck.h"
#include "lock.h"
#include "hash.h"
#include "mesg.h"
#include "agent.h"
#include "listend.h"
#include "syscall.h"
#include "agent_mesg.h"

static lsnd_cntx_t *lsnd_init(lsnd_conf_t *conf, log_cycle_t *log);
static int lsnd_startup(lsnd_cntx_t *ctx);
static int lsnd_set_reg(lsnd_cntx_t *ctx);
static void lsnd_destroy(lsnd_cntx_t *ctx);

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
    log_cycle_t *log;
    lsnd_cntx_t *ctx = NULL;
    char path[FILE_PATH_MAX_LEN];

    memset(&opt, 0, sizeof(opt));

    /* > 解析输入参数 */
    if (lsnd_getopt(argc, argv, &opt))
    {
        return lsnd_usage(argv[0]);
    }
    else if (opt.isdaemon)
    {
        /* int daemon(int nochdir, int noclose);
         *  1． daemon()函数主要用于希望脱离控制台,以守护进程形式在后台运行的程序.
         *  2． 当nochdir为0时,daemon将更改进城的根目录为root(“/”).
         *  3． 当noclose为0是,daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    umask(0);

    /* > 初始化日志 */
    log_get_path(path, sizeof(path), basename(argv[0]));

    log = log_init(LOG_LEVEL_TRACE, path);
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        goto LSND_INIT_ERR;
    }

    /* > 加载配置信息 */
    if (lsnd_load_conf(opt.conf_path, &conf, log))
    {
        fprintf(stderr, "Load configuration failed!\n");
        goto LSND_INIT_ERR;
    }

    /* > 初始化侦听 */
    ctx = lsnd_init(&conf, log);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize lsnd failed!\n");
        goto LSND_INIT_ERR;
    }
 
    /* > 注册回调函数 */
    if (lsnd_set_reg(ctx))
    {
        fprintf(stderr, "Set register callback failed!\n");
        goto LSND_INIT_ERR;
    }

    /* > 启动侦听服务 */
    if (lsnd_startup(ctx))
    {
        fprintf(stderr, "Startup search-engine failed!\n");
        goto LSND_INIT_ERR;
    }

    while (1) { pause(); }

LSND_INIT_ERR:
    lsnd_destroy(ctx);

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
    if (fd < 0)
    {
        return -1;
    }

    /* 3. 尝试加锁 */
    if (proc_try_wrlock(fd) < 0)
    {
        CLOSE(fd);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_search_word_req_hdl
 **功    能: 搜索请求的处理函数
 **输入参数:
 **     type: 全局对象
 **     data: 数据内容
 **     length: 数据长度
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 请求数据的内存结构: 流水信息 + 消息头 + 消息体
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int lsnd_search_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    agent_flow_t *flow;
    agent_header_t *head;
    mesg_search_word_req_t *req;
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)args;

    flow = (agent_flow_t *)data; // 流水信息
    head = (agent_header_t *)(flow + 1);    // 消息头
    req = (mesg_search_word_req_t *)(head + 1); // 消息体

    log_debug(ctx->log, "Call %s() serial:%lu seq:%lu!", __func__, flow->serial, flow->sck_seq);

    /* > 转发搜索请求 */
    req->serial = hton64(flow->serial);

    return rtsd_cli_send(ctx->send_to_invtd, type, req, sizeof(mesg_search_word_req_t));
}

/******************************************************************************
 **函数名称: lsnd_insert_word_req_hdl
 **功    能: 插入关键字的处理函数
 **输入参数:
 **     type: 全局对象
 **     data: 数据内容
 **     length: 数据长度
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 请求数据的内存结构: 流水信息 + 消息头 + 消息体
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.17 21:34:49 #
 ******************************************************************************/
static int lsnd_insert_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    agent_flow_t *flow;
    agent_header_t *head;
    mesg_insert_word_req_t *req;
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)args;

    log_debug(ctx->log, "Call %s()!", __func__);

    flow = (agent_flow_t *)data;    // 流水信息
    head = (agent_header_t *)(flow + 1); // 消息头
    req = (mesg_insert_word_req_t *)(head + 1); // 消息体

    /* > 转发搜索请求 */
    req->serial = hton64(flow->serial);

    return rtsd_cli_send(ctx->send_to_invtd, type, req, sizeof(mesg_insert_word_req_t));
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
#define LSND_REG_CB(ctx, type, proc, args) /* 注册回调 */\
    if (agent_register((ctx)->agent, type, (agent_reg_cb_t)proc, (void *)args)) \
    { \
        return LSND_ERR; \
    }

    LSND_REG_CB(ctx, MSG_SEARCH_WORD_REQ, lsnd_search_word_req_hdl, ctx);
    LSND_REG_CB(ctx, MSG_INSERT_WORD_REQ, lsnd_insert_word_req_hdl, ctx);
    return LSND_OK;
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
    if (lsnd_proc_lock(conf))
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (lsnd_cntx_t *)calloc(1, sizeof(lsnd_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    log_set_level(log, conf->log_level); /* 设置日志级别 */
    memcpy(&ctx->conf, conf, sizeof(lsnd_conf_t));  /* 拷贝配置信息 */

    do
    {
        /* > 附着分发队列 */
        if (lsnd_attach_distq(ctx))
        {
            log_error(log, "Attach distribute queue failed!");
            break;
        }

        /* > 初始化全局信息 */
        ctx->agent = agent_init(&conf->agent, log);
        if (NULL == ctx->agent)
        {
            log_error(log, "Initialize agent failed!");
            break;
        }

        /* > 初始化RTTP信息 */
        ctx->send_to_invtd = rtsd_cli_init(&conf->to_frwd, 0, log);
        if (NULL == ctx->send_to_invtd)
        {
            log_error(log, "Initialize real-time-transport-protocol failed!");
            break;
        }

        /* > 初始化DSVR服务 */
        if (lsnd_dsvr_init(ctx))
        {
            log_error(log, "Initialize dist-server failed!");
            break;
        }

        return ctx;
    } while (0);

    FREE(ctx);
    return NULL;
}

/******************************************************************************
 **函数名称: lsnd_startup
 **功    能: 启动侦听服务
 **输入参数:
 **     ctx: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.20 22:58:16 #
 ******************************************************************************/
static int lsnd_startup(lsnd_cntx_t *ctx)
{
    pthread_t tid;

    /* > 启动代理服务 */
    if (agent_startup(ctx->agent))
    {
        log_error(ctx->log, "Startup agent failed!");
        return LSND_ERR;
    }

    /* > 启动DSVR线程 */
    thread_creat(&tid, lsnd_dsvr_routine, ctx);

    return LSND_OK;
}

/* 销毁侦听服务 */
static void lsnd_destroy(lsnd_cntx_t *ctx)
{
    if (NULL == ctx) { return; }
    if (ctx->agent) { agent_destroy(ctx->agent); }
    free(ctx);
}

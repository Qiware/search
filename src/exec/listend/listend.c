/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: lsnd.c
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

#define LSND_PROC_LOCK_PATH "../temp/lsnd/lsnd.lck"

static lsnd_cntx_t *lsnd_init(char *pname, const char *path);
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
    lsnd_cntx_t *ctx;

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

    /* > 进程初始化 */
    ctx = lsnd_init(argv[0], opt.conf_path);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize lsnd failed!");
        return -1;
    }
 
    /* 注册回调函数 */
    if (lsnd_set_reg(ctx))
    {
        fprintf(stderr, "Set register callback failed!");
        return -1;
    }

    /* 3. 启动爬虫服务 */
    if (agent_startup(ctx->agent))
    {
        fprintf(stderr, "Startup search-engine failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    /* 4. 销毁全局信息 */
    agent_destroy(ctx->agent);

    return -1;
}

/******************************************************************************
 **函数名称: lsnd_proc_lock
 **功    能: 代理服务进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int lsnd_proc_lock(void)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s", LSND_PROC_LOCK_PATH);

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

/* 初始化SDTP对象 */
static rtsd_cli_t *lsnd_to_invtd_init(rtsd_conf_t *conf, log_cycle_t *log)
{
    return rtsd_cli_init(conf, 0, log);
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
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int lsnd_search_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    agent_flow_t *flow;
    agent_header_t *head;
    mesg_search_word_body_t *body;
    mesg_search_word_req_t req;
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)args;

    log_debug(ctx->log, "Call %s()!", __func__);

    flow = (agent_flow_t *)data;
    head = (agent_header_t *)(flow + 1);
    body = (mesg_search_word_body_t *)(head + 1);

    /* > 转发搜索请求 */
    req.serial = flow->serial;
    memcpy(&req.body, body, sizeof(mesg_search_word_body_t));

    return rtsd_cli_send(ctx->send_to_invtd, type, &req, sizeof(req));
}

/* 插入关键字的处理函数 */
static int lsnd_insert_word_req_hdl(unsigned int type, void *data, int length, void *args)
{
    return 0;
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
 **     pname: 进程名
 **     path: 配置路径
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static lsnd_cntx_t *lsnd_init(char *pname, const char *path)
{
    pthread_t tid;
    log_cycle_t *log;
    lsnd_cntx_t *ctx;
    lsnd_conf_t *conf;
    agent_cntx_t *agent;
    rtsd_cli_t *send_to_invtd;
    char log_path[FILE_NAME_MAX_LEN];

    /* > 加进程锁 */
    if (lsnd_proc_lock())
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (lsnd_cntx_t *)calloc(1, sizeof(lsnd_cntx_t));
    if (NULL == ctx)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 初始化日志 */
    log_get_path(log_path, sizeof(log_path), basename(pname));

    log = log_init(LOG_LEVEL_ERROR, log_path);
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 加载配置信息 */
    conf = lsnd_load_conf(path, log);
    if (NULL == conf)
    {
        FREE(ctx);
        fprintf(stderr, "Load configuration failed!\n");
        return NULL;
    }

    ctx->log = log;
    ctx->conf = conf;

    /* > 创建Agentd发送队列 */
    ctx->sendq = shm_queue_attach(LSND_SHM_SENDQ_PATH);
    if (NULL == ctx->sendq)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 初始化全局信息 */
    agent = agent_init(&conf->agent, log);
    if (NULL == agent)
    {
        fprintf(stderr, "Initialize search-engine failed!");
        return NULL;
    }

    /* > 初始化SDTP信息 */
    send_to_invtd = lsnd_to_invtd_init(&conf->to_frwd, log);
    if (NULL == send_to_invtd)
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    /* 启动分发线程 */
    thread_creat(&tid, lsnd_dist_routine, ctx);

    ctx->send_to_invtd = send_to_invtd;
    ctx->agent = agent;

    return ctx;
}

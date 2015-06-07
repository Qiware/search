/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: agentd.c
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
#include "agentd.h"
#include "syscall.h"
#include "agent_mesg.h"

#define AGTD_PROC_LOCK_PATH "../temp/agentd/agentd.lck"

static agentd_cntx_t *agentd_init(char *pname, const char *path);
static int agentd_set_reg(agentd_cntx_t *ctx);

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
    agentd_opt_t opt;
    agentd_cntx_t *ctx;

    memset(&opt, 0, sizeof(opt));

    /* > 解析输入参数 */
    if (agentd_getopt(argc, argv, &opt))
    {
        return agentd_usage(argv[0]);
    }

    if (opt.isdaemon)
    {
        /* int daemon(int nochdir, int noclose);
         *  1． daemon()函数主要用于希望脱离控制台,以守护进程形式在后台运行的程序.
         *  2． 当nochdir为0时,daemon将更改进城的根目录为root(“/”).
         *  3． 当noclose为0是,daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    /* > 进程初始化 */
    ctx = agentd_init(argv[0], opt.conf_path);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize agentd failed!");
        return -1;
    }
 
    /* 注册回调函数 */
    if (agentd_set_reg(ctx))
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
 **函数名称: agentd_proc_lock
 **功    能: 代理服务进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agentd_proc_lock(void)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s", AGTD_PROC_LOCK_PATH);

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
#if defined(__RTTP_SUPPORT__)
static rtsd_cli_t *agentd_to_invtd_init(rtsd_conf_t *conf, log_cycle_t *log)
{
    return rtsd_cli_init(conf, 0, log);
}
#else /*__RTTP_SUPPORT__*/
static sdsd_cli_t *agentd_to_invtd_init(sdsd_conf_t *conf, log_cycle_t *log)
{
    return sdsd_cli_init(conf, 0, log);
}
#endif /*__RTTP_SUPPORT__*/

/******************************************************************************
 **函数名称: agentd_search_req_hdl
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
static int agentd_search_req_hdl(unsigned int type, void *data, int length, void *args)
{
    mesg_search_req_t req;
    agent_flow_t *flow;
    srch_mesg_body_t *body;
    agent_header_t *head;
    agentd_cntx_t *ctx = (agentd_cntx_t *)args;

    flow = (agent_flow_t *)data;
    head = (agent_header_t *)(flow + 1);
    body = (srch_mesg_body_t *)(head + 1);

    /* > 转发搜索请求 */
    req.serial = flow->serial;
    memcpy(&req.body, body, sizeof(srch_mesg_body_t));

#if defined(__RTTP_SUPPORT__)
    return rtsd_cli_send(ctx->send_to_invtd, type, &req, sizeof(req));
#else /*__RTTP_SUPPORT__*/
    return sdsd_cli_send(ctx->send_to_invtd, type, &req, sizeof(req));
#endif /*__RTTP_SUPPORT__*/
}

/******************************************************************************
 **函数名称: agentd_set_reg
 **功    能: 设置注册函数
 **输入参数:
 **     ctx: 全局信息
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int agentd_set_reg(agentd_cntx_t *ctx)
{
    if (agent_register(ctx->agent, MSG_SEARCH_REQ, (agent_reg_cb_t)agentd_search_req_hdl, (void *)ctx))
    {
        return AGTD_ERR;
    }

    return AGTD_OK;
}

/******************************************************************************
 **函数名称: agentd_init
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
static agentd_cntx_t *agentd_init(char *pname, const char *path)
{
    pthread_t tid;
    log_cycle_t *log;
    agentd_cntx_t *ctx;
    agentd_conf_t *conf;
    agent_cntx_t *agent;
#if defined(__RTTP_SUPPORT__)
    rtsd_cli_t *send_to_invtd;
#else /*__RTTP_SUPPORT__*/
    sdsd_cli_t *send_to_invtd;
#endif /*__RTTP_SUPPORT__*/

    /* > 加进程锁 */
    if (agentd_proc_lock())
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (agentd_cntx_t *)calloc(1, sizeof(agentd_cntx_t));
    if (NULL == ctx)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 初始化日志 */
    log = agentd_init_log(pname);
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 加载配置信息 */
    conf = agentd_load_conf(path, log);
    if (NULL == conf)
    {
        FREE(ctx);
        fprintf(stderr, "Load configuration failed!\n");
        return NULL;
    }

    ctx->log = log;
    ctx->conf = conf;

    /* > 创建Agentd发送队列 */
    ctx->sendq = shm_queue_attach(AGTD_SHM_SENDQ_PATH);
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
    send_to_invtd = agentd_to_invtd_init(&conf->sdtp, log);
    if (NULL == send_to_invtd)
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    /* 启动分发线程 */
    thread_creat(&tid, agentd_dist_routine, ctx);

    ctx->send_to_invtd = send_to_invtd;
    ctx->agent = agent;

    return ctx;
}

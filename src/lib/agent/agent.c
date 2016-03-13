#include "log.h"
#include "sck.h"
#include "comm.h"
#include "lock.h"
#include "redo.h"
#include "search.h"
#include "hash_alg.h"
#include "agent_rsvr.h"
#include "agent_worker.h"
#include "agent_listen.h"

static log_cycle_t *agent_init_log(char *fname);
static int agent_comm_init(agent_cntx_t *ctx);
static int agent_init_reg(agent_cntx_t *ctx);
static int agent_creat_agents(agent_cntx_t *ctx);
static int agent_rsvr_pool_destroy(agent_cntx_t *ctx);
static int agent_creat_workers(agent_cntx_t *ctx);
static int agent_workers_destroy(agent_cntx_t *ctx);
static int agent_creat_listens(agent_cntx_t *ctx);
static int agent_creat_queue(agent_cntx_t *ctx);

/******************************************************************************
 **函数名称: agent_init
 **功    能: 初始化全局信息
 **输入参数: 
 **     conf_path: 配置路径
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
agent_cntx_t *agent_init(agent_conf_t *conf, log_cycle_t *log)
{
    agent_cntx_t *ctx;

    /* > 创建全局对象 */
    ctx = (agent_cntx_t *)calloc(1, sizeof(agent_cntx_t));
    if (NULL == ctx) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    ctx->conf = conf;

    do {
        /* > 注册消息处理 */
        if (agent_init_reg(ctx)) {
            log_error(log, "Initialize register failed!");
            break;
        }

        /* > 创建流水->SCK映射表 */
        ctx->serial_to_sck_map = agent_serial_to_sck_map_init(ctx);
        if (NULL == ctx->serial_to_sck_map) {
            log_error(log, "Initialize serial to sck map failed!");
            break;
        }

        /* > 设置进程打开文件数 */
        if (set_fd_limit(conf->connections.max)) {
            log_error(log, "errmsg:[%d] %s! max:%d",
                      errno, strerror(errno), conf->connections.max);
            break;
        }

        /* > 创建队列 */
        if (agent_creat_queue(ctx)) {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建Worker线程池 */
        if (agent_creat_workers(ctx)) {
            log_error(log, "Initialize worker thread pool failed!");
            break;
        }

        /* > 创建Agent线程池 */
        if (agent_creat_agents(ctx)) {
            log_error(log, "Initialize agent thread pool failed!");
            break;
        }

        /* > 创建Listen线程池 */
        if (agent_creat_listens(ctx)) {
            log_error(log, "Initialize agent thread pool failed!");
            break;
        }

        /* > 初始化其他信息 */
        if (agent_comm_init(ctx)) {
            log_error(log, "Initialize client failed!");
            break;
        }

        return ctx;
    } while (0);

    free(ctx);
    return NULL;
}

/******************************************************************************
 **函数名称: agent_destroy
 **功    能: 销毁代理服务上下文
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 依次销毁侦听线程、接收线程、工作线程、日志对象等
 **注意事项: 按序销毁
 **作    者: # Qifeng.zou # 2014.11.17 #
 ******************************************************************************/
void agent_destroy(agent_cntx_t *ctx)
{
    agent_workers_destroy(ctx);
    agent_rsvr_pool_destroy(ctx);

    log_destroy(&ctx->log);
}

/******************************************************************************
 **函数名称: agent_launch
 **功    能: 启动代理服务
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     设置线程回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int agent_launch(agent_cntx_t *ctx)
{
    int idx;
    agent_conf_t *conf = ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker_num; ++idx) {
        thread_pool_add_worker(ctx->workers, agent_worker_routine, ctx);
    }

    /* 2. 设置Agent线程回调 */
    for (idx=0; idx<conf->agent_num; ++idx) {
        thread_pool_add_worker(ctx->agents, agent_rsvr_routine, ctx);
    }
    
    /* 3. 设置Listen线程回调 */
    for (idx=0; idx<conf->lsn_num; ++idx) {
        thread_pool_add_worker(ctx->listens, agent_listen_routine, ctx);
    }
 
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_creat_workers
 **功    能: 创建Worker线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agent_creat_workers(agent_cntx_t *ctx)
{
    int idx, num;
    agent_worker_t *worker;
    const agent_conf_t *conf = ctx->conf;

    /* > 新建Worker对象 */
    worker = (agent_worker_t *)calloc(1, conf->worker_num*sizeof(agent_worker_t));
    if (NULL == worker) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    /* > 创建Worker线程池 */
    ctx->workers = thread_pool_init(conf->worker_num, NULL, worker);
    if (NULL == ctx->workers) {
        log_error(ctx->log, "Initialize thread pool failed!");
        return AGENT_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx) {
        if (agent_worker_init(ctx, worker+idx, idx)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->worker_num) {
        return AGENT_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx) {
        agent_worker_destroy(worker+idx);
    }

    FREE(worker);
    thread_pool_destroy(ctx->workers);

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_workers_destroy
 **功    能: 销毁爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
static int agent_workers_destroy(agent_cntx_t *ctx)
{
    int idx;
    agent_worker_t *worker;
    const agent_conf_t *conf = ctx->conf;

    /* > 释放Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx) {
        worker = (agent_worker_t *)ctx->workers->data + idx;

        agent_worker_destroy(worker);
    }

    /* > 释放线程池对象 */
    FREE(ctx->workers->data);
    thread_pool_destroy(ctx->workers);

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_creat_agents
 **功    能: 创建Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agent_creat_agents(agent_cntx_t *ctx)
{
    int idx, num;
    agent_rsvr_t *agent;
    const agent_conf_t *conf = ctx->conf;

    /* > 新建Agent对象 */
    agent = (agent_rsvr_t *)calloc(1, conf->agent_num*sizeof(agent_rsvr_t));
    if (NULL == agent) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    /* > 创建Worker线程池 */
    ctx->agents = thread_pool_init(conf->agent_num, NULL, agent);
    if (NULL == ctx->agents) {
        log_error(ctx->log, "Initialize thread pool failed!");
        free(agent);
        return AGENT_ERR;
    }

    /* 3. 依次初始化Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx) {
        if (agent_rsvr_init(ctx, agent+idx, idx)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->agent_num) {
        return AGENT_OK; /* 成功 */
    }

    /* 4. 释放Agent对象 */
    num = idx;
    for (idx=0; idx<num; ++idx) {
        agent_rsvr_destroy(agent+idx);
    }

    FREE(agent);
    thread_pool_destroy(ctx->agents);

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_creat_listens
 **功    能: 创建Listen线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-30 15:06:58 #
 ******************************************************************************/
static int agent_creat_listens(agent_cntx_t *ctx)
{
    int idx;
    agent_lsvr_t *lsvr;
    agent_conf_t *conf = ctx->conf;

    /* > 侦听指定端口 */
    ctx->listen.lsn_sck_id = tcp_listen(conf->connections.port);
    if (ctx->listen.lsn_sck_id < 0) {
        log_error(ctx->log, "errmsg:[%d] %s! port:%d",
                  errno, strerror(errno), conf->connections.port);
        return AGENT_ERR;
    }

    spin_lock_init(&ctx->listen.accept_lock);

    /* > 创建LSN对象 */
    ctx->listen.lsvr = (agent_lsvr_t *)calloc(1, conf->lsn_num*sizeof(agent_lsvr_t));
    if (NULL == ctx->listen.lsvr) {
        CLOSE(ctx->listen.lsn_sck_id);
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    /* > 初始化侦听服务 */
    for (idx=0; idx<conf->lsn_num; ++idx) {
        lsvr = ctx->listen.lsvr + idx;
        lsvr->log = ctx->log;
        if (agent_listen_init(ctx, lsvr, idx)) {
            CLOSE(ctx->listen.lsn_sck_id);
            FREE(ctx->listen.lsvr);
            log_error(ctx->log, "Initialize listen-server failed!");
            return AGENT_ERR;
        }
    }

    ctx->listens = thread_pool_init(conf->lsn_num, NULL, ctx->listen.lsvr);
    if (NULL == ctx->listens) {
        CLOSE(ctx->listen.lsn_sck_id);
        FREE(ctx->listen.lsvr);
        log_error(ctx->log, "Initialize thread pool failed!");
        return AGENT_ERR;
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_pool_destroy
 **功    能: 销毁Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agent_rsvr_pool_destroy(agent_cntx_t *ctx)
{
    int idx;
    void *data;
    agent_rsvr_t *agent;
    const agent_conf_t *conf = ctx->conf;

    /* 1. 释放Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx) {
        agent = (agent_rsvr_t *)ctx->agents->data + idx;

        agent_rsvr_destroy(agent);
    }

    /* 2. 释放线程池对象 */
    data = ctx->agents->data;

    thread_pool_destroy(ctx->agents);

    free(data);

    ctx->agents = NULL;

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_reg_def_hdl
 **功    能: 默认注册函数
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
static int agent_reg_def_hdl(unsigned int type, char *buff, size_t len, void *args)
{
    static int total = 0;
    agent_cntx_t *ctx = (agent_cntx_t *)args;

    log_info(ctx->log, "Call: %s()! total:%d", __func__, ++total);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_init_reg
 **功    能: 初始化注册消息处理
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
static int agent_init_reg(agent_cntx_t *ctx)
{
    unsigned int idx;
    agent_reg_t *reg;

    for (idx=0; idx<=AGENT_MSG_TYPE_MAX; ++idx) {
        reg = &ctx->reg[idx];

        reg->type = idx;
        reg->proc = agent_reg_def_hdl;
        reg->args = ctx;
        reg->flag = AGENT_REG_FLAG_UNREG;
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_register
 **功    能: 注册消息处理函数
 **输入参数:
 **     ctx: 全局信息
 **     type: 扩展消息类型. Range:(0 ~ AGENT_MSG_TYPE_MAX)
 **     proc: 指定消息类型对应的处理函数
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     1. 只能用于注册处理扩展数据类型的处理
 **     2. 不允许重复注册 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
int agent_register(agent_cntx_t *ctx, unsigned int type, agent_reg_cb_t proc, void *args)
{
    agent_reg_t *reg;

    if (type >= AGENT_MSG_TYPE_MAX
        || 0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Type 0x%02X is invalid or repeat reg!", type);
        return AGENT_ERR;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->proc = proc;
    reg->args = args;
    reg->flag = 1;

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_creat_queue
 **功    能: 创建队列
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 此过程一旦失败, 程序必须退出运行. 因此, 在此申请的内存未被主动释放也不算内存泄露!
 **作    者: # Qifeng.zou # 2014.12.21 #
 ******************************************************************************/
static int agent_creat_queue(agent_cntx_t *ctx)
{
    int idx;
    const agent_conf_t *conf = ctx->conf;

    /* > 创建CONN队列(与Agent数一致) */
    ctx->connq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->connq) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx) {
        ctx->connq[idx] = queue_creat(conf->connq.max, sizeof(agent_add_sck_t));
        if (NULL == ctx->connq) {
            log_error(ctx->log, "Create conn queue failed!");
            return AGENT_ERR;
        }
    }

    /* > 创建RECV队列(与Agent数一致) */
    ctx->recvq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->recvq) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx) {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq) {
            log_error(ctx->log, "Create recv queue failed!");
            return AGENT_ERR;
        }
    }

    /* > 创建SEND队列(与Agent数一致) */
    ctx->sendq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->sendq) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx) {
        ctx->sendq[idx] = queue_creat(conf->sendq.max, conf->sendq.size);
        if (NULL == ctx->sendq) {
            log_error(ctx->log, "Create send queue failed!");
            return AGENT_ERR;
        }
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_comm_init
 **功    能: 初始化通用信息
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-24 23:58:46 #
 ******************************************************************************/
static int agent_comm_init(agent_cntx_t *ctx)
{
    char path[FILE_PATH_MAX_LEN];

    snprintf(path, sizeof(path), "%s/"AGENT_CLI_CMD_PATH, ctx->conf->path);

    ctx->cmd_sck_id = unix_udp_creat(path);
    if (ctx->cmd_sck_id < 0) {
        return AGENT_ERR;
    }

    return AGENT_OK;
}

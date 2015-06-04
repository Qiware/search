#include "log.h"
#include "sck.h"
#include "comm.h"
#include "lock.h"
#include "hash.h"
#include "search.h"
#include "syscall.h"
#include "agent_rsvr.h"
#include "agent_worker.h"
#include "agent_listen.h"

static log_cycle_t *agent_init_log(char *fname);
static int agent_init_reg(agent_cntx_t *ctx);
static int agent_creat_agent_pool(agent_cntx_t *ctx);
static int agent_rsvr_pool_destroy(agent_cntx_t *ctx);
static int agent_creat_worker_pool(agent_cntx_t *ctx);
static int agent_worker_pool_destroy(agent_cntx_t *ctx);
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
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    ctx->conf = conf;

    /* > 创建内存池 */
    ctx->slab = slab_creat_by_calloc(30 * MB);
    if (NULL == ctx->slab)
    {
        free(ctx);
        log_error(log, "Init slab failed!");
        return NULL;
    }

    do
    {
        /* > 注册消息处理 */
        if (agent_init_reg(ctx))
        {
            log_error(log, "Initialize register failed!");
            break;
        }

        /* > 创建流水->SCK映射表 */
        if (agent_serial_to_sck_map_init(ctx))
        {
            log_error(log, "Initialize serial to sck map failed!");
            break;
        }

        /* > 设置进程打开文件数 */
        if (set_fd_limit(conf->connections.max))
        {
            log_error(log, "errmsg:[%d] %s! max:%d", errno, strerror(errno), conf->connections.max);
            break;
        }

        /* > 创建队列 */
        if (agent_creat_queue(ctx))
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建Worker线程池 */
        if (agent_creat_worker_pool(ctx))
        {
            log_error(log, "Initialize worker thread pool failed!");
            break;
        }

        /* > 创建Agent线程池 */
        if (agent_creat_agent_pool(ctx))
        {
            log_error(log, "Initialize agent thread pool failed!");
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
    pthread_cancel(ctx->lsn_tid);
    agent_worker_pool_destroy(ctx);
    agent_rsvr_pool_destroy(ctx);

    log_destroy(&ctx->log);
    plog_destroy();
}

/******************************************************************************
 **函数名称: agent_startup
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
int agent_startup(agent_cntx_t *ctx)
{
    int idx;
    agent_conf_t *conf = ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        thread_pool_add_worker(ctx->worker_pool, agent_worker_routine, ctx);
    }

    /* 2. 设置Agent线程回调 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        thread_pool_add_worker(ctx->agent_pool, agent_rsvr_routine, ctx);
    }
    
    /* 3. 设置Listen线程回调 */
    if (thread_creat(&ctx->lsn_tid, agent_listen_routine, ctx))
    {
        log_error(ctx->log, "Create listen thread failed!");
        return AGENT_ERR;
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_creat_worker_pool
 **功    能: 创建Worker线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agent_creat_worker_pool(agent_cntx_t *ctx)
{
    int idx, num;
    agent_worker_t *worker;
    thread_pool_opt_t opt;
    const agent_conf_t *conf = ctx->conf;

    /* > 新建Worker对象 */
    worker = (agent_worker_t *)calloc(conf->worker_num, sizeof(agent_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    /* > 创建Worker线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->worker_pool = thread_pool_init(conf->worker_num, &opt, worker);
    if (NULL == ctx->worker_pool)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        return AGENT_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        if (agent_worker_init(ctx, worker+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->worker_num)
    {
        return AGENT_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        agent_worker_destroy(worker+idx);
    }

    free(worker);
    thread_pool_destroy(ctx->worker_pool);

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_worker_pool_destroy
 **功    能: 销毁爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
static int agent_worker_pool_destroy(agent_cntx_t *ctx)
{
    int idx;
    void *data;
    agent_worker_t *worker;
    const agent_conf_t *conf = ctx->conf;

    /* 1. 释放Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        worker = (agent_worker_t *)ctx->worker_pool->data + idx;

        agent_worker_destroy(worker);
    }

    /* 2. 释放线程池对象 */
    data = ctx->worker_pool->data;

    thread_pool_destroy(ctx->worker_pool);

    free(data);

    ctx->worker_pool = NULL;

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_creat_agent_pool
 **功    能: 创建Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agent_creat_agent_pool(agent_cntx_t *ctx)
{
    int idx, num;
    agent_rsvr_t *agent;
    thread_pool_opt_t opt;
    const agent_conf_t *conf = ctx->conf;

    /* > 新建Agent对象 */
    agent = (agent_rsvr_t *)calloc(conf->agent_num, sizeof(agent_rsvr_t));
    if (NULL == agent)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    /* > 创建Worker线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->agent_pool = thread_pool_init(conf->agent_num, &opt, agent);
    if (NULL == ctx->agent_pool)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        free(agent);
        return AGENT_ERR;
    }

    /* 3. 依次初始化Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        if (agent_rsvr_init(ctx, agent+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->agent_num)
    {
        return AGENT_OK; /* 成功 */
    }

    /* 4. 释放Agent对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        agent_rsvr_destroy(agent+idx);
    }

    free(agent);
    thread_pool_destroy(ctx->agent_pool);

    return AGENT_ERR;
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
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        agent = (agent_rsvr_t *)ctx->agent_pool->data + idx;

        agent_rsvr_destroy(agent);
    }

    /* 2. 释放线程池对象 */
    data = ctx->agent_pool->data;

    thread_pool_destroy(ctx->agent_pool);

    free(data);

    ctx->agent_pool = NULL;

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

    for (idx=0; idx<=AGENT_MSG_TYPE_MAX; ++idx)
    {
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
    key_t key;
    const agent_conf_t *conf = ctx->conf;

    /* > 创建CONN队列(与Agent数一致) */
    ctx->connq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->connq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->connq[idx] = queue_creat(conf->connq.max, sizeof(agent_add_sck_t));
        if (NULL == ctx->connq)
        {
            log_error(ctx->log, "Create conn queue failed!");
            return AGENT_ERR;
        }
    }

    /* > 创建RECV队列(与Agent数一致) */
    ctx->recvq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == ctx->recvq)
        {
            log_error(ctx->log, "Create recv queue failed!");
            return AGENT_ERR;
        }
    }

    /* > 创建SEND队列(与Agent数一致) */
    ctx->sendq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->sendq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->sendq[idx] = queue_creat(conf->sendq.max, conf->sendq.size);
        if (NULL == ctx->sendq)
        {
            log_error(ctx->log, "Create send queue failed!");
            return AGENT_ERR;
        }
    }

    /* > 创建SHM-SEND队列 */
    key = 0;
    ctx->shm_sendq = shm_queue_creat(key, conf->sendq.max, conf->sendq.size);
    if (NULL == ctx->shm_sendq)
    {
        log_error(ctx->log, "Create shm-send-queue failed!");
        return AGENT_ERR;
    }

    return AGENT_OK;
}

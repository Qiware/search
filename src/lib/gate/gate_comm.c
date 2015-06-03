/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: search.c
 ** 版本号: 1.0
 ** 描  述: 探针服务
 **         负责接受各种请求，并将最终结果返回给客户端
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/

#include "log.h"
#include "sck.h"
#include "comm.h"
#include "lock.h"
#include "hash.h"
#include "search.h"
#include "syscall.h"
#include "gate_worker.h"
#include "gate_listen.h"
#include "gate_agent.h"

static log_cycle_t *gate_init_log(char *fname);
static int gate_creat_agent_pool(gate_cntx_t *ctx);
static int gate_agent_pool_destroy(gate_cntx_t *ctx);
static int gate_creat_worker_pool(gate_cntx_t *ctx);
static int gate_worker_pool_destroy(gate_cntx_t *ctx);
static int gate_creat_queue(gate_cntx_t *ctx);

/******************************************************************************
 **函数名称: gate_cntx_init
 **功    能: 初始化全局信息
 **输入参数: 
 **     conf_path: 配置文件路径
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **     1. 初始化日志模块
 **     2. 判断程序是否已运行
 **     3. 创建全局对象 
 **     4. 加载配置文件
 **     5. 创建Worker线程池
 **     6. 创建Agent线程池
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
gate_cntx_t *gate_cntx_init(gate_conf_t *conf, log_cycle_t *log)
{
    gate_cntx_t *ctx;

    /* > 创建全局对象 */
    ctx = (gate_cntx_t *)calloc(1, sizeof(gate_cntx_t));
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
        if (gate_init_register(ctx))
        {
            log_error(log, "Initialize register failed!");
            break;
        }

        /* > 设置进程打开文件数 */
        if (set_fd_limit(conf->connections.max))
        {
            log_error(log, "errmsg:[%d] %s! max:%d", errno, strerror(errno), conf->connections.max);
            break;
        }

        /* > 创建队列 */
        if (gate_creat_queue(ctx))
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建Worker线程池 */
        if (gate_creat_worker_pool(ctx))
        {
            log_error(log, "Initialize worker thread pool failed!");
            break;
        }

        /* > 创建Agent线程池 */
        if (gate_creat_agent_pool(ctx))
        {
            log_error(log, "Initialize agent thread pool failed!");
            break;
        }

        return ctx;
    } while (0);

    gate_conf_destroy(ctx->conf);
    free(ctx);
    return NULL;
}

/******************************************************************************
 **函数名称: gate_cntx_destroy
 **功    能: 销毁探针服务上下文
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     依次销毁侦听线程、接收线程、工作线程、日志对象等
 **注意事项: 按序销毁
 **作    者: # Qifeng.zou # 2014.11.17 #
 ******************************************************************************/
void gate_cntx_destroy(gate_cntx_t *ctx)
{
    pthread_cancel(ctx->lsn_tid);
    gate_worker_pool_destroy(ctx);
    gate_agent_pool_destroy(ctx);

    log_destroy(&ctx->log);
    plog_destroy();
}

/******************************************************************************
 **函数名称: gate_startup
 **功    能: 启动探针服务服务
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     设置线程回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int gate_startup(gate_cntx_t *ctx)
{
    int idx;
    gate_conf_t *conf = ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        thread_pool_add_worker(ctx->worker_pool, gate_worker_routine, ctx);
    }

    /* 2. 设置Agent线程回调 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        thread_pool_add_worker(ctx->agent_pool, gate_agent_routine, ctx);
    }
    
    /* 3. 设置Listen线程回调 */
    if (thread_creat(&ctx->lsn_tid, gate_listen_routine, ctx))
    {
        log_error(ctx->log, "Create listen thread failed!");
        return GATE_ERR;
    }

    return GATE_OK;
}

/******************************************************************************
 **函数名称: gate_creat_worker_pool
 **功    能: 创建Worker线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int gate_creat_worker_pool(gate_cntx_t *ctx)
{
    int idx, num;
    gate_worker_t *worker;
    thread_pool_opt_t opt;
    const gate_conf_t *conf = ctx->conf;

    /* > 新建Worker对象 */
    worker = (gate_worker_t *)calloc(conf->worker_num, sizeof(gate_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return GATE_ERR;
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
        return GATE_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        if (gate_worker_init(ctx, worker+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->worker_num)
    {
        return GATE_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        gate_worker_destroy(worker+idx);
    }

    free(worker);
    thread_pool_destroy(ctx->worker_pool);

    return GATE_ERR;
}

/******************************************************************************
 **函数名称: gate_worker_pool_destroy
 **功    能: 销毁爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
static int gate_worker_pool_destroy(gate_cntx_t *ctx)
{
    int idx;
    void *data;
    gate_worker_t *worker;
    const gate_conf_t *conf = ctx->conf;

    /* 1. 释放Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        worker = (gate_worker_t *)ctx->worker_pool->data + idx;

        gate_worker_destroy(worker);
    }

    /* 2. 释放线程池对象 */
    data = ctx->worker_pool->data;

    thread_pool_destroy(ctx->worker_pool);

    free(data);

    ctx->worker_pool = NULL;

    return GATE_ERR;
}

/******************************************************************************
 **函数名称: gate_creat_agent_pool
 **功    能: 创建Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int gate_creat_agent_pool(gate_cntx_t *ctx)
{
    int idx, num;
    gate_agent_t *agent;
    thread_pool_opt_t opt;
    const gate_conf_t *conf = ctx->conf;

    /* > 新建Agent对象 */
    agent = (gate_agent_t *)calloc(conf->agent_num, sizeof(gate_agent_t));
    if (NULL == agent)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return GATE_ERR;
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
        return GATE_ERR;
    }

    /* 3. 依次初始化Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        if (gate_agent_init(ctx, agent+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->agent_num)
    {
        return GATE_OK; /* 成功 */
    }

    /* 4. 释放Agent对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        gate_agent_destroy(agent+idx);
    }

    free(agent);
    thread_pool_destroy(ctx->agent_pool);

    return GATE_ERR;
}

/******************************************************************************
 **函数名称: gate_agent_pool_destroy
 **功    能: 销毁Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int gate_agent_pool_destroy(gate_cntx_t *ctx)
{
    int idx;
    void *data;
    gate_agent_t *agent;
    const gate_conf_t *conf = ctx->conf;

    /* 1. 释放Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        agent = (gate_agent_t *)ctx->agent_pool->data + idx;

        gate_agent_destroy(agent);
    }

    /* 2. 释放线程池对象 */
    data = ctx->agent_pool->data;

    thread_pool_destroy(ctx->agent_pool);

    free(data);

    ctx->agent_pool = NULL;

    return GATE_ERR;
}

/******************************************************************************
 **函数名称: gate_reg_def_hdl
 **功    能: 默认注册函数
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
static int gate_reg_def_hdl(unsigned int type, char *buff, size_t len, void *args, log_cycle_t *log)
{
    static int total = 0;

    log_info(log, "Call: %s()! total:%d", __func__, ++total);

    return GATE_OK;
}

/******************************************************************************
 **函数名称: gate_init_register
 **功    能: 初始化注册消息处理
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
int gate_init_register(gate_cntx_t *ctx)
{
    unsigned int idx;
    gate_reg_t *reg;

    for (idx=0; idx<=GATE_MSG_TYPE_MAX; ++idx)
    {
        reg = &ctx->reg[idx];

        reg->type = idx;
        reg->proc = gate_reg_def_hdl;
        reg->args = NULL;
        reg->flag = GATE_REG_FLAG_UNREG;
    }

    return GATE_OK;
}

/******************************************************************************
 **函数名称: gate_register
 **功    能: 注册消息处理函数
 **输入参数:
 **     ctx: 全局信息
 **     type: 扩展消息类型. Range:(0 ~ GATE_MSG_TYPE_MAX)
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
int gate_register(gate_cntx_t *ctx, unsigned int type, gate_reg_cb_t proc, void *args)
{
    gate_reg_t *reg;

    if (type >= GATE_MSG_TYPE_MAX
        || 0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Type 0x%02X is invalid or repeat reg!", type);
        return GATE_ERR;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->proc = proc;
    reg->args = args;
    reg->flag = 1;

    return GATE_OK;
}

/******************************************************************************
 **函数名称: gate_creat_queue
 **功    能: 创建队列
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     此过程一旦失败, 程序必须推出运行. 因此, 在此申请的内存未被主动释放也不算内存泄露!
 **作    者: # Qifeng.zou # 2014.12.21 #
 ******************************************************************************/
static int gate_creat_queue(gate_cntx_t *ctx)
{
    int idx;
    const gate_conf_t *conf = ctx->conf;

    /* 1. 创建连接队列(与Agent数一致) */
    ctx->connq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->connq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return GATE_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->connq[idx] = queue_creat(conf->connq.max, sizeof(gate_add_sck_t));
        if (NULL == ctx->connq)
        {
            log_error(ctx->log, "Initialize lock queue failed!");
            return GATE_ERR;
        }
    }

    /* 2. 创建接收队列(与Agent数一致) */
    ctx->recvq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return GATE_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->taskq.max, conf->taskq.size);
        if (NULL == ctx->recvq)
        {
            log_error(ctx->log, "Initialize lock queue failed!");
            return GATE_ERR;
        }
    }

    return GATE_OK;
}

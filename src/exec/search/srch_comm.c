/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: search.c
 ** 版本号: 1.0
 ** 描  述: 搜索引擎
 **         负责接受搜索请求，并将搜索结果返回给客户端
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/

#include "log.h"
#include "sck.h"
#include "comm.h"
#include "lock.h"
#include "hash.h"
#include "search.h"
#include "syscall.h"
#include "srch_worker.h"
#include "srch_listen.h"
#include "srch_agent.h"

#define SRCH_PROC_LOCK_PATH "../temp/srch/srch.lck"


static log_cycle_t *srch_init_log(char *fname);
static int srch_proc_lock(void);
static int srch_creat_agent_pool(srch_cntx_t *ctx);
static int srch_agent_pool_destroy(srch_cntx_t *ctx);
static int srch_creat_worker_pool(srch_cntx_t *ctx);
static int srch_worker_pool_destroy(srch_cntx_t *ctx);
static int srch_creat_queue(srch_cntx_t *ctx);

/******************************************************************************
 **函数名称: srch_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 解析输入参数
 **     2. 验证输入参数
 **注意事项: 
 **     c: 配置文件路径
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int srch_getopt(int argc, char **argv, srch_opt_t *opt)
{
    int ch;

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
                return SRCH_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", SRCH_DEF_CONF_PATH);
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     name: 程序名
 **输出参数: NULL
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int srch_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -c <config file> [-l log_level]\n", exec);
    printf("\t-h\tShow help\n"
           "\t-c\tConfiguration path\n\n");
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_init_log
 **功    能: 初始化日志模块
 **输入参数:
 **     fname: 日志文件名
 **输出参数: NONE
 **返    回: 获取域名对应的地址信息
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static log_cycle_t *srch_init_log(char *fname)
{
    log_cycle_t *log;
    char path[FILE_NAME_MAX_LEN];

    /* 1. 初始化系统日志 */
    syslog_get_path(path, sizeof(path), basename(fname));

    if (syslog_init(LOG_LEVEL_ERROR, path))
    {
        fprintf(stderr, "Init syslog failed!");
        return NULL;
    }

    /* 2. 初始化业务日志 */
    log_get_path(path, sizeof(path), basename(fname));

    log = log_init(LOG_LEVEL_ERROR, path);
    if (NULL == log)
    {
        sys_error("Initialize log failed!");
        syslog_destroy();
        return NULL;
    }

    return log;
}

/******************************************************************************
 **函数名称: srch_cntx_init
 **功    能: 初始化全局信息
 **输入参数: 
 **     pname: 进程名
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
srch_cntx_t *srch_cntx_init(char *pname, const char *conf_path)
{
    void *addr;
    log_cycle_t *log;
    srch_cntx_t *ctx;
    srch_conf_t *conf;

    /* 1. 初始化日志模块 */
    log = srch_init_log(pname);
    if (NULL == log)
    {
        fprintf(stderr, "Initialize log failed!");
        return NULL;
    }

    /* 2. 判断程序是否已运行 */
    if (0 != srch_proc_lock())
    {
        log_error(log, "Search-engine is running!");
        return NULL;
    }

    /* 3. 创建全局对象 */
    ctx = (srch_cntx_t *)calloc(1, sizeof(srch_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 4. 加载配置文件 */
    conf = srch_conf_load(conf_path, log);
    if (NULL == conf)
    {
        free(ctx);
        log_error(log, "Load configuration failed! path:%s", conf_path);
        return NULL;
    }

    ctx->log = log;
    ctx->conf = conf;
    log_set_level(log, conf->log.level);
    syslog_set_level(conf->log.syslevel);

    /* 5. 创建内存池 */
    addr = (void *)calloc(1, 30 * MB);
    if (NULL == addr)
    {
        free(ctx);
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->slab = slab_init(addr, 30 * MB);
    if (NULL == ctx->slab)
    {
        free(ctx);
        log_error(log, "Init slab failed!");
        return NULL;
    }

    do
    {
        /* > 注册消息处理 */
        if (srch_init_register(ctx))
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
        if (srch_creat_queue(ctx))
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建Worker线程池 */
        if (srch_creat_worker_pool(ctx))
        {
            log_error(log, "Initialize worker thread pool failed!");
            break;
        }

        /* > 创建Agent线程池 */
        if (srch_creat_agent_pool(ctx))
        {
            log_error(log, "Initialize agent thread pool failed!");
            break;
        }

        return ctx;
    } while (0);

    srch_conf_destroy(ctx->conf);
    free(ctx);
    return NULL;
}

/******************************************************************************
 **函数名称: srch_cntx_destroy
 **功    能: 销毁搜索引擎上下文
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     依次销毁侦听线程、接收线程、工作线程、日志对象等
 **注意事项: 按序销毁
 **作    者: # Qifeng.zou # 2014.11.17 #
 ******************************************************************************/
void srch_cntx_destroy(srch_cntx_t *ctx)
{
    pthread_cancel(ctx->lsn_tid);
    srch_worker_pool_destroy(ctx);
    srch_agent_pool_destroy(ctx);

    log_destroy(&ctx->log);
    syslog_destroy();
}

/******************************************************************************
 **函数名称: srch_startup
 **功    能: 启动搜索引擎服务
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     设置线程回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int srch_startup(srch_cntx_t *ctx)
{
    int idx;
    const srch_conf_t *conf = ctx->conf;


    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        thread_pool_add_worker(ctx->worker_pool, srch_worker_routine, ctx);
    }

    /* 2. 设置Agent线程回调 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        thread_pool_add_worker(ctx->agent_pool, srch_agent_routine, ctx);
    }
    
    /* 3. 设置Listen线程回调 */
    if (thread_creat(&ctx->lsn_tid, srch_listen_routine, ctx))
    {
        log_error(ctx->log, "Create listen thread failed!");
        return SRCH_ERR;
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_creat_worker_pool
 **功    能: 创建Worker线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int srch_creat_worker_pool(srch_cntx_t *ctx)
{
    int idx, num;
    srch_worker_t *worker;
    thread_pool_option_t option;
    const srch_conf_t *conf = ctx->conf;

    /* > 新建Worker对象 */
    worker = (srch_worker_t *)calloc(conf->worker_num, sizeof(srch_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SRCH_ERR;
    }

    /* > 创建Worker线程池 */
    memset(&option, 0, sizeof(option));

    option.pool = ctx->slab;
    option.alloc = (mem_alloc_cb_t)slab_alloc;
    option.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->worker_pool = thread_pool_init(conf->worker_num, &option, worker);
    if (NULL == ctx->worker_pool)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        return SRCH_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        if (srch_worker_init(ctx, worker+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->worker_num)
    {
        return SRCH_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        srch_worker_destroy(worker+idx);
    }

    free(worker);
    thread_pool_destroy(ctx->worker_pool);

    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_worker_pool_destroy
 **功    能: 销毁爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
static int srch_worker_pool_destroy(srch_cntx_t *ctx)
{
    int idx;
    void *data;
    srch_worker_t *worker;
    const srch_conf_t *conf = ctx->conf;

    /* 1. 释放Worker对象 */
    for (idx=0; idx<conf->worker_num; ++idx)
    {
        worker = (srch_worker_t *)ctx->worker_pool->data + idx;

        srch_worker_destroy(worker);
    }

    /* 2. 释放线程池对象 */
    data = ctx->worker_pool->data;

    thread_pool_destroy(ctx->worker_pool);

    free(data);

    ctx->worker_pool = NULL;

    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_creat_agent_pool
 **功    能: 创建Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int srch_creat_agent_pool(srch_cntx_t *ctx)
{
    int idx, num;
    srch_agent_t *agent;
    thread_pool_option_t option;
    const srch_conf_t *conf = ctx->conf;

    /* > 新建Agent对象 */
    agent = (srch_agent_t *)calloc(conf->agent_num, sizeof(srch_agent_t));
    if (NULL == agent)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SRCH_ERR;
    }

    /* > 创建Worker线程池 */
    memset(&option, 0, sizeof(option));

    option.pool = ctx->slab;
    option.alloc = (mem_alloc_cb_t)slab_alloc;
    option.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->agent_pool = thread_pool_init(conf->agent_num, &option, agent);
    if (NULL == ctx->agent_pool)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        free(agent);
        return SRCH_ERR;
    }

    /* 3. 依次初始化Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        if (srch_agent_init(ctx, agent+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->agent_num)
    {
        return SRCH_OK; /* 成功 */
    }

    /* 4. 释放Agent对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        srch_agent_destroy(agent+idx);
    }

    free(agent);
    thread_pool_destroy(ctx->agent_pool);

    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_agent_pool_destroy
 **功    能: 销毁Agent线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int srch_agent_pool_destroy(srch_cntx_t *ctx)
{
    int idx;
    void *data;
    srch_agent_t *agent;
    const srch_conf_t *conf = ctx->conf;

    /* 1. 释放Agent对象 */
    for (idx=0; idx<conf->agent_num; ++idx)
    {
        agent = (srch_agent_t *)ctx->agent_pool->data + idx;

        srch_agent_destroy(agent);
    }

    /* 2. 释放线程池对象 */
    data = ctx->agent_pool->data;

    thread_pool_destroy(ctx->agent_pool);

    free(data);

    ctx->agent_pool = NULL;

    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_proc_lock
 **功    能: 搜索引擎进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int srch_proc_lock(void)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s", SRCH_PROC_LOCK_PATH);

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
 **函数名称: srch_reg_def_hdl
 **功    能: 默认注册函数
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
static int srch_reg_def_hdl(uint8_t type, char *buff, size_t len, void *args, log_cycle_t *log)
{
    static int total = 0;

    log_info(log, "Call: %s()! total:%d", __func__, ++total);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_init_register
 **功    能: 初始化注册消息处理
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.20 #
 ******************************************************************************/
int srch_init_register(srch_cntx_t *ctx)
{
    uint32_t idx;
    srch_reg_t *reg;

    for (idx=0; idx<=SRCH_MSG_TYPE_MAX; ++idx)
    {
        reg = &ctx->reg[idx];

        reg->type = idx;
        reg->proc = srch_reg_def_hdl;
        reg->args = NULL;
        reg->flag = SRCH_REG_FLAG_UNREG;
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_register
 **功    能: 注册消息处理函数
 **输入参数:
 **     ctx: 全局信息
 **     type: 扩展消息类型. Range:(0 ~ SRCH_MSG_TYPE_MAX)
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
int srch_register(srch_cntx_t *ctx, uint32_t type, srch_reg_cb_t proc, void *args)
{
    srch_reg_t *reg;

    if (type >= SRCH_MSG_TYPE_MAX
        || 0 != ctx->reg[type].flag)
    {
        log_error(ctx->log, "Type 0x%02X is invalid or repeat reg!", type);
        return SRCH_ERR;
    }

    reg = &ctx->reg[type];
    reg->type = type;
    reg->proc = proc;
    reg->args = args;
    reg->flag = 1;

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_creat_queue
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
static int srch_creat_queue(srch_cntx_t *ctx)
{
    int idx;
    const srch_conf_t *conf = ctx->conf;

    /* 1. 创建连接队列(与Agent数一致) */
    ctx->connq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->connq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SRCH_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->connq[idx] = queue_creat(conf->connq.max, sizeof(srch_add_sck_t));
        if (NULL == ctx->connq)
        {
            log_error(ctx->log, "Initialize lock queue failed!");
            return SRCH_ERR;
        }
    }

    /* 2. 创建接收队列(与Agent数一致) */
    ctx->recvq = (queue_t **)calloc(conf->agent_num, sizeof(queue_t*));
    if (NULL == ctx->recvq)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SRCH_ERR;
    }

    for (idx=0; idx<conf->agent_num; ++idx)
    {
        ctx->recvq[idx] = queue_creat(conf->taskq.max, conf->taskq.size);
        if (NULL == ctx->recvq)
        {
            log_error(ctx->log, "Initialize lock queue failed!");
            return SRCH_ERR;
        }
    }

    return SRCH_OK;
}

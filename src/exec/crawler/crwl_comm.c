/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_comm.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#include "log.h"
#include "sck.h"
#include "lock.h"
#include "hash.h"
#include "comm.h"
#include "crawler.h"
#include "syscall.h"
#include "crwl_man.h"
#include "crwl_sched.h"
#include "crwl_worker.h"

#define CRWL_PROC_LOCK_PATH "../temp/crwl/crwl.lck"

static int crwl_worker_tpool_creat(crwl_cntx_t *ctx);
int crwl_worker_tpool_destroy(crwl_cntx_t *ctx);
static int crwl_sched_tpool_creat(crwl_cntx_t *ctx);
static void crwl_signal_hdl(int signum);

/******************************************************************************
 **函数名称: crwl_getopt 
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
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
int crwl_getopt(int argc, char **argv, crwl_opt_t *opt)
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
                return CRWL_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", CRWL_DEF_CONF_PATH);
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     name: 程序名
 **输出参数: NULL
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
int crwl_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -c <config file> [-l log_level]\n", exec);
    printf("\t-h\tShow help\n"
           "\t-c\tConfiguration path\n\n");
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_cntx_init
 **功    能: 初始化全局信息
 **输入参数: 
 **     pname: 进程名
 **     path: 配置文件路径
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
crwl_cntx_t *crwl_cntx_init(char *pname, const char *path)
{
    int idx;
    log_cycle_t *log;
    crwl_cntx_t *ctx;
    crwl_conf_t *conf;

    /* 1. 初始化日志模块 */
    log = crwl_init_log(pname);
    if (NULL == log)
    {
        fprintf(stderr, "Initialize log failed!");
        return NULL;
    }

    /* 2. 判断程序是否已运行 */
    if (0 != crwl_proc_lock())
    {
        log_error(log, "Crawler is running!");
        return NULL;
    }

    /* 3. 创建全局对象 */
    ctx = (crwl_cntx_t *)calloc(1, sizeof(crwl_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    do
    {
        /* 4. 加载配置文件 */
        conf = crwl_conf_load(path, log);
        if (NULL == conf)
        {
            log_error(log, "Load configuration failed! path:%s", path);
            break;
        }

        ctx->conf = conf;
        ctx->log = log;
        log_set_level(log, conf->log.level);
        plog_set_level(conf->log.syslevel);

        /* 5. 创建内存池 */
        ctx->slab = slab_creat_by_calloc(30 * MB);
        if (NULL == ctx->slab)
        {
            log_error(log, "Init slab failed!");
            break;
        }

        /* 6. 创建任务队列 */
        ctx->workq = (queue_t **)calloc(conf->worker.num, sizeof(queue_t *));
        if (NULL == ctx->workq)
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        for (idx=0; idx<conf->worker.num; ++idx)
        {
            ctx->workq[idx] = queue_creat(
                    conf->workq_count, sizeof(crwl_task_t) + sizeof(crwl_task_space_u));
            if (NULL == ctx->workq[idx])
            {
                log_error(ctx->log, "Create queue failed! workq_count:%d", conf->workq_count);
                goto CRWL_INIT_ERR;
            }
        }

        /* 8. 修改进程打开文件描述符的最大限制 */
        if (set_fd_limit(65535))
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* 9. 创建Worker线程池 */
        if (crwl_worker_tpool_creat(ctx))
        {
            log_error(log, "Initialize thread pool failed!");
            break;
        }

        /* 10. 创建Sched线程池 */
        if (crwl_sched_tpool_creat(ctx))
        {
            log_error(log, "Initialize thread pool failed!");
            break;
        }

        return ctx;
    } while (0);

CRWL_INIT_ERR:
    /* 释放内存 */
    if (ctx->conf) { crwl_conf_destroy(ctx->conf); }
    if (ctx->workq) { free(ctx->workq); }
    free(ctx);

    return NULL;
}

/******************************************************************************
 **函数名称: crwl_cntx_destroy
 **功    能: 销毁爬虫上下文
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     依次销毁线程池、日志对象等
 **注意事项: 按序销毁
 **作    者: # Qifeng.zou # 2014.11.17 #
 ******************************************************************************/
void crwl_cntx_destroy(crwl_cntx_t *ctx)
{
    int idx;
    crwl_conf_t *conf = ctx->conf;

    for (idx=0; idx<conf->worker.num; ++idx)
    {
        queue_destroy(ctx->workq[idx]);
    }
    FREE(ctx->workq);

    crwl_worker_tpool_destroy(ctx);
    log_destroy(&ctx->log);
    plog_destroy();
}

/******************************************************************************
 **函数名称: crwl_startup
 **功    能: 启动爬虫服务
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     设置线程回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_startup(crwl_cntx_t *ctx)
{
    int idx;
    pthread_t tid;
    const crwl_conf_t *conf = ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker.num; ++idx)
    {
        thread_pool_add_worker(ctx->worker_pool, crwl_worker_routine, ctx);
    }
    
    /* 2. 设置Sched线程回调 */
    for (idx=0; idx<CRWL_SCHED_THD_NUM; ++idx)
    {
        thread_pool_add_worker(ctx->scheds, crwl_sched_routine, ctx);
    }

    /* 3. 启动代理服务 */
    if (thread_creat(&tid, crwl_manager_routine, ctx))
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* 4. 获取运行时间 */
    ctx->run_tm = time(NULL);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_tpool_creat
 **功    能: 初始化爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
static int crwl_worker_tpool_creat(crwl_cntx_t *ctx)
{
    int idx, num;
    crwl_worker_t *worker;
    thread_pool_opt_t opt;
    const crwl_worker_conf_t *conf = &ctx->conf->worker;

    /* > 新建Worker对象 */
    worker = (crwl_worker_t *)calloc(conf->num, sizeof(crwl_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* > 创建Worker线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->worker_pool = thread_pool_init(conf->num, &opt, worker);
    if (NULL == ctx->worker_pool)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        free(worker);
        return CRWL_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->num; ++idx)
    {
        if (crwl_worker_init(ctx, worker+idx, idx))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->num)
    {
        return CRWL_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        crwl_worker_destroy(ctx, worker+idx);
    }

    free(worker);
    thread_pool_destroy(ctx->worker_pool);

    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_worker_tpool_destroy
 **功    能: 销毁爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
int crwl_worker_tpool_destroy(crwl_cntx_t *ctx)
{
    int idx;
    crwl_worker_t *worker;
    const crwl_worker_conf_t *conf = &ctx->conf->worker;

    /* 1. 释放Worker对象 */
    for (idx=0; idx<conf->num; ++idx)
    {
        worker = (crwl_worker_t *)ctx->worker_pool->data + idx;

        crwl_worker_destroy(ctx, worker);
    }

    free(ctx->worker_pool->data);

    /* 2. 释放线程池对象 */
    thread_pool_destroy(ctx->worker_pool);

    ctx->worker_pool = NULL;

    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_sched_tpool_creat
 **功    能: 初始化Sched线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.15 #
 ******************************************************************************/
static int crwl_sched_tpool_creat(crwl_cntx_t *ctx)
{
    thread_pool_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    /* 1. 设置内存池信息 */
    opt.pool = (void *)ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    /* 2. 创建Sched线程池 */
    ctx->scheds = thread_pool_init(CRWL_SCHED_THD_NUM, &opt, NULL);
    if (NULL == ctx->scheds)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_proc_lock
 **功    能: 爬虫进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.21 #
 ******************************************************************************/
int crwl_proc_lock(void)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s", CRWL_PROC_LOCK_PATH);

    Mkdir2(path, DIR_MODE);

    /* 2. 打开文件 */
    fd = Open(path, OPEN_FLAGS, OPEN_MODE);
    if (fd < 0)
    {
        return CRWL_ERR;
    }

    /* 3. 尝试加锁 */
    if (proc_try_wrlock(fd) < 0)
    {
        CLOSE(fd);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_domain_ip_map_cmp_cb
 **功    能: 域名IP映射表的比较
 **输入参数:
 **     _domain: 域名
 **     data: 域名IP映射表数据(crwl_domain_ip_map_t)
 **输出参数: NONE
 **返    回: 0:相等 <0:小于 >0:大于
 **实现描述: 
 **注意事项: 
 **     查找成功后，将会更新访问时间. 该时间将会是表数据更新的参考依据
 **作    者: # Qifeng.zou # 2014.11.14 #
 ******************************************************************************/
int crwl_domain_ip_map_cmp_cb(const char *domain, const crwl_domain_ip_map_t *map)
{
    return strcmp(domain, map->host);
}

/******************************************************************************
 **函数名称: crwl_domain_blacklist_cmp_cb
 **功    能: 域名黑名单的比较
 **输入参数:
 **     domain: 域名
 **     blacklist: 域名黑名单数据
 **输出参数: NONE
 **返    回: 0:相等 <0:小于 >0:大于
 **实现描述: 
 **注意事项: 
 **     查找成功后，将会更新访问时间. 该时间将会是表数据更新的参考依据
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
int crwl_domain_blacklist_cmp_cb(
        const char *domain, const crwl_domain_blacklist_t *blacklist)
{
    return strcmp(domain, blacklist->host);
}

/******************************************************************************
 **函数名称: crwl_init_log
 **功    能: 初始化日志模块
 **输入参数:
 **     fname: 日志文件名
 **输出参数: NONE
 **返    回: 日志对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.21 #
 ******************************************************************************/
log_cycle_t *crwl_init_log(char *fname)
{
    log_cycle_t *log;
    char path[FILE_NAME_MAX_LEN];

    /* 1. 初始化系统日志 */
    plog_get_path(path, sizeof(path), basename(fname));

    if (plog_init(LOG_LEVEL_ERROR, path))
    {
        fprintf(stderr, "Init syslog failed!");
        return NULL;
    }

    /* 2. 初始化业务日志 */
    log_get_path(path, sizeof(path), basename(fname));

    log = log_init(LOG_LEVEL_ERROR, path);
    if (NULL == log)
    {
        plog_error("Initialize log failed!");
        plog_destroy();
        return NULL;
    }

    return log;
}

/******************************************************************************
 **函数名称: crwl_set_signal
 **功    能: 设置信号处理
 **输入参数: NONE
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **     1) sa_handler: 此参数和signal()的参数handler相同, 代表新的信号处理函数,
 **         其他意义请参考signal().
 **     2) sa_mask: 用来设置在处理该信号时暂时将sa_mask指定的信号集搁置.
 **     3) sa_restorer: 此参数没有使用.
 **     4) sa_flags: 用来设置信号处理的其他相关操作, 下列的数值可用:
 **         SA_NOCLDSTOP: 对于SIGCHLD, 当子进程停止时（ctrl+z）不产生此信号, 当
 **             子进程终止时, 产生此信号
 **         SA_RESTART: 系统调用自动再启动
 **         SA_ONSTACK: ???
 **         SA_NOCLDWAIT: 当调用进程的子进程终止时, 不再创建僵尸进程.因此父进程
 **             将得不到SIGCHLD信号, 调用wait时, 将出错返回, errno：ECHLD
 **         SA_NODEFER: 系统在执行信号处理函数时, 不自动阻塞该信号
 **         SA_RESETHAND: 系统在执行信号处理函数时, 恢复该信号的默认处理方式：SIG_DEF
 **         SA_SIGINFO: 附加信息(较少使用)
 **作    者: # Qifeng.zou # 2014.11.22 #
 ******************************************************************************/
void crwl_set_signal(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));

    act.sa_handler = crwl_signal_hdl;
    sigemptyset(&act.sa_mask); /* 清空此信号集 */
    act.sa_flags = 0;

    sigaction(SIGPIPE, &act, NULL);
}

/******************************************************************************
 **函数名称: crwl_signal_hdl
 **功    能: 信号处理回调函数
 **输入参数:
 **     signum: 信号编号
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.22 #
 ******************************************************************************/
static void crwl_signal_hdl(int signum)
{
    switch (signum)
    {
        case SIGINT:
        {
            plog_error("Catch SIGINT [%d] signal!", signum);
            return;
        }
        case SIGPIPE:
        {
            plog_error("Catch SIGPIPE [%d] signal!", signum);
            return;
        }
        default:
        {
            plog_error("Catch unknown signal! signum:[%d]", signum);
            return;
        }
    }
}

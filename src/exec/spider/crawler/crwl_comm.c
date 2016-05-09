/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
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
#include "comm.h"
#include "redo.h"
#include "crawler.h"
#include "crwl_man.h"
#include "hash_alg.h"
#include "crwl_sched.h"
#include "crwl_worker.h"

#define CRWL_PROC_LOCK_PATH "../temp/crwl/crwl.lck"

static int crwl_creat_workq(crwl_cntx_t *ctx);
static int crwl_creat_workers(crwl_cntx_t *ctx);
int crwl_workers_destroy(crwl_cntx_t *ctx);
static int crwl_creat_scheds(crwl_cntx_t *ctx);
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
 **     c: 配置路径
 **     l: 日志级别(取值范围: trace|debug|info|warn|error|fatal)
 **     d: 后台运行
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
int crwl_getopt(int argc, char **argv, crwl_opt_t *opt)
{
    int ch;
    const struct option opts[] = {
        {"conf",            required_argument,  NULL, 'c'}
        , {"help",          no_argument,        NULL, 'h'}
        , {"daemon",        no_argument,        NULL, 'd'}
        , {"log level",     required_argument,  NULL, 'l'}
        , {NULL,            0,                  NULL, 0}
    };

    memset(opt, 0, sizeof(crwl_opt_t));

    opt->isdaemon = false;
    opt->log_level = LOG_LEVEL_TRACE;
    opt->conf_path = CRWL_DEF_CONF_PATH;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt_long(argc, argv, "c:l:dh", opts, NULL))) {
        switch (ch) {
            case 'c':   /* 指定配置文件 */
            {
                opt->conf_path = optarg;
                break;
            }
            case 'l':
            {
                opt->log_level = log_get_level(optarg);
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

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     exec: 程序名
 **输出参数: NULL
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
int crwl_usage(const char *exec)
{
    printf("\nUsage: %s -l <log level> -L <log key path> -c <config file> [-h] [-d]\n", exec);
    printf("\t-l: Log level\n"
            "\t-c: Configuration path\n"
            "\t-d: Run as daemon\n"
            "\t-h: Show help\n\n");
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_init
 **功    能: 爬虫初始化
 **输入参数:
 **     pname: 进程名
 **     path: 配置文件路径
 **     log_level: 日志级别
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
crwl_cntx_t *crwl_init(char *pname, crwl_opt_t *opt)
{
    log_cycle_t *log;
    crwl_cntx_t *ctx;
    crwl_conf_t *conf;

    /* > 初始化日志模块 */
    log = crwl_init_log(pname, opt->log_level);
    if (NULL == log) {
        fprintf(stderr, "Initialize log failed!");
        return NULL;
    }

    /* > 判断程序是否已运行 */
    if (0 != crwl_proc_lock()) {
        log_error(log, "Crawler is running!");
        return NULL;
    }

    /* > 创建全局对象 */
    ctx = (crwl_cntx_t *)calloc(1, sizeof(crwl_cntx_t));
    if (NULL == ctx) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    do {
        conf = &ctx->conf;

        /* > 加载配置文件 */
        if (crwl_load_conf(opt->conf_path, conf, log)) {
            log_error(log, "Load configuration failed! path:%s", opt->conf_path);
            break;
        }

        /* > 创建任务队列 */
        if (crwl_creat_workq(ctx)) {
            log_error(log, "Create workq failed!");
            break;
        }

        /* > 修改进程打开文件描述符的最大限制 */
        if (set_fd_limit(65535)) {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建Worker线程池 */
        if (crwl_creat_workers(ctx)) {
            log_error(log, "Initialize thread pool failed!");
            break;
        }

        /* > 创建Sched线程池 */
        if (crwl_creat_scheds(ctx)) {
            log_error(log, "Initialize thread pool failed!");
            break;
        }

        return ctx;
    } while (0);

    return NULL;
}

/******************************************************************************
 **函数名称: crwl_destroy
 **功    能: 销毁爬虫
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 依次销毁线程池、日志对象等
 **注意事项: 按序销毁
 **作    者: # Qifeng.zou # 2014.11.17 #
 ******************************************************************************/
void crwl_destroy(crwl_cntx_t *ctx)
{
    int idx;
    crwl_conf_t *conf = &ctx->conf;

    for (idx=0; idx<conf->worker.num; ++idx) {
        queue_destroy(ctx->workq[idx]);
    }
    FREE(ctx->workq);

    crwl_workers_destroy(ctx);
}

/******************************************************************************
 **函数名称: crwl_launch
 **功    能: 启动爬虫服务
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 设置线程回调
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_launch(crwl_cntx_t *ctx)
{
    int idx;
    pthread_t tid;
    const crwl_conf_t *conf = &ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker.num; ++idx) {
        thread_pool_add_worker(ctx->workers, crwl_worker_routine, ctx);
    }

    /* 2. 设置Sched线程回调 */
    for (idx=0; idx<CRWL_SCHED_THD_NUM; ++idx) {
        thread_pool_add_worker(ctx->scheds, crwl_sched_routine, ctx);
    }

    /* 3. 启动代理服务 */
    if (thread_creat(&tid, crwl_manager_routine, ctx)) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* 4. 获取运行时间 */
    ctx->run_tm = time(NULL);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_creat_workers
 **功    能: 初始化爬虫线程池
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
static int crwl_creat_workers(crwl_cntx_t *ctx)
{
    int idx, num;
    crwl_worker_t *worker;
    const crwl_worker_conf_t *conf = &ctx->conf.worker;

    /* > 新建Worker对象 */
    worker = (crwl_worker_t *)calloc(1, conf->num*sizeof(crwl_worker_t));
    if (NULL == worker) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* > 创建Worker线程池 */
    ctx->workers = thread_pool_init(conf->num, NULL, worker);
    if (NULL == ctx->workers) {
        log_error(ctx->log, "Initialize thread pool failed!");
        FREE(worker);
        return CRWL_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->num; ++idx) {
        if (crwl_worker_init(ctx, worker+idx, idx)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->num) {
        return CRWL_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx) {
        crwl_worker_destroy(ctx, worker+idx);
    }

    FREE(worker);
    thread_pool_destroy(ctx->workers);

    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_workers_destroy
 **功    能: 销毁爬虫线程池
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
int crwl_workers_destroy(crwl_cntx_t *ctx)
{
    int idx;
    crwl_worker_t *worker;
    const crwl_worker_conf_t *conf = &ctx->conf.worker;

    /* 1. 释放Worker对象 */
    for (idx=0; idx<conf->num; ++idx) {
        worker = (crwl_worker_t *)ctx->workers->data + idx;
        crwl_worker_destroy(ctx, worker);
    }

    FREE(ctx->workers->data);

    /* 2. 释放线程池对象 */
    thread_pool_destroy(ctx->workers);

    ctx->workers = NULL;

    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_creat_scheds
 **功    能: 初始化Sched线程池
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.15 #
 ******************************************************************************/
static int crwl_creat_scheds(crwl_cntx_t *ctx)
{
    /* 创建Sched线程池 */
    ctx->scheds = thread_pool_init(CRWL_SCHED_THD_NUM, NULL, NULL);
    if (NULL == ctx->scheds) {
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
    if (fd < 0) {
        return CRWL_ERR;
    }

    /* 3. 尝试加锁 */
    if (proc_try_wrlock(fd) < 0) {
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
 **注意事项: 查找成功后，将会更新访问时间. 该时间将会是表数据更新的参考依据
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
 **     log_level: 日志级别
 **     log_key_path: 日志键值路径
 **输出参数: NONE
 **返    回: 日志对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.21 #
 ******************************************************************************/
log_cycle_t *crwl_init_log(char *fname, int log_level)
{
    char path[FILE_NAME_MAX_LEN];

    log_get_path(path, sizeof(path), basename(fname));

    return log_init(log_level, path);
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
    switch (signum) {
        case SIGINT:
        {
            fprintf(stderr, "Catch SIGINT [%d] signal!", signum);
            return;
        }
        case SIGPIPE:
        {
            fprintf(stderr, "Catch SIGPIPE [%d] signal!", signum);
            return;
        }
        default:
        {
            fprintf(stderr, "Catch unknown signal! signum:[%d]", signum);
            return;
        }
    }
}

/******************************************************************************
 **函数名称: crwl_creat_workq
 **功    能: 创建工作队列
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015-07-03 12:02:23 #
 ******************************************************************************/
static int crwl_creat_workq(crwl_cntx_t *ctx)
{
    int size, idx, num;
    crwl_conf_t *conf = &ctx->conf;

    /* > 申请内存空间 */
    ctx->workq = (queue_t **)calloc(1, conf->worker.num*sizeof(queue_t *));
    if (NULL == ctx->workq) {
        log_error(ctx->log, "Alloc memory failed!");
        return CRWL_ERR;
    }

    /* > 依次创建队列 */
    size = sizeof(crwl_task_t) + sizeof(crwl_task_space_u);
    for (idx=0, num=0; idx<conf->worker.num; ++idx) {
        ctx->workq[idx] = queue_creat(conf->workq_count, size);
        if (NULL == ctx->workq[idx]) {
            FREE(ctx->workq);
            goto CREAT_QUEUE_ERR;
        }
    }

    return CRWL_OK;

CREAT_QUEUE_ERR:
    num = idx;
    for (idx=0; idx<num; ++idx) {
        queue_destroy(ctx->workq[idx]);
    }
    return CRWL_ERR;
}

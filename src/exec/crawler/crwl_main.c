/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>

#include "log.h"
#include "common.h"
#include "crawler.h"
#include "crwl_sched.h"
#include "crwl_worker.h"

#define CRWL_LOG2_LEVEL  "error"        /* 日志级别 */

/* 程序输入参数信息 */
typedef struct
{
    bool is_daemon;                     /* 是否后台运行 */
    char conf_path[FILE_NAME_MAX_LEN];  /* 配置文件路径 */
    int log_level;                      /* 指定日志级别 */
}crwl_opt_t;

static int crwl_getopt(int argc, char **argv, crwl_opt_t *opt);
static int crwl_usage(const char *exec);
static int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log);

static int crwl_init_workers(crwl_cntx_t *ctx);
int crwl_workers_destroy(crwl_cntx_t *ctx);

/******************************************************************************
 **函数名称: main 
 **功    能: 爬虫主程序
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载爬虫配置
 **     2. 启动爬虫功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int ret;
    crwl_opt_t opt;
    crwl_cntx_t *ctx;
    log_cycle_t *log;
    crwl_conf_t conf;
    char log_path[FILE_NAME_MAX_LEN];

    memset(&opt, 0, sizeof(opt));
    memset(&conf, 0, sizeof(conf));

    /* 1. 解析输入参数 */
    ret = crwl_getopt(argc, argv, &opt);
    if (CRWL_OK != ret)
    {
        return crwl_usage(argv[0]);
    }

    if (opt.is_daemon)
    {
        daemon(1, 0);
    }

    /* 2. 初始化日志模块 */
    log2_get_path(log_path, sizeof(log_path), basename(argv[0]));

    ret = log2_init(CRWL_LOG2_LEVEL, log_path);
    if (0 != ret)
    {
        fprintf(stderr, "Init log2 failed! level:%s path:%s\n",
                CRWL_LOG2_LEVEL, log_path);
        goto ERROR;
    }

    log_get_path(log_path, sizeof(log_path), basename(argv[0]));

    log = log_init(opt.log_level, log_path);
    if (NULL == log)
    {
        log2_error("Init log failed!");
        goto ERROR;
    }

    /* 3. 加载配置文件 */
    ret = crwl_load_conf(&conf, opt.conf_path, log);
    if (CRWL_OK != ret)
    {
        log2_error("Load crawler configuration failed!");
        goto ERROR;
    }

    /* 4. 初始化爬虫信息 */
    ctx = crwl_cntx_init(&conf, log);
    if (NULL == ctx)
    {
        log2_error("Start crawler server failed!");
        goto ERROR;
    }

    /* 5. 启动爬虫服务 */
    ret = crwl_cntx_startup(ctx);
    if (CRWL_OK != ret)
    {
        log2_error("Startup crawler server failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    log2_destroy();

    return CRWL_ERR;
}

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
 **     l: 日志级别
 **     d: 后台运行
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static int crwl_getopt(int argc, char **argv, crwl_opt_t *opt)
{
    int ch;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt(argc, argv, "c:l:dh")))
    {
        switch (ch)
        {
            case 'd':   /* 是否后台运行 */
            case 'D':
            {
                opt->is_daemon = true;
                break;
            }
            case 'c':   /* 指定配置文件 */
            case 'C':
            {
                snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", optarg);
                break;
            }
            case 'l':   /* 指定日志级别 */
            case 'L':
            {
                opt->log_level = log_get_level(optarg);
                break;
            }
            case 'h':   /* 显示帮助信息 */
            case 'H':
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
        snprintf(opt->conf_path, sizeof(opt->conf_path), "../conf/search.xml");
    }

    if (!opt->log_level)
    {
        opt->log_level = log_get_level(LOG_DEF_LEVEL_STR);
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
static int crwl_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -c <config file> [-l log_level]\n", exec);
    printf("\t-h\tShow help\n"
           "\t-d\tRun as daemon\n"
           "\t-l\tSet log level. [trace|debug|info|warn|error|fatal]\n"
           "\t-c\tConfiguration path\n\n");
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_load_conf
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
static int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log)
{
    int ret;

    /* 1. 加载Worker配置 */
    ret = crwl_worker_load_conf(&conf->worker, path, log);
    if (CRWL_OK != ret)
    {
        log_error(log, "Load worker configuration failed! path:%s", path);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_cntx_init
 **功    能: 初始化全局信息
 **输入参数: 
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
crwl_cntx_t *crwl_cntx_init(const crwl_conf_t *conf, log_cycle_t *log)
{
    int ret;
    crwl_cntx_t *ctx;

    /* 1. 创建全局对象 */
    ctx = (crwl_cntx_t *)calloc(1, sizeof(crwl_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    memcpy(&ctx->conf, conf, sizeof(crwl_conf_t));

    /* 2. 新建SLAB机制 */
    ret = crwl_slab_init(ctx);
    if (CRWL_OK != ret)
    {
        free(ctx);
        log_error(log, "Initialize slab failed!");
        return NULL;
    }

    /* 3. 创建Worker线程池 */
    ret = crwl_init_workers(ctx);
    if (CRWL_OK != ret)
    {
        crwl_slab_destroy(ctx);
        free(ctx);
        log_error(log, "Initialize thread pool failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: crwl_cntx_startup
 **功    能: 启动爬虫服务
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_cntx_startup(crwl_cntx_t *ctx)
{
    int ret, idx;
    pthread_t tid;
    const crwl_conf_t *conf = &ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker.thread_num; ++idx)
    {
        thread_pool_add_worker(ctx->workers, crwl_worker_routine, ctx);
    }
    
    /* 2. 设置Sched线程回调 */
    ret = thread_creat(&tid, crwl_sched_routine, ctx);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Create thread failed!");
        return CRWL_OK;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_slab_init
 **功    能: 初始化全局SLAB
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
int crwl_slab_init(crwl_cntx_t *ctx)
{
    int ret;

    pthread_rwlock_init(&ctx->slab_lock, NULL);

    ret = eslab_init(&ctx->slab, 1 * MB);
    if (0 != ret)
    {
        pthread_rwlock_destroy(&ctx->slab_lock);

        log_error(ctx->log, "Initialize slab failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_slab_alloc
 **功    能: 从全局SLAB申请空间
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void *crwl_slab_alloc(crwl_cntx_t *ctx, int size)
{
    void *addr;

    pthread_rwlock_wrlock(&ctx->slab_lock);
    addr = eslab_alloc(&ctx->slab, size); 
    pthread_rwlock_unlock(&ctx->slab_lock);

    return addr;
}

/******************************************************************************
 **函数名称: crwl_slab_free
 **功    能: 全局SLAB回收空间
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void crwl_slab_dealloc(crwl_cntx_t *ctx, void *p)
{
    pthread_rwlock_wrlock(&ctx->slab_lock);
    eslab_dealloc(&ctx->slab, p); 
    pthread_rwlock_unlock(&ctx->slab_lock);
}

/******************************************************************************
 **函数名称: crwl_slab_destroy
 **功    能: 销毁全局SLAB
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void crwl_slab_destroy(crwl_cntx_t *ctx)
{
    eslab_destroy(&ctx->slab);
    pthread_rwlock_destroy(&ctx->slab_lock);
}

/******************************************************************************
 **函数名称: crwl_init_workers
 **功    能: 初始化爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
static int crwl_init_workers(crwl_cntx_t *ctx)
{
    int idx, ret, num;
    crwl_worker_t *worker;
    const crwl_worker_conf_t *conf = &ctx->conf.worker;

    /* 1. 创建Worker线程池 */
    ctx->workers = thread_pool_init(conf->thread_num);
    if (NULL == ctx->workers)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        return CRWL_ERR;
    }

    /* 2. 新建Worker对象 */
    ctx->workers->data =
        (crwl_worker_t *)calloc(conf->thread_num, sizeof(crwl_worker_t));
    if (NULL == ctx->workers->data)
    {
        thread_pool_destroy(ctx->workers);
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->thread_num; ++idx)
    {
        worker = (crwl_worker_t *)ctx->workers->data + idx;

        ret = crwl_worker_init(ctx, worker);
        if (CRWL_OK != ret)
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    if (idx == conf->thread_num)
    {
        return CRWL_OK; /* 成功 */
    }

    /* 4. 释放Worker对象 */
    num = idx;
    for (idx=0; idx<num; ++idx)
    {
        worker = (crwl_worker_t *)ctx->workers->data + idx;

        crwl_worker_destroy(worker);
    }

    free(ctx->workers->data);
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
    for (idx=0; idx<conf->thread_num; ++idx)
    {
        worker = (crwl_worker_t *)ctx->workers->data + idx;

        crwl_worker_destroy(worker);
    }

    free(ctx->workers->data);

    /* 2. 释放线程池对象 */
    thread_pool_destroy(ctx->workers);

    ctx->workers = NULL;

    return CRWL_ERR;
}

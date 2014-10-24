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
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>


#include "log.h"
#include "lock.h"
#include "hash.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "xd_socket.h"
#include "crwl_sched.h"
#include "crwl_worker.h"
#include "crwl_parser.h"

#define CRWL_PROC_LOCK_PATH "../temp/crwl/crwl.lck"

/* 程序输入参数信息 */
typedef struct
{
    bool is_daemon;                     /* 是否后台运行 */
    int log_level;                      /* 日志级别 */
    char conf_path[FILE_NAME_MAX_LEN];  /* 配置文件路径 */
}crwl_opt_t;

static int crwl_getopt(int argc, char **argv, crwl_opt_t *opt);
static int crwl_usage(const char *exec);
static int crwl_proc_lock(void);
static int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log);

static int crwl_init_workers(crwl_cntx_t *ctx);
int crwl_workers_destroy(crwl_cntx_t *ctx);

static int crwl_parse_comm_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log);

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
 **     1. 初始化爬虫信息
 **     2. 启动爬虫功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int ret, status;
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

    ret = log2_init(opt.log_level, log_path);
    if (0 != ret)
    {
        fprintf(stderr, "Init log2 failed! level:%d path:%s\n",
                opt.log_level, log_path);
        goto ERROR;
    }

    log_get_path(log_path, sizeof(log_path), basename(argv[0]));

    log = log_init(opt.log_level, log_path);
    if (NULL == log)
    {
        log2_error("Initialize log failed!");
        goto ERROR;
    }

    /* 3. 加载配置文件 */
    ret = crwl_load_conf(&conf, opt.conf_path, log);
    if (CRWL_OK != ret)
    {
        log_error(log, "Load configuration failed! path:%s", opt.conf_path);
        goto ERROR;
    }

    /* 4. 初始化爬虫信息 */
    ctx = crwl_cntx_init(&conf, log);
    if (NULL == ctx)
    {
        log_error(log, "Initialize crawler failed!");
        goto ERROR;
    }

    /* 5. 启动爬虫服务 */
    ret = crwl_cntx_startup(ctx);
    if (CRWL_OK != ret)
    {
        log_error(log, "Startup crawler failed!");
        goto ERROR;
    }

    while (1)
    {
        wait(&status);
        pause();
    }

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
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", CRWL_DEF_CONF_PATH);
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
    xml_tree_t *xml;

    /* 1. 加载爬虫配置 */
    xml = xml_creat(path);
    if (NULL == xml)
    {
        log_error(log, "Create xml failed! path:%s", path);
        return CRWL_ERR;
    }

    /* 2. 加载通用配置 */
    ret = crwl_parse_comm_conf(xml, conf, log);
    if (CRWL_OK != ret)
    {
        log_error(log, "Load common conf failed! path:%s", path);
        return CRWL_ERR;
    }

    /* 2. 提取爬虫配置 */
    ret = crwl_worker_parse_conf(xml, &conf->worker, log);
    if (0 != ret)
    {
        log_error(log, "Parse worker configuration failed! path:%s", path);
        xml_destroy(xml);
        return CRWL_ERR;
    }

    xml_destroy(xml);
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

    /* 1. 判断程序是否已运行 */
    if (0 != crwl_proc_lock())
    {
        log_error(log, "Crawler is running!");
        return NULL;
    }

    log_set_level(log, conf->log_level);
    log2_set_level(conf->log2_level);

    /* 1. 创建全局对象 */
    ctx = (crwl_cntx_t *)calloc(1, sizeof(crwl_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;
    memcpy(&ctx->conf, conf, sizeof(crwl_conf_t));

    /* 2. 新建域名表 */
    ctx->domain = hash_tab_init(10, hash_time33_ex, NULL);
    if (NULL == ctx->domain)
    {
        free(ctx);
        log_error(log, "Initialize hash table failed!");
        return NULL;
    }

    /* 3. 创建Worker线程池 */
    ret = crwl_init_workers(ctx);
    if (CRWL_OK != ret)
    {
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
    pid_t pid;
    int ret, idx;
    pthread_t tid;
    const crwl_conf_t *conf = &ctx->conf;

    /* 1. 设置Worker线程回调 */
    for (idx=0; idx<conf->worker.num; ++idx)
    {
        thread_pool_add_worker(ctx->workers, crwl_worker_routine, ctx);
    }
    
    /* 2. 设置Sched线程回调 */
    ret = thread_creat(&tid, crwl_sched_routine, ctx);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Create thread failed!");
        return CRWL_ERR;
    }

    /* 3. 启动Parser进程 */
    pid = fork();
    if (pid < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }
    else if (0 == pid)
    {
        crwl_parser_exec(conf);
        exit(0);
    }

    return CRWL_OK;
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
    ctx->workers = thread_pool_init(conf->num);
    if (NULL == ctx->workers)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        return CRWL_ERR;
    }

    /* 2. 新建Worker对象 */
    ctx->workers->data =
        (crwl_worker_t *)calloc(conf->num, sizeof(crwl_worker_t));
    if (NULL == ctx->workers->data)
    {
        thread_pool_destroy(ctx->workers);
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->num; ++idx)
    {
        worker = (crwl_worker_t *)ctx->workers->data + idx;

        worker->tidx = idx;

        ret = crwl_worker_init(ctx, worker);
        if (CRWL_OK != ret)
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
    for (idx=0; idx<conf->num; ++idx)
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

/******************************************************************************
 **函数名称: crwl_parse_comm_conf
 **功    能: 加载通用配置
 **输入参数: 
 **     xml: XML配置
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     通过XML查询接口查找对应的配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.16 #
 ******************************************************************************/
static int crwl_parse_comm_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* 1. 定位LOG标签
     *  获取日志级别信息
     * */
    fix = xml_search(xml, ".CRAWLER.COMMON.LOG");
    if (NULL != fix)
    {
        /* 1.1 日志级别 */
        node = xml_rsearch(xml, fix, "LOG_LEVEL");
        if (NULL != node)
        {
            conf->log_level = log_get_level(node->value);
        }

        /* 1.2 系统日志级别 */
        node = xml_rsearch(xml, fix, "LOG2_LEVEL");
        if (NULL != node)
        {
            conf->log2_level = log_get_level(node->value);
        }
    }
    else
    {
        log_warn(log, "Didn't configure log!");
    }

    /* 2. 定位Download标签
     *  获取网页抓取深度和存储路径
     * */
    fix = xml_search(xml, ".CRAWLER.COMMON.DOWNLOAD");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure download!");
        return CRWL_ERR;
    }

    /* 2.1 获取抓取深度 */
    node = xml_rsearch(xml, fix, "DEEP");
    if (NULL == node)
    {
        log_error(log, "Get download deep failed!");
        return CRWL_ERR;
    }

    conf->download.deep = atoi(node->value);

    /* 2.2 获取存储路径 */
    node = xml_rsearch(xml, fix, "PATH");
    if (NULL == node)
    {
        log_error(log, "Get download path failed!");
        return CRWL_ERR;
    }

    snprintf(conf->download.path, sizeof(conf->download.path), "%s", node->value);

    /* 3. 定位REDIS标签
     *  获取Redis的IP地址、端口号、队列等信息
     * */
    fix = xml_search(xml, ".CRAWLER.COMMON.REDIS");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure redis!");
        return CRWL_ERR;
    }

    /* 3.1 获取IP地址 */
    node = xml_rsearch(xml, fix, "IPADDR");
    if (NULL == node)
    {
        log_error(log, "Get redis ip address failed!");
        return CRWL_ERR;
    }

    snprintf(conf->redis.ipaddr, sizeof(conf->redis.ipaddr), "%s", node->value);

    /* 3.2 获取端口号 */
    node = xml_rsearch(xml, fix, "PORT");
    if (NULL == node)
    {
        log_error(log, "Get redis port failed!");
        return CRWL_ERR;
    }

    conf->redis.port = atoi(node->value);

    /* 3.3 获取队列名 */
    node = xml_rsearch(xml, fix, "QUEUE.UNDO_TASKQ");
    if (NULL == node)
    {
        log_error(log, "Get undo task queue failed!");
        return CRWL_ERR;
    }

    snprintf(conf->redis.undo_taskq,
            sizeof(conf->redis.undo_taskq), "%s", node->value);

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
static int crwl_proc_lock(void)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s", CRWL_PROC_LOCK_PATH);

    Mkdir2(path, DIR_MODE);

    /* 2. 打开文件 */
    fd = Open(path, OPEN_FLAGS, OPEN_MODE);
    if(fd < 0)
    {
        return -1;
    }

    /* 3. 尝试加锁 */
    if(proc_try_wrlock(fd) < 0)
    {
        Close(fd);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: crwl_get_ipaddr
 **功    能: 获取域名对应的IP地址
 **输入参数:
 **     ctx: 全局信息
 **     host: 域名
 **输出参数: NONE
 **返    回: 获取域名对应的地址信息
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.21 #
 ******************************************************************************/
crwl_domain_t *crwl_get_ipaddr(crwl_cntx_t *ctx, char *host)
{
    int ret;
    avl_unique_t unique;
    crwl_domain_t *domain;
    struct sockaddr_in *sockaddr;
    struct addrinfo *addrinfo, *curr;

    unique.data = host;
    unique.len = strlen(host);

CRWL_FETCH_DOMAIN:
    /* 1. 从域名表中查找IP地址 */
    domain = hash_tab_search(ctx->domain, &unique);
    if (NULL == domain)
    {
        /* 查找失败则通过getaddrinfo()查询IP地址 */
        if (0 != getaddrinfo(host, NULL, NULL, &addrinfo))
        {
            log_error(ctx->log, "Get address info failed! host:%s", host);
            return NULL;
        }

        /* 申请域名信息：此空间插入哈希表中(此处不释放空间) */
        domain = (crwl_domain_t *)calloc(1, sizeof(crwl_domain_t));
        if (NULL == domain)
        {
            freeaddrinfo(addrinfo);

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return NULL;
        }

        snprintf(domain->host, sizeof(domain->host), "%s", host);
        domain->ip_num = 0;

        curr = addrinfo;
        while (NULL != curr
            && domain->ip_num < CRWL_IP_MAX_NUM)
        {
            sockaddr = (struct sockaddr_in *)curr->ai_addr;

            inet_ntop(AF_INET,
                    &sockaddr->sin_addr.s_addr,
                    domain->ip[domain->ip_num],
                    sizeof(domain->ip[domain->ip_num]));
            ++domain->ip_num;

            curr = curr->ai_next;
        }

        freeaddrinfo(addrinfo);

        ret = hash_tab_insert(ctx->domain, &unique, domain);
        if (0 != ret)
        {
            free(domain);

            if (AVL_NODE_EXIST == ret)
            {
                log_debug(ctx->log, "Domain is exist! host:[%s]",
                        ret, AVL_NODE_EXIST, host);
                goto CRWL_FETCH_DOMAIN;
            }

            log_error(ctx->log, "Insert into hash table failed! ret:[%x/%x] host:[%s]",
                    ret, AVL_NODE_EXIST, host);
            return NULL;
        }
    }

    return domain;
}

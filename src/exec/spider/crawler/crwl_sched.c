/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: crwl_sched.c
 ** 版本号: 1.0
 ** 描  述: 爬虫任务分配
 **         负责将REDIS任务队列中的数据分发到不同的爬虫工作队列中
 ** 作  者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
#include "log.h"
#include "str.h"
#include "sck.h"
#include "comm.h"
#include "redo.h"
#include "redis.h"
#include "crawler.h"
#include "crwl_task.h"
#include "crwl_sched.h"

static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx);
static void crwl_sched_destroy(crwl_sched_t *sched);

static int crwl_sched_timeout_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_event_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_task(crwl_cntx_t *ctx, crwl_sched_t *sched);

static int crwl_task_parse(const char *str, crwl_task_t *task, log_cycle_t *log);
static int crwl_task_parse_download_webpage(xml_tree_t *xml, crwl_task_down_webpage_t *dw);
static int crwl_sched_task_hdl(crwl_cntx_t *ctx, queue_t *workq, crwl_task_t *task);

/******************************************************************************
 **函数名称: crwl_sched_routine
 **功    能: 运行任务调度线程
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void *crwl_sched_routine(void *_ctx)
{
    crwl_sched_t *sched;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;
    crwl_conf_t *conf = &ctx->conf;

    /* 1. 初始化调度器 */
    sched = crwl_sched_init(ctx);
    if (NULL == sched) {
        log_error(ctx->log, "Create schedule failed!");
        abort();
        return (void *)CRWL_ERR;
    }

    while (1) {
        Sleep(1);

        if (!conf->sched_stat) {
            continue;
        }

        crwl_sched_task(ctx, sched);
    }

    crwl_sched_destroy(sched);

    abort();
    return (void *)CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_sched_init
 **功    能: 初始化调度对象
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 调队对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx)
{
    struct timeval tv;
    crwl_sched_t *sched;
    crwl_conf_t *conf = &ctx->conf;

    /* 1. 创建调度器对象 */
    sched = (crwl_sched_t *)calloc(1, sizeof(crwl_sched_t));
    if (NULL == sched) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 连接Redis服务 */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    sched->redis = redisConnectWithTimeout(
        conf->redis.conf.ip, conf->redis.conf.port, tv);
    if (sched->redis->err) {
        redisFree(sched->redis);
        free(sched);
        log_error(ctx->log, "Connect redis failed! IP:[%s:%d]",
                conf->redis.conf.ip, conf->redis.conf.port);
        return NULL;
    }

    return sched;
}

/******************************************************************************
 **函数名称: crwl_sched_destroy
 **功    能: 销毁调度对象
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
static void crwl_sched_destroy(crwl_sched_t *sched)
{
    redisFree(sched->redis);
    sched->redis = NULL;
    free(sched);
}

/******************************************************************************
 **函数名称: crwl_sched_task
 **功    能: 从UNDO队列中取数据，并放入到Worker队列中
 **输入参数:
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 选空闲Worker队列
 **     2. 取Undo任务数据
 **     3. 新建crwl_task_t对象
 **     4. 解析Undo数据
 **     5. 处理Undo数据
 **注意事项: 从Undo Task队列中申请的内存将由Worker线程去释放
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_task(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int idx, size;
    void *addr;
    redisReply *r;
    queue_t *workq;
    crwl_task_t *task;
    crwl_conf_t *conf = &ctx->conf;

    size = sizeof(crwl_task_t) + sizeof(crwl_task_space_u);

    while (conf->sched_stat) {
        /* > 随机选择任务队列 */
        idx = rand() % conf->worker.num;

        workq = ctx->workq[idx];

        /* > 申请队列空间(head + body) */
        addr = queue_malloc(workq, size);
        if (NULL == addr) {
            log_warn(ctx->log, "Alloc from queue failed! num:%d size:%d/%d",
                    queue_space(workq), size, queue_size(workq));
            return CRWL_OK;
        }

        task = (crwl_task_t *)addr;

        /* > 取Undo任务数据 */
        r = redis_lpop(sched->redis, conf->redis.taskq);
        if (REDIS_REPLY_NIL == r->type) {
            freeReplyObject(r);
            queue_dealloc(workq, addr);
            return CRWL_OK;
        }

        /* > 解析Undo数据信息 */
        if (crwl_task_parse(r->str, task, ctx->log)) {
            log_error(ctx->log, "Parse task string failed! %s", r->str);
            freeReplyObject(r);
            queue_dealloc(workq, addr);
            return CRWL_ERR;
        }

        /* > 处理Undo任务 */
        if (crwl_sched_task_hdl(ctx, workq, task)) {
            log_error(ctx->log, "Handle undo task failed! %s", r->str);
            freeReplyObject(r);
            queue_dealloc(workq, addr);
            return CRWL_ERR;
        }

        freeReplyObject(r);
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_task_parse
 **功    能: 解析TASK字串
 **输入参数:
 **     str: TASK格式字串
 **输出参数:
 **     task: TASK信息(注意: 此字段为消息头+消息体格式)
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 字段task的地址指向"头+体"的内存首地址, 根据解析的数据类型，将报体
 **          放入仅接于该字段内存地址后面。
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
static int crwl_task_parse(const char *str, crwl_task_t *task, log_cycle_t *log)
{
    int ret;
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;
    mem_pool_t *pool;

    /* 1. 解析XML字串 */
    pool = mem_pool_creat(4 * KB);
    if (NULL == pool) {
        return CRWL_ERR;
    }

    memset(&opt, 0, sizeof(opt));

    opt.log = log;
    opt.pool = pool;
    opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    xml = xml_screat(str, -1, &opt);
    if (NULL == xml) {
        mem_pool_destroy(pool);
        return CRWL_ERR;
    }

    /* 2. 获取任务类型 */
    node = xml_query(xml, "TASK.TYPE");
    if (NULL == node) {
        xml_destroy(xml);
        mem_pool_destroy(pool);
        return CRWL_ERR;
    }

    task->type = atoi(node->value.str);
    switch(task->type) {
        case CRWL_TASK_DOWN_WEBPAGE:
        {
            task->length = sizeof(crwl_task_t) + sizeof(crwl_task_down_webpage_t);

            ret = crwl_task_parse_download_webpage(xml, (crwl_task_down_webpage_t *)(task + 1));
            break;
        }
        default:
        {
            ret = CRWL_ERR;
            break;
        }
    }

    xml_destroy(xml);
    mem_pool_destroy(pool);

    return ret;
}

/******************************************************************************
 **函数名称: crwl_task_parse_download_webpage
 **功    能: 解析TASK字串中DOWNLOAD WEBPAGE的配置
 **输入参数:
 **     xml: XML树
 **输出参数:
 **     dw: Download webpage的配置
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
static int crwl_task_parse_download_webpage(xml_tree_t *xml, crwl_task_down_webpage_t *dw)
{
    xml_node_t *node, *body;

    /* 定位BODY结点 */
    body = xml_query(xml, "TASK.BODY");
    if (NULL == body) {
        return CRWL_ERR;
    }

    /* 获取IP */
    node = xml_search(xml, body, "IP");
    if (NULL == node) {
        return CRWL_ERR;
    }

    snprintf(dw->ip, sizeof(dw->ip), "%s", node->value.str);

    /* 获取IP.FAMILY */
    node = xml_search(xml, body, "IP.FAMILY");
    if (NULL == node) {
        return CRWL_ERR;
    }

    dw->family = atoi(node->value.str);

    /* 获取URI */
    node = xml_search(xml, body, "URI");
    if (NULL == node) {
        return CRWL_ERR;
    }

    snprintf(dw->uri, sizeof(dw->uri), "%s", node->value.str);

    /* 获取URI.DEPTH */
    node = xml_search(xml, body, "URI.DEPTH");
    if (NULL == node) {
        return CRWL_ERR;
    }

    dw->depth = atoi(node->value.str);
    dw->port = CRWL_WEB_SVR_PORT;

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_download_webpage_task_hdl
 **功    能: 任务TASK_DOWN_WEBPAGE的处理
 **输入参数:
 **     ctx: 全局信息
 **     workq: 任务队列
 **     task: 任务信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 内存结构: crwl_task_t + crwl_task_down_webpage_t
 **作    者: # Qifeng.zou # 2014.12.12 #
 ******************************************************************************/
static int crwl_sched_download_webpage_task_hdl(
        crwl_cntx_t *ctx, queue_t *workq, crwl_task_t *task)
{
    return queue_push(workq, (void *)task);
}

/******************************************************************************
 **函数名称: crwl_sched_task_hdl
 **功    能: 处理Undo任务
 **输入参数:
 **     ctx: 全局信息
 **     workq: 任务队列
 **     task: 任务信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.12 #
 ******************************************************************************/
static int crwl_sched_task_hdl(crwl_cntx_t *ctx, queue_t *workq, crwl_task_t *task)
{
    switch (task->type) {
        case CRWL_TASK_DOWN_WEBPAGE:
        {
            return crwl_sched_download_webpage_task_hdl(ctx, workq, task);
        }
        default:
        {
            log_error(ctx->log, "Task type [%d] is unknown!", task->type);
            return CRWL_ERR;
        }
    }

    return CRWL_ERR;
}

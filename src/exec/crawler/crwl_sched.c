/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_sched.c
 ** 版本号: 1.0
 ** 描  述: 爬虫任务分配
 **         负责将REDIS任务队列中的数据分发到不同的爬虫工作队列中
 ** 作  者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
#include <stdint.h>

#include "log.h"
#include "str.h"
#include "redis.h"
#include "common.h"
#include "crawler.h"
#include "sck_api.h"
#include "syscall.h"
#include "crwl_task.h"
#include "crwl_sched.h"

static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx);
static void crwl_sched_destroy(crwl_sched_t *sched);

static int crwl_sched_timeout_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_event_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_fetch_task(crwl_cntx_t *ctx, crwl_sched_t *sched);

static int crwl_task_parse(const char *str, crwl_task_t *task);
static int crwl_task_parse_download_webpage(xml_tree_t *xml, crwl_task_down_webpage_t *dw);
static int crwl_sched_task_hdl(crwl_cntx_t *ctx, queue_t *taskq, crwl_task_t *task);

/******************************************************************************
 **函数名称: crwl_sched_routine
 **功    能: 运行任务调度线程
 **输入参数: 
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化调度器
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void *crwl_sched_routine(void *_ctx)
{
    int ret, max;
    struct timeval tv;
    crwl_sched_t *sched;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;

    /* 1. 初始化调度器 */
    sched = crwl_sched_init(ctx);
    if (NULL == sched)
    {
        log_error(ctx->log, "Create schedule failed!");
        abort();
        return (void *)CRWL_ERR;
    }

    while (1)
    {
        /* 2. 等待事件通知 */
        FD_ZERO(&sched->rdset);
        FD_ZERO(&sched->wrset);

        max = sched->cmd_sck_id;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        ret = select(max+1, &sched->rdset, &sched->wrset, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            break;
        }
        else if (0 == ret)
        {
            /* 超时处理 */
            crwl_sched_timeout_hdl(ctx, sched);
            continue;
        }

        /* 3. 进行事件处理 */
        crwl_sched_event_hdl(ctx, sched);
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
 **     1. 初始化调度器
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx)
{
    struct timeval tv;
    crwl_sched_t *sched;
    crwl_conf_t *conf = ctx->conf;

    /* 1. 创建调度器对象 */
    sched = (crwl_sched_t *)calloc(1, sizeof(crwl_sched_t));
    if (NULL == sched)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 连接Redis服务 */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    sched->redis = redisConnectWithTimeout(
            conf->redis.master.ip, conf->redis.master.port, tv);
    if (sched->redis->err)
    {
        redisFree(sched->redis);
        free(sched);
        log_error(ctx->log, "Connect redis failed! IP:[%s:%d]",
                conf->redis.master.ip, conf->redis.master.port);
        return NULL;
    }

    /* 3. 创建命令套接字 */
    sched->cmd_sck_id = -1;

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
    Close(sched->cmd_sck_id);
    free(sched);
}

/******************************************************************************
 **函数名称: crwl_sched_timeout_hdl
 **功    能: 超时处理
 **输入参数: 
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_timeout_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int ret;

    /* 1. 取Undo任务, 并放入Worker队列 */
    ret = crwl_sched_fetch_task(ctx, sched);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Fetch task failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_event_hdl
 **功    能: 时间处理
 **输入参数: 
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_event_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int ret;    

    ret = crwl_sched_fetch_task(ctx, sched);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Fetch task failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_fetch_task
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
 **注意事项: 
 **     从Undo Task队列中申请的内存将由Worker线程去释放
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_fetch_task(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int times, taskq_id;
    void *addr;
    redisReply *r;
    queue_t *taskq;
    crwl_task_t *task;
    crwl_conf_t *conf = ctx->conf;

    times = 0;
    while (1)
    {
        /* 1. 随机选择任务队列 */
        taskq_id = rand() % conf->worker.num;

        taskq = ctx->taskq[taskq_id];

        if (!queue_space(&taskq->queue))
        {
            ++times;
            if (times >= conf->worker.num)
            {
                times = 0;
                log_trace(ctx->log, "Undo task queue space isn't enough!");
                return CRWL_OK;
            }
            continue;
        }

        times = 0;

        /* 2. 取Undo任务数据 */
        r = redis_lpop(sched->redis, conf->redis.undo_taskq);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            return CRWL_OK;
        }

        log_trace(ctx->log, "TQ:%02d URL:%s!", taskq_id, r->str);

        /* 3. 新建crwl_task_t对象 */
        addr = queue_malloc(taskq);
        if (NULL == addr)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Alloc memory from queue failed!");
            return CRWL_OK;
        }

        task = (crwl_task_t *)addr;

        /* 4. 解析Undo数据信息 */
        if (crwl_task_parse(r->str, task))
        {
            log_error(ctx->log, "Parse task string failed! %s", r->str);

            freeReplyObject(r);
            queue_dealloc(taskq, addr);
            return CRWL_ERR;
        }

        /* 5. 处理Undo任务 */
        if (crwl_sched_task_hdl(ctx, taskq, task))
        {
            log_error(ctx->log, "Handle undo task failed! %s", r->str);

            freeReplyObject(r);
            queue_dealloc(taskq, addr);
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
 **注意事项: 
 **     字段task的地址指向"头+体"的内存首地址, 根据解析的数据类型，将报体放入仅
 **     接于该字段内存地址后面。
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
static int crwl_task_parse(const char *str, crwl_task_t *task)
{
    int ret;
    xml_tree_t *xml;
    xml_node_t *node;

    /* 1. 解析XML字串 */
    xml = xml_screat(str);
    if (NULL == xml)
    {
        return CRWL_ERR;
    }

    /* 2. 获取任务类型 */
    node = xml_query(xml, "TASK.TYPE");
    if (NULL == node)
    {
        xml_destroy(xml);
        return CRWL_ERR;
    }

    task->type = atoi(node->value);
    switch(task->type)
    {
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
static int crwl_task_parse_download_webpage(
        xml_tree_t *xml, crwl_task_down_webpage_t *dw)
{
    xml_node_t *node, *body;

    /* 定位BODY结点 */
    body = xml_query(xml, "TASK.BODY");
    if (NULL == body)
    {
        return CRWL_ERR;
    }

    /* 获取URI */
    node = xml_rquery(xml, body, "URI");
    if (NULL == node)
    {
        return CRWL_ERR;
    }

    snprintf(dw->uri, sizeof(dw->uri), "%s", node->value);

    /* 获取URI.DEPTH */
    node = xml_rquery(xml, body, "URI.DEPTH");
    if (NULL == node)
    {
        return CRWL_ERR;
    }

    dw->depth = atoi(node->value);
    dw->port = CRWL_WEB_SVR_PORT;

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_task_down_webpage_hdl
 **功    能: 任务TASK_DOWN_WEBPAGE的处理
 **输入参数: 
 **     ctx: 全局信息
 **     taskq: 任务队列
 **     task: 任务信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.12 #
 ******************************************************************************/
static int crwl_sched_task_down_webpage_hdl(
        crwl_cntx_t *ctx, queue_t *taskq, crwl_task_t *task)
{
    int ret, idx;
    uri_field_t field;
    crwl_domain_ip_map_t map;
    crwl_task_down_webpage_t *args = (crwl_task_down_webpage_t *)(task + 1);

    memset(&map, 0, sizeof(map));
    memset(&field, 0, sizeof(field));

    /* 1. 解析URI字串 */
    if(0 != uri_reslove(args->uri, &field))
    {
        log_error(ctx->log, "Reslove uri [%s] failed!", args->uri);
        return CRWL_ERR;
    }

    /* 2. 通过URL获取WEB服务器IP信息 */
    ret = crwl_get_domain_ip_map(ctx, field.host, &map);
    if (0 != ret || 0 == map.ip_num)
    {
        log_error(ctx->log, "Get ip failed! uri:%s host:%s", field.uri, field.host);
        return CRWL_ERR;
    }

    idx = random() % map.ip_num;

    args->family = map.ip[idx].family;
    snprintf(args->ip, sizeof(args->ip), "%s", map.ip[idx].ip);

    /* 3. 放入Worker任务队列 */
    if (queue_push(taskq, (void *)task))
    {
        log_error(ctx->log, "Push into worker queue failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_task_hdl
 **功    能: 处理Undo任务
 **输入参数: 
 **     ctx: 全局信息
 **     taskq: 任务队列
 **     task: 任务信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.12 #
 ******************************************************************************/
static int crwl_sched_task_hdl(crwl_cntx_t *ctx, queue_t *taskq, crwl_task_t *task)
{
    switch (task->type)
    {
        case CRWL_TASK_DOWN_WEBPAGE:
        {
            return crwl_sched_task_down_webpage_hdl(ctx, taskq, task);
        }
        default:
        {
            log_error(ctx->log, "Task type [%d] is unknown!", task->type);
            return CRWL_ERR;
        }
    }

    return CRWL_ERR;
}

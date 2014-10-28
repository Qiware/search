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
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "crwl_task.h"
#include "xd_socket.h"
#include "crwl_worker.h"
#include "crwl_sched.h"

static crwl_sched_t *crwl_sched_init(crwl_cntx_t *ctx);
static void crwl_sched_destroy(crwl_sched_t *sched);

static int crwl_sched_timeout_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_event_hdl(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_fetch_undo_task(crwl_cntx_t *ctx, crwl_sched_t *sched);
static int crwl_sched_push_undo_task(crwl_cntx_t *ctx, crwl_sched_t *sched);

static int crwl_task_parse(const char *str, crwl_task_t *task);
static int crwl_task_parse_download_webpage_by_uri(
        xml_tree_t *xml, crwl_task_down_webpage_by_uri_t *dw);

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
        pthread_exit((void *)-1);
        return (void *)CRWL_ERR;
    }

    while (1)
    {
        /* 2. 等待事件通知 */
        FD_ZERO(&sched->rdset);
        FD_ZERO(&sched->wrset);

        max = sched->cmd_sck_id;

        tv.tv_sec = 0;
        tv.tv_usec = 50000;

        ret = select(max+1, &sched->rdset, &sched->wrset, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
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
    int ret;
    struct timeval tv;
    crwl_sched_t *sched;
    crwl_conf_t *conf = &ctx->conf;

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
    sched->redis_ctx = redisConnectWithTimeout(conf->redis.ipaddr, conf->redis.port, tv);
    if (NULL == sched->redis_ctx)
    {
        free(sched);
        log_error(ctx->log, "Connect redis failed! IP:[%s:%d]",
                conf->redis.ipaddr, conf->redis.port);
        return NULL;
    }

    /* 3. 创建命令套接字 */
    sched->cmd_sck_id = -1;

    /* 4. 将种子插入队列 */
    ret = crwl_sched_push_undo_task(ctx, sched);
    if (CRWL_OK != ret)
    {
        redisFree(sched->redis_ctx);
        sched->redis_ctx = NULL;
        log_error(ctx->log, "Push task into undo queue failed!");
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
    redisFree(sched->redis_ctx);
    sched->redis_ctx = NULL;
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
    ret = crwl_sched_fetch_undo_task(ctx, sched);
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

    ret = crwl_sched_fetch_undo_task(ctx, sched);
    if (CRWL_OK != ret)
    {
        log_error(ctx->log, "Fetch task failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_fetch_undo_task
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
 **     4. 放入Worker任务队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_sched_fetch_undo_task(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int ret, times;
    void *addr;
    redisReply *r;
    crwl_worker_t *worker;
    crwl_conf_t *conf = &ctx->conf;

    crwl_task_t *task;
    size_t size = sizeof(crwl_task_t) + sizeof(crwl_task_space_u);

    worker = (crwl_worker_t *)ctx->workers->data;

    times = 0;
    while (1)
    {
        /* 1. 选空闲Worker队列 */
        ++sched->last_idx;
        sched->last_idx %= conf->worker.num;

        if (!crwl_worker_undo_taskq_space(worker + sched->last_idx))
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
        r = redisCommand(sched->redis_ctx, "LPOP %s", conf->redis.undo_taskq);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            return CRWL_OK;
        }

        log_trace(ctx->log, "[%02d] URL:%s!", sched->last_idx, r->str);

        /* 3. 新建crwl_task_t对象 */
        addr = lqueue_mem_alloc(&worker[sched->last_idx].undo_taskq, size);
        if (NULL == addr)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Alloc memory from slab failed!");
            return CRWL_OK;
        }

        task = (crwl_task_t *)addr;

        /* 4. 解析Undo数据信息 */
        ret = crwl_task_parse(r->str, task);
        if (CRWL_OK != ret)
        {
            log_error(ctx->log, "Parse task string failed! %s", r->str);

            freeReplyObject(r);
            lqueue_mem_dealloc(&worker[sched->last_idx].undo_taskq, addr);
            return CRWL_ERR;
        }

        /* 4. 放入Worker任务队列 */
        ret = lqueue_push(&worker[sched->last_idx].undo_taskq, addr);
        if (CRWL_OK != ret)
        {
            log_error(ctx->log, "Push into worker queue failed!");

            freeReplyObject(r);
            lqueue_mem_dealloc(&worker[sched->last_idx].undo_taskq, addr);
            return CRWL_OK;
        }

        freeReplyObject(r);
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_sched_push_undo_task
 **功    能: 将数据放入UNDO队列
 **输入参数: 
 **     ctx: 全局信息
 **     sched: 调度对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
static int crwl_sched_push_undo_task(crwl_cntx_t *ctx, crwl_sched_t *sched)
{
    int len;
    redisReply *r; 
    list_node_t *node;
    crwl_seed_item_t *seed;
    char task_str[CRWL_TASK_STR_LEN];

    node = ctx->conf.seed.head;
    while (NULL != node)
    {
        seed = (crwl_seed_item_t *)node->data;

        /* 1. 组装任务格式 */
        len = snprintf(task_str, sizeof(task_str),
                "<TASK>\n"
                "\t<TYPE>%d</TYPE>\n"
                "\t<BODY>\n"
                "\t\t<URI DEEP=\"%d\">%s</URI>\n"
                "\t</BODY>\n"
                "</TASK>",
                CRWL_TASK_DOWN_WEBPAGE_BY_URL, seed->deep+1, seed->uri);
        if (len >= sizeof(task_str))
        {
            log_info(ctx->log, "Task string is too long! uri:[%s]", seed->uri);
            node = node->next;
            continue;
        }

        /* 2. 插入Undo任务队列 */
        r = redisCommand(sched->redis_ctx,
                "RPUSH %s %s", ctx->conf.redis.undo_taskq, task_str);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Push into undo task queue failed!");
            return CRWL_ERR;
        }

        freeReplyObject(r);
        node = node->next;
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
    node = xml_search(xml, "TASK.TYPE");
    if (NULL == node)
    {
        xml_destroy(xml);
        return CRWL_ERR;
    }

    task->type = atoi(node->value);
    switch(task->type)
    {
        case CRWL_TASK_DOWN_WEBPAGE_BY_URL:
        {
            task->length = sizeof(crwl_task_t) + sizeof(crwl_task_down_webpage_by_uri_t);

            ret = crwl_task_parse_download_webpage_by_uri(
                    xml, (crwl_task_down_webpage_by_uri_t *)(task + 1));
            if (CRWL_OK != ret)
            {
                xml_destroy(xml);
                return CRWL_ERR;
            }
            break;
        }
        case CRWL_TASK_DOWN_WEBPAGE_BY_IP:
        {
            break;
        }
        default:
        {
            break;
        }
    }

    xml_destroy(xml);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_task_parse_download_webpage_by_uri
 **功    能: 解析TASK字串中DOWNLOAD WEBPAGE BY URI的配置
 **输入参数: 
 **     xml: XML树
 **输出参数:
 **     dw: Download webpage by uri的配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
static int crwl_task_parse_download_webpage_by_uri(
        xml_tree_t *xml, crwl_task_down_webpage_by_uri_t *dw)
{
    xml_node_t *node, *body;

    /* 定位BODY结点 */
    body = xml_search(xml, "TASK.BODY");
    if (NULL == body)
    {
        return CRWL_ERR;
    }

    /* 获取URI */
    node = xml_rsearch(xml, body, "URI");
    if (NULL == node)
    {
        return CRWL_ERR;
    }

    snprintf(dw->uri, sizeof(dw->uri), "%s", node->value);

    /* 获取URI.DEEP */
    node = xml_rsearch(xml, body, "URI.DEEP");
    if (NULL == node)
    {
        return CRWL_ERR;
    }

    dw->deep = atoi(node->value);
    dw->port = CRWL_WEB_SVR_PORT;

    return CRWL_OK;
}

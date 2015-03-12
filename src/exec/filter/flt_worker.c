/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: flt_worker.c
 ** 版本号: 1.0
 ** 描  述: 过滤处理模块
 **         负责进行网页的过滤处理
 ** 作  者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/

#include "log.h"
#include "str.h"
#include "http.h"
#include "filter.h"
#include "common.h"
#include "syscall.h"
#include "crawler.h"
#include "flt_conf.h"
#include "flt_worker.h"

static int flt_worker_workflow(flt_cntx_t *ctx, flt_worker_t *worker);
static int flt_worker_deep_hdl(flt_cntx_t *ctx, flt_worker_t *worker, gumbo_result_t *result);

/******************************************************************************
 **函数名称: flt_push_task
 **功    能: 将Seed放入UNDO队列
 **输入参数: 
 **     filter: Filter对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
static int flt_push_task(flt_cntx_t *ctx)
{
    int ret;
    uint32_t len, idx, i;
    redisReply *r; 
    flt_seed_conf_t *seed;
    char task_str[FLT_TASK_STR_LEN];
    flt_conf_t *conf = ctx->conf;
    uri_field_t field;
    flt_domain_ip_map_t map;

    for (idx=0; idx<conf->seed_num; ++idx)
    {
        seed = &conf->seed[idx];
        if (seed->depth > conf->download.depth) /* 判断网页深度 */
        {
            continue;
        }

        /* > 将href转至uri */
        if (0 != href_to_uri((const char *)seed->uri, "", &field))
        {
            log_info(ctx->log, "Uri [%s] is invalid!", (char *)seed->uri);
            continue;
        }

        /* > 查询域名IP映射 */
        ret = flt_get_domain_ip_map(ctx, seed->uri, &map);
        if (0 != ret || 0 == map.ip_num)
        {
            log_error(ctx->log, "Get ip failed! uri:%s host:%s", field.uri, field.host);
            return FLT_ERR;
        }

        i = rand() % map.ip_num;

        /* > 组装任务格式 */
        len = flt_get_task_str(task_str, sizeof(task_str),
                seed->uri, seed->depth, map.ip[i].ip, map.ip[i].family);
        if (len >= sizeof(task_str))
        {
            log_info(ctx->log, "Task string is too long! uri:[%s]", seed->uri);
            continue;
        }

        /* 2. 插入Undo任务队列 */
        r = redis_rpush(ctx->redis->master, ctx->conf->redis.undo_taskq, task_str);
        if (NULL == r
            || REDIS_REPLY_NIL == r->type)
        {
            if (r) { freeReplyObject(r); }
            log_error(ctx->log, "Push into undo task queue failed!");
            return FLT_ERR;
        }

        freeReplyObject(r);
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_worker_get_by_idx
 **功    能: 通过索引获取WORKER对象
 **输入参数:
 **     ctx: 全局信息
 **     idx: 索引号
 **输出参数: NONE
 **返    回: 工作对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
#define flt_worker_get_by_idx(ctx, idx) (&ctx->worker[idx])

/******************************************************************************
 **函数名称: flt_worker_self
 **功    能: 获取工作对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 爬虫对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
static flt_worker_t *flt_worker_self(flt_cntx_t *ctx)
{
    int tidx;

    tidx = thread_pool_get_tidx(ctx->worker_tp);
    if (tidx < 0)
    {
        return NULL;
    }

    return flt_worker_get_by_idx(ctx, tidx);
}



/******************************************************************************
 **函数名称: flt_worker_get_webpage_info
 **功    能: 获取网页信息
 **输入参数: 
 **     path: 网页信息文件
 **输出参数:
 **     info: 网页信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
static int flt_worker_get_webpage_info(
        const char *path, flt_webpage_info_t *info, log_cycle_t *log)
{
    xml_tree_t *xml;
    xml_option_t opt;
    mem_pool_t *pool;
    xml_node_t *node, *fix;

    memset(&opt, 0, sizeof(opt));

    /* 1. 新建内存池 */
    pool = mem_pool_creat(4 * KB);
    if (NULL == pool)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* 2. 新建XML树 */
    opt.pool = pool;
    opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    xml = xml_creat(info->fname, &opt);
    if (NULL == xml)
    {
        mem_pool_destroy(pool);
        log_error(log, "Create XML failed!");
        return CRWL_ERR;
    }

    /* 2. 提取网页信息 */
    do
    {
        fix = xml_query(xml, ".WPI");
        if (NULL == fix)
        {
            log_error(log, "Get WPI mark failed!");
            break;
        }

        /* 获取URI字段 */
        node = xml_rquery(xml, fix, "URI");
        if (NULL == node)
        {
            log_error(log, "Get URI mark failed!");
            break;
        }

        snprintf(info->uri, sizeof(info->uri), "%s", node->value);

        /* 获取DEPTH字段 */
        node = xml_rquery(xml, fix, "URI.DEPTH");
        if (NULL == node)
        {
            log_error(log, "Get DEPTH mark failed!");
            break;
        }

        info->depth = atoi(node->value);

        /* 获取IP字段 */
        node = xml_rquery(xml, fix, "URI.IP");
        if (NULL == node)
        {
            log_error(log, "Get IP mark failed!");
            break;
        }

        snprintf(info->ip, sizeof(info->ip), "%s", node->value);

        /* 获取PORT字段 */
        node = xml_rquery(xml, fix, "URI.PORT");
        if (NULL == node)
        {
            log_error(log, "Get PORT mark failed!");
            break;
        }

        info->port = atoi(node->value);

        /* 获取HTML字段 */
        node = xml_rquery(xml, fix, "HTML");
        if (NULL == node)
        {
            log_error(log, "Get HTML mark failed!");
            break;
        }

        snprintf(info->html, sizeof(info->html), "%s", node->value);

        /* 获取HTML.SIZE字段 */
        node = xml_rquery(xml, fix, "HTML.SIZE");
        if (NULL == node)
        {
            log_error(log, "Get HTML.SIZE mark failed!");
            break;
        }

        info->size = atoi(node->value);
        if (info->size <= 0)
        {
            log_info(log, "Html size is zero!");
            break;
        }

        xml_destroy(xml);
        mem_pool_destroy(pool);
        return CRWL_OK;
    } while(0);

    /* 3. 释放XML树 */
    xml_destroy(xml);
    mem_pool_destroy(pool);
    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: flt_worker_routine
 **功    能: 运行工作线程
 **输入参数: 
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
void *flt_worker_routine(void *_ctx)
{
    flt_task_t *task;
    flt_worker_t *worker;
    flt_cntx_t *ctx = (flt_cntx_t *)_ctx;

    worker = flt_worker_self(ctx);

    while (1)
    {
        /* > 获取任务数据 */
        task = queue_pop(ctx->taskq);
        if (NULL == task)
        {
            Sleep(1);
            continue;
        }

        /* > 提取网页数据 */
        if (flt_worker_get_webpage_info(task->path, &worker->info, ctx->log))
        {
            queue_dealloc(ctx->taskq, task);
            continue;
        }

        queue_dealloc(ctx->taskq, task);

        /* > 进行网页处理 */
        flt_worker_workflow(ctx, worker);
    }
    return (void *)-1;
}

/******************************************************************************
 **函数名称: flt_worker_init
 **功    能: 初始化工作对象
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **     idx: 索引编号
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
int flt_worker_init(flt_cntx_t *ctx, flt_worker_t *worker, int idx)
{
    worker->tidx = idx;
    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_worker_destroy
 **功    能: 销毁工作对象
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
int flt_worker_destroy(flt_cntx_t *ctx, flt_worker_t *worker)
{
    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_worker_deep_hdl
 **功    能: 超链接的深入分析和处理
 **输入参数: 
 **     ctx: 解析器对象
 **     result: URI集合
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断超链接深度
 **     2. 判断超链接是否已被爬取
 **     3. 将超链接插入任务队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
static int flt_worker_deep_hdl(flt_cntx_t *ctx, flt_worker_t *worker, gumbo_result_t *result)
{
    uint32_t len, idx, ret;
    redisReply *r; 
    uri_field_t field;
    flt_domain_ip_map_t map;
    char task_str[FLT_TASK_STR_LEN];
    flt_conf_t *conf = ctx->conf;
    list_node_t *node = result->list->head;
    flt_webpage_info_t *info = &worker->info;

    /* 遍历URL集合 */
    for (; NULL != node; node = node->next)
    {
        /* > 将href转至uri */
        if (0 != href_to_uri((const char *)node->data, info->uri, &field))
        {
            log_info(ctx->log, "Uri [%s] is invalid!", (char *)node->data);
            continue;
        }

        /* > 判断URI是否已经被推送到队列中 */
        if (flt_is_uri_push(ctx->redis, conf->redis.push_tab, field.uri))
        {
            log_info(ctx->log, "Uri [%s] was pushed!", (char *)node->data);
            continue;
        }

        /* > 获取域名IP映射数据 */
        ret = flt_get_domain_ip_map(ctx, field.host, &map);
        if (0 != ret || 0 == map.ip_num)
        {
            log_error(ctx->log, "Get ip failed! uri:%s host:%s", field.uri, field.host);
            return FLT_ERR;
        }

        idx = random() % map.ip_num;

        /* > 组装任务格式 */
        len = flt_get_task_str(task_str, sizeof(task_str),
                field.uri, info->depth+1, map.ip[idx].ip, map.ip[idx].family);
        if (len >= sizeof(task_str))
        {
            log_info(ctx->log, "Task string is too long! [%s]", task_str);
            continue;
        }

        /* 4. 插入Undo任务队列 */
        r = redis_rpush(ctx->redis->master, ctx->conf->redis.undo_taskq, task_str);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            log_error(ctx->log, "Push into undo task queue failed!");
            return FLT_ERR;
        }

        freeReplyObject(r);
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_worker_workflow
 **功    能: 处理流程
 **输入参数: 
 **     filter: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.11 #
 ******************************************************************************/
static int flt_worker_workflow(flt_cntx_t *ctx, flt_worker_t *worker)
{
    gumbo_html_t *html;             /* HTML对象 */
    gumbo_result_t *result;         /* 结果集合 */
    char fpath[FILE_PATH_MAX_LEN];  /* HTML文件名 */
    flt_conf_t *conf = ctx->conf;
    flt_webpage_info_t *info = &worker->info;

    /* > 判断网页深度 */
    if (info->depth > conf->download.depth)
    {
        log_info(ctx->log, "Drop handle webpage! uri:%s depth:%d", info->uri, info->depth);
        return FLT_OK;
    }

    /* > 判断网页(URI)是否已下载
     *  判断的同时设置网页的下载标志
     *  如果已下载，则不做提取该网页中的超链接
     * */
    if (flt_is_uri_down(ctx->redis, conf->redis.done_tab, info->uri))
    {
        log_info(ctx->log, "Uri [%s] was downloaded!", info->uri);
        return FLT_OK;
    }

    snprintf(fpath, sizeof(fpath), "%s/%s", conf->download.path, info->html);

    /* > 解析HTML文件 */
    html = gumbo_html_parse(fpath);
    if (NULL == html)
    {
        log_error(ctx->log, "Parse html failed! fpath:%s", fpath);
        return FLT_ERR;
    }

    /* > 提取超链接 */
    result = gumbo_parse_href(html);
    if (NULL == result)
    {
        log_error(ctx->log, "Parse href failed! fpath:%s", fpath);

        gumbo_result_destroy(result);
        gumbo_html_destroy(html);
        return FLT_ERR;
    }

    /* > 深入处理超链接
     *  1. 判断超链接深度
     *  2. 判断超链接是否已被爬取
     *  3. 将超链接插入任务队列
     * */
    if (flt_worker_deep_hdl(ctx, worker, result))
    {
        log_error(ctx->log, "Deep handler failed! fpath:%s", fpath);

        gumbo_result_destroy(result);
        gumbo_html_destroy(html);
        return FLT_ERR;
    }

    /* 5. 内存释放 */
    gumbo_result_destroy(result);
    gumbo_html_destroy(html);
    return FLT_OK;
}

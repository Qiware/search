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
#include "uri.h"
#include "http.h"
#include "filter.h"
#include "common.h"
#include "syscall.h"
#include "flt_conf.h"
#include "flt_worker.h"

/* 静态函数 */
static int flt_worker_workflow(flt_cntx_t *ctx, flt_worker_t *worker);
static int flt_worker_deep_hdl(flt_cntx_t *ctx, flt_worker_t *worker, gumbo_result_t *result);

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

    tidx = thread_pool_get_tidx(ctx->worker_pool);
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
        return FLT_ERR;
    }

    /* 2. 新建XML树 */
    opt.pool = pool;
    opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        mem_pool_destroy(pool);
        log_error(log, "Create XML failed! path:%s", path);
        return FLT_ERR;
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

        snprintf(info->fname, sizeof(info->fname), "%s", path);

        xml_destroy(xml);
        mem_pool_destroy(pool);
        return FLT_OK;
    } while(0);

    /* 3. 释放XML树 */
    xml_destroy(xml);
    mem_pool_destroy(pool);
    return FLT_ERR;
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
        if (flt_worker_get_webpage_info(task->path, &worker->info, worker->log))
        {
            queue_dealloc(ctx->taskq, task);
            continue;
        }

        /* > 进行网页处理 */
        flt_worker_workflow(ctx, worker);

        /* > 释放内存和磁盘 */
        remove(task->path);
        queue_dealloc(ctx->taskq, task);
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
    worker->log = ctx->log;

    /* > 连接Redis集群 */
    worker->redis = redis_clst_init(ctx->conf->redis.conf, ctx->conf->redis.num);
    if (NULL == worker->redis)
    {
        log_error(worker->log, "Initialize redis context failed!");
        return FLT_ERR;
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_worker_destroy
 **功    能: 销毁工作对象
 **输入参数: 
 **     ctx: 全局对象
 **     worker: 工作对象
 **输出参数: NONE
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
    uri_field_t field;
    flt_conf_t *conf = ctx->conf;
    list_node_t *node = result->list->head;
    flt_webpage_info_t *info = &worker->info;

    /* 遍历URL集合 */
    for (; NULL != node; node = node->next)
    {
        /* > 将href转至uri */
        if (0 != href_to_uri((const char *)node->data, info->uri, &field))
        {
            log_warn(ctx->log, "Href [%s] of uri [%s] is invalid!", (char *)node->data, info->uri);
            continue;
        }

        if (URI_HTTP_PROTOCOL != field.protocol)
        {
            log_warn(ctx->log, "Uri [%s] isn't base http protocol!", field.uri);
            continue;
        }

        /* > 判断URI是否已经被推送到队列中 */
        if (flt_is_uri_push(worker->redis, conf->redis.push_tab, field.uri))
        {
            log_warn(ctx->log, "Uri [%s] was pushed!", field.uri);
            continue;
        }

        /* > 推送到CRWL队列 */
        if (flt_push_url_to_crwlq(ctx, field.uri, field.host, field.port, info->depth+1))
        {
            log_error(ctx->log, "Push url [%s] redis taskq failed!", (char *)node->data);
            continue;
        }
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
    if (flt_is_uri_down(worker->redis, conf->redis.done_tab, info->uri))
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

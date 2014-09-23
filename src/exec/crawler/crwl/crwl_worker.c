/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/

#include "crawler.h"
#include "xml_tree.h"
#include "thread_pool.h"

static int crwl_parse_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log);
static void *crwl_worker_routine(void *_ctx);

/******************************************************************************
 **函数名称: crwl_load_conf
 **功    能: 加载爬虫配置信息
 **输入参数:
 **     path: 配置路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载爬虫配置
 **     2. 提取配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log)
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

    /* 2. 提取爬虫配置 */
    ret = crwl_parse_conf(xml, conf, log);
    if (0 != ret)
    {
        xml_destroy(xml);
        log_error(log, "Crawler get configuration failed! path:%s", path);
        return CRWL_ERR;
    }

    xml_destroy(xml);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_start_work
 **功    能: 启动爬虫服务
 **输入参数: 
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
crwl_ctx_t *crwl_start_work(crwl_conf_t *conf, log_cycle_t *log)
{
    int idx;
    crwl_ctx_t *ctx;

    /* 1. 创建爬虫对象 */
    ctx = (crwl_ctx_t *)calloc(1, sizeof(crwl_ctx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    memcpy(&ctx->conf, conf, sizeof(crwl_conf_t));

    /* 2. 初始化线程池 */
    ctx->tpool = thread_pool_init(conf->thread_num);
    if (NULL == ctx->tpool)
    {
        free(ctx);
        log_error(log, "Initialize thread pool failed!");
        return NULL;
    }

    /* 3. 初始化线程池 */
    for (idx=0; idx<conf->thread_num; ++idx)
    {
        thread_pool_add_worker(ctx->tpool, crwl_worker_routine, ctx);
    }
    
    return ctx;
}

/******************************************************************************
 **函数名称: crwl_parse_conf
 **功    能: 提取配置信息
 **输入参数: 
 **     xml: 配置文件
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static int crwl_parse_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *curr, *node, *node2;

    /* 1. 定位工作进程配置 */
    curr = xml_search(xml, ".SEARCH.CRWLSYS.CRAWLER");
    if (NULL != curr)
    {
        log_error(log, "Didn't configure worker process!");
        return CRWL_ERR;
    }

    /* 2. 爬虫线程数(相对查找) */
    node = xml_rsearch(xml, curr, "THD_NUM");
    if (NULL != node)
    {
        log_warn(log, "Didn't configure the number of worker process!");
        conf->thread_num = CRWL_WORKER_DEF_THD_NUM;
    }
    else
    {
        conf->thread_num = atoi(node->value);
    }

    if (conf->thread_num <= 0)
    {
        log_error(log, "Crawler thread number [%d] isn't right!", conf->thread_num);
        return CRWL_ERR;
    }

    /* 3. 任务分配服务IP(相对查找) */
    node = xml_rsearch(xml, curr, "SVRIP");
    if (NULL != node)
    {
        log_error(log, "Didn't configure distribute server ip address!");
        return CRWL_ERR;
    }

    snprintf(conf->svrip, sizeof(conf->svrip), "%s", node->value);

    /* 4. 任务分配服务端口(相对查找) */
    node = xml_rsearch(xml, curr, "PORT");
    if (NULL != node)
    {
        log_error(log, "Didn't configure distribute server port!");
        return CRWL_ERR;
    }

    conf->port = atoi(node->value);

    /* 5. 日志级别:如果本级没有设置，则继承上一级的配置 */
    node2 = curr;
    while (NULL != node2)
    {
        node = xml_rsearch(xml, node2, ".LOG.LEVEL");
        if (NULL != node)
        {
            snprintf(conf->log_level_str,
                sizeof(conf->log_level_str), "%s", node->value);
            break;
        }

        node2 = node2->parent;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_creat
 **功    能: 创建爬虫对象
 **输入参数: 
 **     ctx: 上下文
 **输出参数: NONE
 **返    回: 爬虫对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static crwl_worker_t *crwl_worker_creat(crwl_ctx_t *ctx)
{
    int ret;
    crwl_worker_t *worker;

    /* 1.创建爬虫对象 */
    worker = (crwl_worker_t *)calloc(1, sizeof(crwl_worker_t));
    if (NULL == worker)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    worker->log = ctx->log;

    /* 2.创建SLAB内存池 */
    ret = eslab_init(&worker->slab, CRWL_WORKER_SLAB_SIZE);
    if (0 != ret)
    {
        free(worker);
        log_error(worker->log, "Initialize slab pool failed!");
        return NULL;
    }

    return worker;
}

/******************************************************************************
 **函数名称: crwl_worker_destroy
 **功    能: 销毁爬虫对象
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_destroy(crwl_worker_t *worker)
{
    eslab_destroy(&worker->slab);
    free(worker);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_reset_fdset
 **功    能: 重置读写集合
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 最大的套接字
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_reset_fdset(crwl_worker_t *worker)
{
    return 0;
}

/******************************************************************************
 **函数名称: crwl_worker_recv_data
 **功    能: 接收数据
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_recv_data(crwl_worker_t *worker)
{
    int idx;
    crwl_sck_t *data;
    list2_node_t *node;

    node = worker->sck_lst.head;
    for (idx=0; idx<worker->sck_lst.num; ++idx)
    {
        data = (crwl_sck_t *)node->data;
        if (NULL == data)
        {
            continue;
        }

        if (FD_ISSET(data->fd, &worker->rdset))
        {
        }

        node = node->next;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_send_data
 **功    能: 发送数据
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_send_data(crwl_worker_t *worker)
{
    int idx;
    crwl_sck_t *data;
    list2_node_t *node;

    node = worker->sck_lst.head;
    for (idx=0; idx<worker->sck_lst.num; ++idx)
    {
        data = (crwl_sck_t *)node->data;
        if (NULL == data)
        {
            continue;
        }

        if (FD_ISSET(data->fd, &worker->wrset))
        {
        }

        node = node->next;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_event_hdl
 **功    能: 爬虫的事件处理
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_event_hdl(crwl_worker_t *worker)
{
    int ret;

    /* 1. 进行事件处理 */
    ret = crwl_worker_recv_data(worker);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Worker recv data failed!");
        return CRWL_ERR;
    }

    /* 2. 进行事件处理 */
    ret = crwl_worker_send_data(worker);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Worker send data failed!");
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_routine
 **功    能: 运行爬虫线程
 **输入参数: 
 **     _ctx: 上下文
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static void *crwl_worker_routine(void *_ctx)
{
    int ret, max;
    struct timeval tv;
    crwl_worker_t *worker;
    crwl_ctx_t *ctx = (crwl_ctx_t *)_ctx;

    worker = crwl_worker_creat(ctx);
    if (NULL == worker)
    {
        log_error(ctx->log, "Create worker failed!");
        return (void *)-1;
    }

    while (1)
    {
        /* 1. 设置读写集合 */
        FD_ZERO(&worker->rdset);
        FD_ZERO(&worker->wrset);

        max = crwl_worker_reset_fdset(worker);

        /* 2. 等待事件通知 */
        tv.tv_sec = CRWL_WORKER_TV_SEC;
        tv.tv_usec = CRWL_WORKER_TV_USEC;
        ret = select(max+1, &worker->rdset, &worker->wrset, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));

            crwl_worker_destroy(worker);
            return (void *)-1;
        }
        else if (0 == ret)
        {
            continue;
        }

        /* 3. 进行事件处理 */
        crwl_worker_event_hdl(worker);
    }

    crwl_worker_destroy(worker);
    return (void *)-1;
}

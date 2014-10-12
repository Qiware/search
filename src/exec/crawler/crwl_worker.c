/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_worker.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/

#include "common.h"
#include "syscall.h"
#include "crawler.h"
#include "xml_tree.h"
#include "thread_pool.h"
#include "crwl_worker.h"

static int crwl_worker_parse_conf(
        xml_tree_t *xml, crwl_worker_conf_t *conf, log_cycle_t *log);

static int crwl_worker_remove_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck);

static int crwl_worker_task_handler(crwl_worker_t *worker, crwl_task_t *t);

/******************************************************************************
 **函数名称: crwl_worker_load_conf
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
 **     在此需要验证参数的合法性!
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_worker_load_conf(crwl_worker_conf_t *conf, const char *path, log_cycle_t *log)
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
    ret = crwl_worker_parse_conf(xml, conf, log);
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
 **函数名称: crwl_worker_parse_conf
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
static int crwl_worker_parse_conf(
        xml_tree_t *xml, crwl_worker_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *curr, *node;

    /* 1. 定位工作进程配置 */
    curr = xml_search(xml, ".SEARCH.CRWLSYS.WORKER");
    if (NULL == curr)
    {
        log_error(log, "Didn't configure worker process!");
        return CRWL_ERR;
    }

    /* 2. 爬虫线程数(相对查找) */
    node = xml_rsearch(xml, curr, "THREAD_NUM");
    if (NULL == node)
    {
        log_warn(log, "Didn't configure the number of worker process!");
        conf->thread_num = CRWL_WRK_DEF_THD_NUM;
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
    if (NULL == node)
    {
        log_error(log, "Didn't configure distribute server ip address!");
        return CRWL_ERR;
    }

    snprintf(conf->svrip, sizeof(conf->svrip), "%s", node->value);

    /* 4. 任务分配服务端口(相对查找) */
    node = xml_rsearch(xml, curr, "PORT");
    if (NULL == node)
    {
        log_error(log, "Didn't configure distribute server port!");
        return CRWL_ERR;
    }

    conf->port = atoi(node->value);

    /* 5. 下载网页的数目(相对查找) */
    node = xml_rsearch(xml, curr, "LOAD_WEB_PAGE_NUM");
    if (NULL == node)
    {
        log_error(log, "Didn't configure load web page number!");
        return CRWL_ERR;
    }

    conf->load_web_page_num = atoi(node->value);
    if (!conf->load_web_page_num)
    {
        conf->load_web_page_num = CRWL_WRK_LOAD_WEB_PAGE_NUM;
    }

    /* 6. 任务队列配置(相对查找) */
    node = xml_rsearch(xml, curr, "TASK_QUEUE.COUNT");
    if (NULL == node)
    {
        log_error(log, "Didn't configure count of task queue unit!");
        return CRWL_ERR;
    }

    conf->task_queue.count = atoi(node->value);
    if (conf->task_queue.count <= 0)
    {
        conf->task_queue.count = 1;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_get
 **功    能: 获取爬虫对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 爬虫对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.09 #
 ******************************************************************************/
static crwl_worker_t *crwl_worker_get(crwl_cntx_t *ctx)
{
    int tidx;

    tidx = thread_pool_get_tidx(ctx->worker_tp);

    return (crwl_worker_t *)ctx->worker_tp->data + tidx;
}

/******************************************************************************
 **函数名称: crwl_worker_init
 **功    能: 创建爬虫对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
int crwl_worker_init(crwl_cntx_t *ctx, crwl_worker_t *worker)
{
    int ret;
    const crwl_worker_conf_t *conf = &ctx->conf.worker;

    worker->log = ctx->log;

    /* 1. 创建SLAB内存池 */
    ret = eslab_init(&worker->slab, CRWL_WRK_SLAB_SIZE);
    if (0 != ret)
    {
        log_error(worker->log, "Initialize slab pool failed!");
        return CRWL_ERR;
    }

    /* 2. 创建任务队列 */
    ret = crwl_task_queue_init(&worker->task, conf->task_queue.count);
    if (CRWL_OK != ret)
    {
        eslab_destroy(&worker->slab);
        return CRWL_ERR;
    }

    return CRWL_OK;
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
    void *data;
    crwl_cntx_t *ctx = worker->ctx;

    eslab_destroy(&worker->slab);

    /* 释放TASK队列及数据 */
    pthread_rwlock_wrlock(&worker->task.lock);
    while (1)
    {
        /* 弹出数据 */
        data = queue_pop(&worker->task.queue);
        if (NULL == data)
        {
            break;
        }

        /* 释放内存 */
        pthread_rwlock_wrlock(&ctx->slab_lock);
        eslab_free(&ctx->slab, data);
        pthread_rwlock_unlock(&ctx->slab_lock);
    }
    pthread_rwlock_unlock(&worker->task.lock);

    pthread_rwlock_destroy(&worker->task.lock);
    queue_destroy(&worker->task.queue);

    free(worker);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_tpool_init
 **功    能: 初始化爬虫线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.11 #
 ******************************************************************************/
int crwl_worker_tpool_init(crwl_cntx_t *ctx)
{
    int idx, ret, num;
    crwl_worker_t *worker;
    const crwl_worker_conf_t *conf = &ctx->conf.worker;

    /* 1. 创建Worker线程池 */
    ctx->worker_tp = thread_pool_init(conf->thread_num);
    if (NULL == ctx->worker_tp)
    {
        log_error(ctx->log, "Initialize thread pool failed!");
        return CRWL_ERR;
    }

    /* 2. 新建Worker对象 */
    ctx->worker_tp->data =
        (crwl_worker_t *)calloc(conf->thread_num, sizeof(crwl_worker_t));
    if (NULL == ctx->worker_tp->data)
    {
        thread_pool_destroy(ctx->worker_tp);
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* 3. 依次初始化Worker对象 */
    for (idx=0; idx<conf->thread_num; ++idx)
    {
        worker = (crwl_worker_t *)ctx->worker_tp->data + idx;

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
        worker = (crwl_worker_t *)ctx->worker_tp->data + idx;

        crwl_worker_destroy(worker);
    }

    free(ctx->worker_tp->data);
    thread_pool_destroy(ctx->worker_tp);

    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_worker_get_task
 **功    能: 获取工作任务
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
static int crwl_worker_get_task(crwl_cntx_t *ctx, crwl_worker_t *worker)
{
    void *data;
    crwl_task_t *t;

    /* 1. 判断是否应该取任务 */
    if (0 == worker->task.queue.num
        || worker->sock_list.num >= ctx->conf.worker.load_web_page_num)
    {
        return CRWL_OK;
    }

    /* 2. 从任务队列取数据 */
    pthread_rwlock_wrlock(&worker->task.lock);

    data = queue_pop(&worker->task.queue);
    if (NULL == data)
    {
        pthread_rwlock_unlock(&worker->task.lock);
        return CRWL_OK;
    }

    pthread_rwlock_unlock(&worker->task.lock);

    /* 3. 连接远程Web服务器 */
    t = (crwl_task_t *)data;

    return crwl_worker_task_handler(worker, t);
}

/******************************************************************************
 **函数名称: crwl_worker_fdset
 **功    能: 设置读写集合
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 最大的套接字
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_fdset(crwl_worker_t *worker)
{
    int max = -1;
    list_node_t *node;
    crwl_worker_socket_t *sck;

    node = (list_node_t *)worker->sock_list.head;
    for (; NULL != node; node = node->next)
    {
        sck = (crwl_worker_socket_t *)node->data;

        /* 1. 设置可读集合 */
        FD_SET(sck->sckid, &worker->rdset);

        /* 2. 设置可写集合
         *  正在发送数据或发送链表不为空时, 表示有数据需要发送,
         *  因此, 需要将此套接字加入到可写集合 */
        if (NULL != sck->send.addr
            || NULL != sck->send_list.head)
        {
            FD_SET(sck->sckid, &worker->wrset);
        }

        max = (max < sck->sckid)? sck->sckid : max;
    }

    return max;
}

/******************************************************************************
 **函数名称: crwl_worker_fsync
 **功    能: 将接收的HTML同步到文件
 **输入参数: 
 **     worker: 爬虫对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_fsync(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    fprintf(stdout, "%s", sck->read.addr);
    sck->read.off = 0;
    sck->read.total = CRWL_WRK_READ_SIZE;
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_trav_recv
 **功    能: 遍历接收数据
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 依次遍历套接字, 判断是否可读
 **     2. 如果可读, 则接收数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_trav_recv(crwl_worker_t *worker)
{
    int n, left;
    time_t ctm = time(NULL);
    list_node_t *node;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否可读 */
    node = worker->sock_list.head;
    for (; NULL != node; node = node->next)
    {
        sck = (crwl_worker_socket_t *)node->data;
        if (NULL == sck)
        {
            continue;
        }

        /* 2. 如果可读, 则接收数据 */
        if (!FD_ISSET(sck->sckid, &worker->rdset))
        {
            continue;
        }

        sck->rdtm = ctm;
        left = sck->read.total - sck->read.off;

        n = Readn(sck->sckid, sck->read.addr + sck->read.off, left);
        if (n < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));
            Close(sck->sckid);
            return CRWL_ERR;
        }
        else if (0 == n)
        {
            log_error(worker->log, "End of read data! uri:%s", sck->uri);
            crwl_worker_fsync(worker, sck);
            Close(sck->sckid);
            continue;
        }

        /* 3. 将HTML数据写入文件 */
        sck->read.off += n;
        if (sck->read.off >= CRWL_WRK_SYNC_SIZE)
        {
            crwl_worker_fsync(worker, sck);
        }
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
 **     1. 如果发送指针为空, 则从发送队列取数据
 **     2. 发送数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
static int crwl_worker_send_data(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    int n, left;
    list_node_t *node;
    crwl_data_info_t *info;

    /* 1. 从发送列表取数据 */
    if (!sck->send.addr)
    {
        node = list_remove_head(&sck->send_list);
        if (NULL == node)
        {
            return CRWL_OK;
        }

        info = (crwl_data_info_t *)node->data;

        sck->send.addr = node->data;
        sck->send.off = sizeof(crwl_data_info_t);
        sck->send.total = info->length + sizeof(crwl_data_info_t);

        eslab_free(&worker->slab, node);
    }

    /* 2. 发送数据 */
    sck->wrtm = time(NULL);
    left = sck->send.total - sck->send.off;

    n = Writen(sck->sckid, sck->send.addr + sck->send.off, left);
    if (n < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));
        eslab_free(&worker->slab, sck->send.addr);
        return CRWL_ERR;
    }

    sck->send.off += n;
    left = sck->send.total - sck->send.off;
    if (0 == left)
    {
        eslab_free(&worker->slab, sck->send.addr);

        sck->send.addr = NULL;
        sck->send.total = 0;
        sck->send.off = 0;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_trav_send
 **功    能: 遍历发送数据
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 依次遍历套接字列表
 **     2. 判断是否可写
 **     3. 发送数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_trav_send(crwl_worker_t *worker)
{
    int ret;
    list_node_t *node, *prev;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字列表 */
    node = worker->sock_list.head;
    prev = node;
    for (; NULL != node; node = node->next)
    {
        sck = (crwl_worker_socket_t *)node->data;
        if (NULL == sck)
        {
            prev = node;
            continue;
        }

        /* 2. 判断是否可写 */
        if (!FD_ISSET(sck->sckid, &worker->wrset))
        {
            prev = node;
            continue;
        }
        
        /* 3. 发送数据 */
        ret = crwl_worker_send_data(worker, sck);
        if (CRWL_OK != ret)
        {
            log_error(worker->log, "Send data failed!"
                " ipaddr:[%s:%d] uri:[%s] base64_uri:[%s]",
                sck->ipaddr, sck->port, sck->uri, sck->base64_uri);

            /* 将异常套接字踢出链表, 并释放空间 */
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                worker->sock_list.head = node->next;
            }

            if (node == worker->sock_list.tail)
            {
                worker->sock_list.tail = prev;
            }

            --worker->sock_list.num;

            eslab_free(&worker->slab, node);

            crwl_worker_remove_sock(worker, sck);

            return CRWL_ERR;  /* 结束处理：简化异常的后续链表处理逻辑 */
        }
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_timeout_hdl
 **功    能: 爬虫的超时处理
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 依次遍历套接字, 判断是否超时
 **     2. 超时关闭套接字、释放内存等
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.28 #
 ******************************************************************************/
static int crwl_worker_timeout_hdl(crwl_worker_t *worker)
{
    time_t ctm = time(NULL);
    list_node_t *node, *prev;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否超时 */
    node = worker->sock_list.head;
    prev = node;
    while (NULL != node)
    {
        sck = (crwl_worker_socket_t *)node->data;
        if (NULL == sck)
        {
            prev = node;
            node = node->next;
            continue;
        }

        if ((ctm - sck->rdtm <= CRWL_WRK_TMOUT_SEC)
            && (ctm - sck->wrtm <= CRWL_WRK_TMOUT_SEC))
        {
            prev = node;
            node = node->next;
            continue;
        }

        Close(sck->sckid);
        crwl_worker_fsync(worker, sck);

        /* 删除链表头 */
        if (prev == node)
        {
            if (worker->sock_list.head == worker->sock_list.tail)
            {
                worker->sock_list.head = NULL;
                worker->sock_list.tail = NULL;

                --worker->sock_list.num;
                eslab_free(&worker->slab, node);
                crwl_worker_remove_sock(worker, sck);
                return CRWL_OK;
            }

            prev = node->next;
            worker->sock_list.head = node->next;

            --worker->sock_list.num;
            eslab_free(&worker->slab, node);
            crwl_worker_remove_sock(worker, sck);

            node = prev;
            continue;
        }
        /* 删除链表尾 */
        else if (node == worker->sock_list.tail)
        {
            worker->sock_list.tail = prev;

            --worker->sock_list.num;
            eslab_free(&worker->slab, node);
            crwl_worker_remove_sock(worker, sck);
            return CRWL_OK;
        }

        /* 删除链表中间结点 */
        prev->next = node->next;

        --worker->sock_list.num;
        eslab_free(&worker->slab, node);
        crwl_worker_remove_sock(worker, sck);

        node = prev->next;
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
 **     1. 接收数据
 **     2. 发送数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_event_hdl(crwl_worker_t *worker)
{
    int ret;

    /* 1. 接收数据 */
    ret = crwl_worker_trav_recv(worker);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Worker recv data failed!");
        return CRWL_ERR;
    }

    /* 2. 发送数据 */
    ret = crwl_worker_trav_send(worker);
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
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID *
 **实现描述: 
 **     1. 创建爬虫对象
 **     2. 设置读写集合
 **     3. 等待事件通知
 **     4. 进行事件处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
void *crwl_worker_routine(void *_ctx)
{
    int ret, max;
    struct timeval tv;
    crwl_worker_t *worker;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;

    /* 1. 创建爬虫对象 */
    worker = crwl_worker_get(ctx);
    if (NULL == worker)
    {
        log_error(ctx->log, "Initialize worker failed!");
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    while (1)
    {
        /* 2. 获取爬虫任务 */
        crwl_worker_get_task(ctx, worker);

        /* 1. 设置读写集合 */
        FD_ZERO(&worker->rdset);
        FD_ZERO(&worker->wrset);

        max = crwl_worker_fdset(worker);
        if (max < 0)
        {
            continue;
        }

        /* 3. 等待事件通知 */
        tv.tv_sec = CRWL_WRK_TV_SEC;
        tv.tv_usec = CRWL_WRK_TV_USEC;
        ret = select(max+1, &worker->rdset, &worker->wrset, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));

            crwl_worker_destroy(worker);
            pthread_exit((void *)-1);
            return (void *)-1;
        }
        else if (0 == ret)
        {
            crwl_worker_timeout_hdl(worker);
            continue;
        }

        /* 4. 进行事件处理 */
        crwl_worker_event_hdl(worker);
    }

    crwl_worker_destroy(worker);
    pthread_exit((void *)-1);
    return (void *)-1;
}

/******************************************************************************
 **函数名称: crwl_worker_add_sock
 **功    能: 添加套接字
 **输入参数: 
 **     worker: 爬虫对象 
 **     sck: 套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
int crwl_worker_add_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    int ret;
    list_node_t *node;

    /* 1. 申请内存空间 */
    node = eslab_alloc(&worker->slab, sizeof(list_node_t));
    if (NULL == node)
    {
        log_error(worker->log, "Alloc memory failed!");
        return CRWL_ERR;
    }

    node->data = sck;

    /* 2. 插入链表尾 */
    ret = list_insert_tail(&worker->sock_list, node);
    if (0 != ret)
    {
        log_error(worker->log, "Insert socket node failed!");
        eslab_free(&worker->slab, node);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_remove_sock
 **功    能: 删除套接字
 **输入参数: 
 **     worker: 爬虫对象 
 **     sck: 套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
static int crwl_worker_remove_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    list_node_t *item, *next;

    Close(sck->sckid);

    item = sck->send_list.head;
    while (NULL != item)
    {
        eslab_free(&worker->slab, item->data);

        next = item->next;

        eslab_free(&worker->slab, item);

        item = next;
    }

    eslab_free(&worker->slab, sck);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_task_handler
 **功    能: 爬虫任务的处理
 **输入参数: 
 **     worker: 爬虫对象 
 **     t: 任务对象(对象+数据)
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
static int crwl_worker_task_handler(crwl_worker_t *worker, crwl_task_t *t)
{
    char *args = (char *)t + sizeof(crwl_task_t);

    switch (t->type)
    {
        /* 通过URL加载网页 */
        case CRWL_TASK_LOAD_WEB_PAGE_BY_URL:
        {
            return crwl_task_load_webpage_by_uri(
                    worker, (const crwl_task_load_webpage_by_uri_t *)args);
        }
        /* 通过IP加载网页 */
        case CRWL_TASK_LOAD_WEB_PAGE_BY_IP:
        {
            return crwl_task_load_webpage_by_ip(
                    worker, (const crwl_task_load_webpage_by_ip_t *)args);
        }
        /* 未知任务类型 */
        case CRWL_TASK_TYPE_UNKNOWN:
        default:
        {
            log_debug(worker->log, "Unknown task type! [%d]", t->type);
            return CRWL_OK;
        }
    }
    return CRWL_OK;
}

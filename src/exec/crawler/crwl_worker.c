/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_worker.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#include <time.h>

#include "http.h"
#include "common.h"
#include "syscall.h"
#include "crawler.h"
#include "xml_tree.h"
#include "thread_pool.h"
#include "crwl_worker.h"

static int crwl_worker_task_handler(crwl_worker_t *worker, crwl_task_t *t);

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

    tidx = thread_pool_get_tidx(ctx->workers);

    return (crwl_worker_t *)ctx->workers->data + tidx;
}

/******************************************************************************
 **函数名称: crwl_worker_init
 **功    能: 创建爬虫对象
 **输入参数: 
 **     ctx: 全局信息
 **     worker: Worker对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     依次创建Worker的成员和所依赖的资源.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
int crwl_worker_init(crwl_cntx_t *ctx, crwl_worker_t *worker)
{
    int ret;
    const crwl_worker_conf_t *conf = &ctx->conf.worker;

    worker->ctx = ctx;
    worker->log = ctx->log;

    /* 1. 创建SLAB内存池 */
    ret = eslab_init(&worker->slab, CRWL_SLAB_SIZE);
    if (0 != ret)
    {
        log_error(worker->log, "Initialize slab pool failed!");
        return CRWL_ERR;
    }

    /* 2. 创建任务队列 */
    ret = lqueue_init(&worker->undo_taskq, conf->taskq_count, 5 * MB);
    if (CRWL_OK != ret)
    {
        eslab_destroy(&worker->slab);

        log_error(worker->log, "Init lock queue failed!");
        return CRWL_ERR;
    }

#if defined(__EVENT_EPOLL__)
    /* 3. 创建epoll对象 */
    worker->ep_fd = epoll_create(CRWL_EVENT_MAX_NUM);
    if (worker->ep_fd < 0)
    {
        lqueue_destroy(&worker->undo_taskq);
        eslab_destroy(&worker->slab);

        log_error(worker->log, "Create epoll failed! errmsg:[%d] %s!");
        return CRWL_ERR;
    }

    worker->events = (struct epoll_event *)eslab_alloc(
                            &worker->slab,
                            conf->connections * sizeof(struct epoll_event));
    if (NULL == worker->events)
    {
        lqueue_destroy(&worker->undo_taskq);
        eslab_destroy(&worker->slab);
        Close(worker->ep_fd);

        log_error(worker->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }
#endif /*__EVENT_EPOLL__*/

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
int crwl_worker_destroy(crwl_worker_t *worker)
{
    void *data;

    eslab_destroy(&worker->slab);

    /* 释放TASK队列及数据 */
    while (1)
    {
        /* 弹出数据 */
        data = lqueue_pop(&worker->undo_taskq);
        if (NULL == data)
        {
            break;
        }

        /* 释放内存 */
        lqueue_mem_dealloc(&worker->undo_taskq, data);
    }

    lqueue_destroy(&worker->undo_taskq);

#if defined(__EVENT_EPOLL__)
    close(worker->ep_fd);
#endif /*__EVENT_EPOLL__*/

    free(worker);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_fetch_task
 **功    能: 获取工作任务
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
static int crwl_worker_fetch_task(crwl_cntx_t *ctx, crwl_worker_t *worker)
{
    void *data;
    crwl_task_t *t;

    /* 1. 判断是否应该取任务 */
    if (0 == worker->undo_taskq.queue.num
        || worker->sock_list.num >= ctx->conf.worker.connections)
    {
        return CRWL_OK;
    }

    /* 2. 从任务队列取数据 */
    data = lqueue_pop(&worker->undo_taskq);
    if (NULL == data)
    {
        log_error(worker->log, "Get task from queue failed!");
        return CRWL_OK;
    }

    /* 3. 连接远程Web服务器 */
    t = (crwl_task_t *)data;

    crwl_worker_task_handler(worker, t);

    lqueue_mem_dealloc(&worker->undo_taskq, data);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_recv_data
 **功    能: 接收数据
 **输入参数: 
 **     worker: 爬虫对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     接收数据直到出现EAGAIN -- 一次性接收最大量的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.30 #
 ******************************************************************************/
static int crwl_worker_recv_data(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    int n, left;

    sck->rdtm = time(NULL);

    while (1)
    {
        left = sck->read.total - sck->read.off;

        n = Readn(sck->sckid, sck->read.addr + sck->read.off, left);
        if (n < left)
        {
            if (n > 0)          /* 等待再次触发 */
            {
                sck->webpage.size += n;

                log_debug(worker->log, "Wait next recv! uri:%s", sck->webpage.uri);

                /* 将HTML数据写入文件 */
                sck->read.off += n;
                sck->read.addr[sck->read.off] = '\0';
                if (sck->read.off >= CRWL_SYNC_SIZE)
                {
                    crwl_worker_webpage_fsync(worker, sck);
                }
                return CRWL_OK;
            }
            else if (0 == n)    /* 已关闭 */
            {
                log_debug(worker->log, "End of recv! uri:%s", sck->webpage.uri);

                crwl_worker_webpage_fsync(worker, sck);
                crwl_worker_webpage_finfo(worker, sck);
                crwl_worker_remove_sock(worker, sck);
                return CRWL_SCK_CLOSE;
            }

            if (EINTR == errno)
            {
                continue;
            }

            /* 异常情况处理 */
            log_error(worker->log, "errmsg:[%d] %s! uri:%s ip:%s",
                    errno, strerror(errno), sck->webpage.uri, sck->webpage.ip);
            crwl_worker_remove_sock(worker, sck);
            return CRWL_ERR;
        }

        sck->webpage.size += n;

        /* 3. 将HTML数据写入文件 */
        sck->read.off += n;
        sck->read.addr[sck->read.off] = '\0';
        if (sck->read.off >= CRWL_SYNC_SIZE)
        {
            crwl_worker_webpage_fsync(worker, sck);
        }
    }

    return CRWL_OK;
}

#if !defined(__EVENT_EPOLL__)
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
    list_node_t *node, *next;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否可读 */
    node = worker->sock_list.head;
    for (; NULL != node; node = next)
    {
        next = node->next;

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

        crwl_worker_recv_data(worker, sck);
    }

    return CRWL_OK;
}
#else /*__EVENT_EPOLL__*/
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
 **作    者: # Qifeng.zou # 2014.10.30 #
 ******************************************************************************/
static int crwl_worker_trav_recv(crwl_worker_t *worker)
{
    int idx;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否可读 */
    for (idx=0; idx<worker->ep_fds; ++idx)
    {
        /* 判断遍历是否可读 */
        if (!(worker->events[idx].events & EPOLLIN))
        {
            continue;
        }

        /* 接收发送过来的数据 */
        sck = crwl_worker_query_sock(worker, worker->events[idx].data.fd);
        if (NULL == sck)
        {
            log_fatal(worker->log, "Query socket failed!");
            assert(0);
            continue;
        }
        
        /* 接收网络数据 */
        log_info(worker->log, "idx:%d/%d fd:%d!",
                idx, worker->ep_fds, worker->events[idx].data.fd);
        crwl_worker_recv_data(worker, sck);
    }

    return CRWL_OK;
}
#endif /*__EVENT_EPOLL__*/

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
 **     发送数据直到出现EAGAIN -- 一次性发送最大量的数据
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
static int crwl_worker_send_data(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    int n, left;
    list_node_t *node;
    crwl_data_info_t *info;
#if defined(__EVENT_EPOLL__)
    struct epoll_event ev;
#endif /*__EVENT_EPOLL__*/

    sck->wrtm = time(NULL);

    while (1)
    {
        /* 1. 从发送列表取数据 */
        if (!sck->send.addr)
        {
            node = list_remove_head(&sck->send_list);
            if (NULL == node)
            {
            #if defined(__EVENT_EPOLL__)
                memset(&ev, 0, sizeof(ev));

                ev.data.fd = sck->sckid;
                ev.events = EPOLLIN | EPOLLET;

                epoll_ctl(worker->ep_fd, EPOLL_CTL_MOD, sck->sckid, &ev);    
            #endif /*__EVENT_EPOLL__*/
                return CRWL_OK;
            }

            info = (crwl_data_info_t *)node->data;

            sck->send.addr = (void *)node->data;
            sck->send.off = sizeof(crwl_data_info_t); /* 不发送头部信息 */
            sck->send.total = info->length;

            eslab_dealloc(&worker->slab, node);
        }

        /* 2. 发送数据 */
        left = sck->send.total - sck->send.off;

        n = Writen(sck->sckid, sck->send.addr + sck->send.off, left);
        if (n < 0)
        {
            log_error(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));

            eslab_dealloc(&worker->slab, sck->send.addr);
            sck->send.addr = NULL;
            crwl_worker_remove_sock(worker, sck);
            return CRWL_ERR;
        }
        else if (n != left)
        {
            sck->send.off += n;
            left = sck->send.total - sck->send.off;
            /* 等待下次继续发送, 空间暂不释放 */
            return CRWL_OK;
        }

        sck->send.off += n;
        left = sck->send.total - sck->send.off;

        /* 释放空间 */
        eslab_dealloc(&worker->slab, sck->send.addr);

        sck->send.addr = NULL;
        sck->send.total = 0;
        sck->send.off = 0;
    }

    return CRWL_OK;
}

#if defined(__EVENT_EPOLL__)
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
    int idx;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否可读 */
    for (idx=0; idx<worker->ep_fds; ++idx)
    {
        /* 判断遍历是否可读 */
        if (!(worker->events[idx].events & EPOLLOUT))
        {
            continue;
        }

        /* 接收发送过来的数据 */
        sck = crwl_worker_query_sock(worker, worker->events[idx].data.fd);
        if (NULL == sck)
        {
            log_fatal(worker->log, "Query socket failed!");
            assert(0);
            continue;
        }
        
        /* 接收网络数据 */
        crwl_worker_send_data(worker, sck);
    }

    return CRWL_OK;
}
#else /*!__EVENT_EPOLL__*/
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
 **     如果在发送过程中出现异常情况, 发送函数中将会删除该套接字.
 **作    者: # Qifeng.zou # 2014.09.23 #
 ******************************************************************************/
static int crwl_worker_trav_send(crwl_worker_t *worker)
{
    list_node_t *node, *next;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字列表 */
    node = worker->sock_list.head;
    for (; NULL != node; node = next)
    {
        next = node->next;

        sck = (crwl_worker_socket_t *)node->data;
        if (NULL == sck)
        {
            continue;
        }

        /* 2. 判断是否可写 */
        if (!FD_ISSET(sck->sckid, &worker->wrset))
        {
            continue;
        }
        
        /* 3. 发送数据 */
        crwl_worker_send_data(worker, sck);
    }

    return CRWL_OK;
}
#endif /*!__EVENT_EPOLL__*/

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
    list_node_t *node, *next;
    crwl_worker_socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否超时 */
    node = worker->sock_list.head;
    for (; NULL != node; node = next)
    {
        next = node->next;

        sck = (crwl_worker_socket_t *)node->data;
        if (NULL == sck)
        {
            continue;
        }

        /* 超时未发送或接收数据时, 认为无数据传输, 将直接关闭套接字 */
        if ((ctm - sck->rdtm <= CRWL_SCK_TMOUT_SEC)
            || (ctm - sck->wrtm <= CRWL_SCK_TMOUT_SEC))
        {
            continue;
        }

        log_error(worker->log, "Timeout! uri:%s ip:%s size:%d",
                sck->webpage.uri, sck->webpage.ip, sck->webpage.size);

        crwl_worker_webpage_fsync(worker, sck);
        crwl_worker_webpage_finfo(worker, sck);

        crwl_worker_remove_sock(worker, sck);
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

    /* 3. 超时扫描 */
    crwl_worker_timeout_hdl(worker);
    return CRWL_OK;
}

#if defined(__EVENT_EPOLL__)
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
    crwl_worker_t *worker;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;
    crwl_worker_conf_t *conf = &ctx->conf.worker;

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
        crwl_worker_fetch_task(ctx, worker);

        /* 3. 等待事件通知 */
        worker->ep_fds = epoll_wait(
                worker->ep_fd, worker->events, conf->connections, CRWL_TMOUT_SEC);
        if (worker->ep_fds < 0)
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
        else if (0 == worker->ep_fds)
        {
            crwl_worker_timeout_hdl(worker);
            continue;
        }

        /* 5. 进行事件处理 */
        crwl_worker_event_hdl(worker);
    }

    crwl_worker_destroy(worker);
    pthread_exit((void *)-1);
    return (void *)-1;
}
#else /*!__EVENT_EPOLL__*/
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
        crwl_worker_fetch_task(ctx, worker);

        /* 3. 设置读写集合 */
        FD_ZERO(&worker->rdset);
        FD_ZERO(&worker->wrset);

        max = crwl_worker_fdset(worker);
        if (max < 0)
        {
            usleep(500);
            continue;
        }

        /* 4. 等待事件通知 */
        tv.tv_sec = CRWL_TMOUT_SEC;
        tv.tv_usec = CRWL_TMOUT_USEC;

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

        /* 5. 进行事件处理 */
        crwl_worker_event_hdl(worker);
    }

    crwl_worker_destroy(worker);
    pthread_exit((void *)-1);
    return (void *)-1;
}
#endif /*__EVENT_EPOLL__*/

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
#if defined(__EVENT_EPOLL__)
    struct epoll_event ev;
#endif /*__EVENT_EPOLL__*/

    /* 1. 申请内存空间 */
    node = eslab_alloc(&worker->slab, sizeof(list_node_t));
    if (NULL == node)
    {
        log_error(worker->log, "Alloc memory failed!");
        return CRWL_ERR;
    }

    sck->read.addr = sck->recv;
    sck->read.off = 0;
    sck->read.total = CRWL_RECV_SIZE;

    node->data = sck;

    /* 2. 插入链表尾 */
    ret = list_insert_tail(&worker->sock_list, node);
    if (0 != ret)
    {
        log_error(worker->log, "Insert socket node failed!");
        eslab_dealloc(&worker->slab, node);
        return CRWL_ERR;
    }

#if defined(__EVENT_EPOLL__)
    /* 3. 加入epoll监听(首先是发送数据, 所以设置EPOLLOUT) */
    memset(&ev, 0, sizeof(ev));

    ev.data.fd = sck->sckid;
    ev.events = EPOLLOUT | EPOLLET;

    ret = epoll_ctl(worker->ep_fd, EPOLL_CTL_ADD, sck->sckid, &ev);
    if (0 != ret)
    {
        log_error(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));

        crwl_worker_remove_sock(worker, sck);
        return CRWL_ERR;
    }
#endif /*__EVENT_EPOLL__*/

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_query_sock
 **功    能: 通过sckid查找socket对象
 **输入参数: 
 **     worker: 爬虫对象 
 **     sckid: 套接字ID
 **输出参数: NONE
 **返    回: Socket对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.30 #
 ******************************************************************************/
crwl_worker_socket_t *crwl_worker_query_sock(crwl_worker_t *worker, int sckid)
{
    list_node_t *node;
    crwl_worker_socket_t *sck;

    node = (list_node_t *)worker->sock_list.head;
    for (; NULL != node; node = node->next)
    {
        sck = (crwl_worker_socket_t *)node->data;
        if (sckid == sck->sckid)
        {
            return sck;
        }
    }

    return NULL;
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
int crwl_worker_remove_sock(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
#if defined(__EVENT_EPOLL__)
    struct epoll_event ev;
#endif /*__EVENT_EPOLL__*/
    list_node_t *item, *next, *prev;

    log_debug(worker->log, "Remove socket! ip:%s port:%d",
            sck->webpage.ip, sck->webpage.port);

#if defined(__EVENT_EPOLL__)
    /* 移除epoll监听 */
    epoll_ctl(worker->ep_fd, EPOLL_CTL_DEL, sck->sckid, &ev);
#endif /*__EVENT_EPOLL__*/

    if (sck->webpage.fp)
    {
        fclose(sck->webpage.fp);
    }
    Close(sck->sckid);

    /* 1. 释放发送链表 */
    item = sck->send_list.head;
    while (NULL != item)
    {
        eslab_dealloc(&worker->slab, item->data);

        next = item->next;

        eslab_dealloc(&worker->slab, item);

        item = next;
    }

    /* 2. 从套接字链表剔除SCK */
    item = worker->sock_list.head;
    prev = item;
    while (NULL != item)
    {
        if (item->data == sck)
        {
            /* 在链表头 */
            if (prev == item)
            {
                if (worker->sock_list.head == worker->sock_list.tail)
                {
                    worker->sock_list.num = 0;
                    worker->sock_list.head = NULL;
                    worker->sock_list.tail = NULL;

                    eslab_dealloc(&worker->slab, item);
                    eslab_dealloc(&worker->slab, sck);
                    return CRWL_OK;
                }

                --worker->sock_list.num;
                worker->sock_list.head = item->next;

                eslab_dealloc(&worker->slab, item);
                eslab_dealloc(&worker->slab, sck);
                return CRWL_OK;
            }
            /* 在链表尾 */
            else if (item == worker->sock_list.tail)
            {
                --worker->sock_list.num;
                prev->next = NULL;
                worker->sock_list.tail = prev;

                eslab_dealloc(&worker->slab, item);
                eslab_dealloc(&worker->slab, sck);
                return CRWL_OK;
            }
            /* 在链表中间 */
            --worker->sock_list.num;
            prev->next = item->next;

            eslab_dealloc(&worker->slab, item);
            eslab_dealloc(&worker->slab, sck);
            return CRWL_OK;
        }

        prev = item;
        item = item->next;
    }

    log_error(worker->log, "Didn't find special socket!");
    return CRWL_OK; /* 未找到 */
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
    char *args = (char *)(t + 1);

    switch (t->type)
    {
        /* 通过URL加载网页 */
        case CRWL_TASK_DOWN_WEBPAGE_BY_URL:
        {
            return crwl_task_down_webpage_by_uri(
                    worker, (const crwl_task_down_webpage_by_uri_t *)args);
        }
        /* 通过IP加载网页 */
        case CRWL_TASK_DOWN_WEBPAGE_BY_IP:
        {
            return crwl_task_down_webpage_by_ip(
                    worker, (const crwl_task_down_webpage_by_ip_t *)args);
        }
        /* 未知任务类型 */
        case CRWL_TASK_TYPE_UNKNOWN:
        default:
        {
            log_error(worker->log, "Unknown task type! [%d]", t->type);
            return CRWL_OK;
        }
    }
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_add_http_get_req
 **功    能: 添加HTTP GET请求
 **输入参数: 
 **     worker: 爬虫对象 
 **     sck: 指定套接字
 **     uri: 源URI
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建链表结点
 **     2. 新建HTTP GET请求
 **     3. 将结点插入链表
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
int crwl_worker_add_http_get_req(
        crwl_worker_t *worker, crwl_worker_socket_t *sck, const char *uri)
{
    int ret;
    void *addr;
    list_node_t *node;
    crwl_data_info_t *info;

    do
    {
        /* 1. 创建链表结点 */
        node = eslab_alloc(&worker->slab, sizeof(list_node_t));
        if (NULL == node)
        {
            log_error(worker->log, "Alloc memory from slab failed!");
            break;
        }

        /* 2. 新建HTTP GET请求 */
        node->data = eslab_alloc(&worker->slab,
                        sizeof(crwl_data_info_t) + HTTP_GET_REQ_STR_LEN);
        if (NULL == node->data)
        {
            log_error(worker->log, "Alloc memory from slab failed!");
            break;
        }

        info = node->data;
        addr = node->data + sizeof(crwl_data_info_t);

        ret = http_get_request(uri, addr, HTTP_GET_REQ_STR_LEN);
        if (0 != ret)
        {
            log_error(worker->log, "HTTP GET request string failed");
            break;
        }

        info->type = CRWL_HTTP_GET_REQ;
        info->length = sizeof(crwl_data_info_t) + strlen((const char *)addr);

        /* 3. 将结点插入链表 */
        ret = list_insert_tail(&sck->send_list, node);
        if (0 != ret)
        {
            log_error(worker->log, "Insert list tail failed");
            break;
        }

        return CRWL_OK;
    } while(0);

    /* 释放空间 */
    if (NULL != node)
    {
        if (node->data)
        {
            eslab_dealloc(&worker->slab, node->data);
        }
        eslab_dealloc(&worker->slab, node);
    }

    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_worker_webpage_creat
 **功    能: 打开网页存储文件
 **输入参数: 
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     网页存储文件在此fopen(), 在crwl_worker_remove_sock()中fclose().
 **作    者: # Qifeng.zou # 2014.10.15 #
 ******************************************************************************/
int crwl_worker_webpage_creat(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    struct tm loctm;
    char path[FILE_NAME_MAX_LEN];

    localtime_r(&sck->crtm.time, &loctm);

    sck->webpage.size = 0;
    sck->webpage.idx = ++worker->down_webpage_total;

    snprintf(sck->webpage.fname, sizeof(sck->webpage.fname),
            "%02d-%08ld-%04d%02d%02d%02d%02d%02d%03d",
            worker->tidx, sck->webpage.idx,
            loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
            loctm.tm_hour, loctm.tm_min, loctm.tm_sec, sck->crtm.millitm);

    snprintf(path, sizeof(path), "%s/%s.html",
            worker->ctx->conf.download.path, sck->webpage.fname);

    Mkdir2(path, 0777);

    sck->webpage.fp = fopen(path, "w");
    if (NULL == sck->webpage.fp)
    {
        log_error(worker->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_webpage_finfo
 **功    能: 创建网页的信息文件
 **输入参数: 
 **     worker: 爬虫对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 首先将数据写入临时目录
 **     2. 再将临时文件移入指定目录
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
int crwl_worker_webpage_finfo(crwl_worker_t *worker, crwl_worker_socket_t *sck)
{
    FILE *fp;
    struct tm loctm;
    char path[FILE_NAME_MAX_LEN],
         temp[FILE_NAME_MAX_LEN];

    localtime_r(&sck->crtm.time, &loctm);

    snprintf(temp, sizeof(temp), "%s/info/.temp/%s.info",
            worker->ctx->conf.download.path, sck->webpage.fname);

    snprintf(path, sizeof(path), "%s/info/%s.info",
            worker->ctx->conf.download.path, sck->webpage.fname);

    Mkdir2(temp, 0777);

    /* 1. 新建文件 */
    fp = fopen(temp, "w");
    if (NULL == fp)
    {
        log_error(worker->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return CRWL_ERR;
    }

    /* 2. 写入校验内容 */
    fprintf(fp, 
        "<INFO>\n"
        "\t<URI DEPTH=\"%d\" IP=\"%s\" PORT=\"%d\">%s</URI>\n"
        "\t<HTML SIZE=\"%lu\">%s.html</HTML>\n"
        "</INFO>\n",
        sck->webpage.depth, sck->webpage.ip,
        sck->webpage.port, sck->webpage.uri,
        sck->webpage.size, sck->webpage.fname);

    fclose(fp);

    Rename(temp, path);

    return CRWL_OK;
}

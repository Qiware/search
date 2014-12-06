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
    void *addr;

    worker->ctx = ctx;
    worker->log = ctx->log;
    worker->scan_tm = time(NULL);
    worker->conf = ctx->conf;

    /* 1. 创建SLAB内存池 */
    addr = calloc(1, CRWL_SLAB_SIZE);
    if (NULL == addr)
    {
        log_fatal(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    worker->slab = slab_init(addr, CRWL_SLAB_SIZE);
    if (NULL == worker->slab)
    {
        free(addr);
        log_error(worker->log, "Initialize slab pool failed!");
        return CRWL_ERR;
    }

    /* 2. 创建任务队列 */
    worker->undo_taskq = lqueue_init(worker->conf->worker.taskq_count, 20 * MB);
    if (NULL == worker->undo_taskq)
    {
        slab_destroy(worker->slab);

        log_error(worker->log, "Init lock queue failed! taskq_count:%d",
                worker->conf->worker.taskq_count);
        return CRWL_ERR;
    }

    /* 3. 创建epoll对象 */
    worker->ep_fd = epoll_create(CRWL_EVENT_MAX_NUM);
    if (worker->ep_fd < 0)
    {
        lqueue_destroy(worker->undo_taskq);
        slab_destroy(worker->slab);

        log_error(worker->log, "Create epoll failed! errmsg:[%d] %s!");
        return CRWL_ERR;
    }

    worker->events = (struct epoll_event *)slab_alloc(
            worker->slab,
            worker->conf->worker.conn_max_num * sizeof(struct epoll_event));
    if (NULL == worker->events)
    {
        lqueue_destroy(worker->undo_taskq);
        slab_destroy(worker->slab);
        Close(worker->ep_fd);

        log_error(worker->log, "Alloc memory from slab failed!");
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
int crwl_worker_destroy(crwl_worker_t *worker)
{
    void *data;

    slab_destroy(worker->slab);

    /* 释放TASK队列及数据 */
    while (1)
    {
        /* 弹出数据 */
        data = lqueue_pop(worker->undo_taskq);
        if (NULL == data)
        {
            break;
        }

        /* 释放内存 */
        lqueue_mem_dealloc(worker->undo_taskq, data);
    }

    lqueue_destroy(worker->undo_taskq);

    close(worker->ep_fd);

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
    int idx, num;
    void *data;
    crwl_task_t *t;

    /* 1. 判断是否应该取任务 */
    if (0 == worker->undo_taskq->queue.num
        || worker->sock_list.num >= worker->conf->worker.conn_max_num)
    {
        return CRWL_OK;
    }

    /* 2. 从任务队列取数据 */
    num = worker->conf->worker.conn_max_num - worker->sock_list.num;

    for (idx=0; idx<num; ++idx)
    {
        data = lqueue_trypop(worker->undo_taskq);
        if (NULL == data)
        {
            return CRWL_OK;
        }

        /* 3. 连接远程Web服务器 */
        t = (crwl_task_t *)data;

        crwl_worker_task_handler(worker, t);

        lqueue_mem_dealloc(worker->undo_taskq, data);
    }
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
int crwl_worker_recv_data(crwl_worker_t *worker, socket_t *sck)
{
    int n, left;
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    sck->rdtm = time(NULL);

    while (1)
    {
        left = sck->recv.total - sck->recv.off;

        n = read(sck->fd, sck->recv.addr + sck->recv.off, left);
        if (n > 0)
        {
            log_debug(worker->log, "Recv! uri:%s n:%d", data->webpage.uri, n);

            data->webpage.size += n;

            /* 将HTML数据写入文件 */
            sck->recv.off += n;
            sck->recv.addr[sck->recv.off] = '\0';
            if (sck->recv.off >= CRWL_SYNC_SIZE)
            {
                crwl_worker_webpage_fsync(worker, sck);
            }
            continue;
        }
        else if (0 == n)
        {
            log_info(worker->log, "End of recv! uri:%s size:%d",
                    data->webpage.uri, data->webpage.size);

            crwl_worker_webpage_fsync(worker, sck);
            crwl_worker_webpage_finfo(worker, sck);
            crwl_worker_remove_sock(worker, sck);
            return CRWL_SCK_CLOSE;
        }
        else if (n < 0 && EAGAIN == errno)
        {
            log_debug(worker->log, "Again! uri:%s", data->webpage.uri);
            return CRWL_OK;
        }
        else if (EINTR == errno)
        {
            continue;
        }

        log_error(worker->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }
}

/******************************************************************************
 **函数名称: crwl_worker_send_data
 **功    能: 发送数据
 **输入参数: 
 **     worker: 爬虫对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 如果发送指针为空, 则从发送队列取数据
 **     2. 发送数据
 **注意事项: 
 **     发送数据直到出现EAGAIN -- 一次性发送最大量的数据
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
int crwl_worker_send_data(crwl_worker_t *worker, socket_t *sck)
{
    int n, left;
    list_node_t *node;
    crwl_data_info_t *info;
    struct epoll_event ev;
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    sck->wrtm = time(NULL);

    while (1)
    {
        /* 1. 从发送列表取数据 */
        if (!sck->send.addr)
        {
            node = list_remove_head(&data->send_list);
            if (NULL == node)
            {
                memset(&ev, 0, sizeof(ev));

                ev.data.ptr = sck;
                ev.events = EPOLLIN | EPOLLET;  /* 边缘触发 */

                epoll_ctl(worker->ep_fd, EPOLL_CTL_MOD, sck->fd, &ev);    

                return CRWL_OK;
            }

            info = (crwl_data_info_t *)node->data;

            sck->send.addr = (void *)node->data;
            sck->send.off = sizeof(crwl_data_info_t); /* 不发送头部信息 */
            sck->send.total = info->length;

            slab_dealloc(worker->slab, node);
        }

        /* 2. 发送数据 */
        left = sck->send.total - sck->send.off;

        n = Writen(sck->fd, sck->send.addr + sck->send.off, left);
        if (n < 0)
        {
            log_error(worker->log, "errmsg:[%d] %s! total:%d off:%d",
                    errno, strerror(errno), sck->send.total, sck->send.off);

            slab_dealloc(worker->slab, sck->send.addr);
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
        slab_dealloc(worker->slab, sck->send.addr);

        sck->send.addr = NULL;
        sck->send.total = 0;
        sck->send.off = 0;
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
    list_node_t *node, *next;
    socket_t *sck;
    crwl_worker_socket_data_t *data;

    /* 1. 依次遍历套接字, 判断是否超时 */
    node = worker->sock_list.head;
    for (; NULL != node; node = next)
    {
        next = node->next;

        sck = (socket_t *)node->data;
        if (NULL == sck)
        {
            continue;
        }

        data = (crwl_worker_socket_data_t *)sck->data;

        /* 超时未发送或接收数据时, 认为无数据传输, 将直接关闭套接字 */
        if ((ctm - sck->rdtm <= worker->conf->worker.conn_tmout_sec)
            || (ctm - sck->wrtm <= worker->conf->worker.conn_tmout_sec))
        {
            continue; /* 未超时 */
        }

        log_info(worker->log, "Timeout! uri:%s ip:%s size:%d!",
                data->webpage.uri, data->webpage.ip, data->webpage.size);

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
    int idx;
    time_t ctm = time(NULL);
    socket_t *sck;

    /* 1. 依次遍历套接字, 判断是否可读可写 */
    for (idx=0; idx<worker->fds; ++idx)
    {
        sck = (socket_t *)worker->events[idx].data.ptr;

        /* 1.1 判断是否可读 */
        if (worker->events[idx].events & EPOLLIN)
        {
            /* 接收网络数据 */
            if (sck->recv_cb(worker, sck))
            {
                continue; /* 异常: 不必判断是否可写(套接字已关闭) */
            }
        }

        /* 1.2 判断是否可写 */
        if (worker->events[idx].events & EPOLLOUT)
        {
            /* 发送网络数据 */
            if (sck->send_cb(worker, sck))
            {
                continue; /* 异常: 套接字已关闭 */
            }
        }
    }

    /* 2. 超时扫描 */
    if (ctm - worker->scan_tm > CRWL_TMOUT_SCAN_SEC)
    {
        worker->scan_tm = ctm;

        crwl_worker_timeout_hdl(worker);
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

        /* 3. 等待事件通知 */
        worker->fds = epoll_wait(
                worker->ep_fd, worker->events,
                worker->conf->worker.conn_max_num, CRWL_TMOUT_SEC);
        if (worker->fds < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));

            crwl_worker_destroy(worker);
            abort();
            return (void *)-1;
        }
        else if (0 == worker->fds) /* Timeout */
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
int crwl_worker_add_sock(crwl_worker_t *worker, socket_t *sck)
{
    list_node_t *node;
    struct epoll_event ev;
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    /* 1. 申请内存空间 */
    node = slab_alloc(worker->slab, sizeof(list_node_t));
    if (NULL == node)
    {
        log_error(worker->log, "Alloc memory failed!");
        return CRWL_ERR;
    }

    sck->recv.addr = data->recv;
    sck->recv.off = 0;
    sck->recv.total = CRWL_RECV_SIZE;

    node->data = sck;

    /* 2. 插入链表尾 */
    if (list_insert_tail(&worker->sock_list, node))
    {
        log_error(worker->log, "Insert socket node failed!");
        slab_dealloc(worker->slab, node);
        return CRWL_ERR;
    }

    /* 3. 加入epoll监听(首先是发送数据, 所以需设置EPOLLOUT,
     * 又可能服务端主动断开连接, 所以需要设置EPOLLIN, 否则可能出现EPIPE的异常) */
    memset(&ev, 0, sizeof(ev));

    ev.data.ptr = sck;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET; /* 边缘触发 */

    epoll_ctl(worker->ep_fd, EPOLL_CTL_ADD, sck->fd, &ev);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_query_sock
 **功    能: 通过fd查找socket对象
 **输入参数: 
 **     worker: 爬虫对象 
 **     fd: 套接字ID
 **输出参数: NONE
 **返    回: Socket对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.30 #
 ******************************************************************************/
socket_t *crwl_worker_query_sock(crwl_worker_t *worker, int fd)
{
    list_node_t *node;
    socket_t *sck;

    node = (list_node_t *)worker->sock_list.head;
    for (; NULL != node; node = node->next)
    {
        sck = (socket_t *)node->data;
        if (fd == sck->fd)
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
int crwl_worker_remove_sock(crwl_worker_t *worker, socket_t *sck)
{
    struct epoll_event ev;
    list_node_t *item, *next, *prev;
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    log_debug(worker->log, "Remove socket! ip:%s port:%d",
            data->webpage.ip, data->webpage.port);

    /* 移除epoll监听 */
    epoll_ctl(worker->ep_fd, EPOLL_CTL_DEL, sck->fd, &ev);

    if (data->webpage.fp)
    {
        fClose(data->webpage.fp);
    }
    Close(sck->fd);

    /* 1. 释放发送链表 */
    item = data->send_list.head;
    while (NULL != item)
    {
        slab_dealloc(worker->slab, item->data);

        next = item->next;

        slab_dealloc(worker->slab, item);

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

                    slab_dealloc(worker->slab, item);
                    crwl_worker_socket_dealloc(worker, sck);
                    return CRWL_OK;
                }

                --worker->sock_list.num;
                worker->sock_list.head = item->next;

                slab_dealloc(worker->slab, item);
                crwl_worker_socket_dealloc(worker, sck);
                return CRWL_OK;
            }
            /* 在链表尾 */
            else if (item == worker->sock_list.tail)
            {
                --worker->sock_list.num;
                prev->next = NULL;
                worker->sock_list.tail = prev;

                slab_dealloc(worker->slab, item);
                crwl_worker_socket_dealloc(worker, sck);
                return CRWL_OK;
            }
            /* 在链表中间 */
            --worker->sock_list.num;
            prev->next = item->next;

            slab_dealloc(worker->slab, item);
            crwl_worker_socket_dealloc(worker, sck);
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
        crwl_worker_t *worker, socket_t *sck, const char *uri)
{
    void *addr;
    list_node_t *node;
    crwl_data_info_t *info;
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    do
    {
        /* 1. 创建链表结点 */
        node = slab_alloc(worker->slab, sizeof(list_node_t));
        if (NULL == node)
        {
            log_error(worker->log, "Alloc memory from slab failed!");
            break;
        }

        /* 2. 新建HTTP GET请求 */
        node->data = slab_alloc(worker->slab,
                        sizeof(crwl_data_info_t) + HTTP_GET_REQ_STR_LEN);
        if (NULL == node->data)
        {
            log_error(worker->log, "Alloc memory from slab failed!");
            break;
        }

        info = node->data;
        addr = node->data + sizeof(crwl_data_info_t);

        if (http_get_request(uri, addr, HTTP_GET_REQ_STR_LEN))
        {
            log_error(worker->log, "HTTP GET request string failed");
            break;
        }

        info->type = CRWL_HTTP_GET_REQ;
        info->length = sizeof(crwl_data_info_t) + strlen((const char *)addr);

        /* 3. 将结点插入链表 */
        if (list_insert_tail(&data->send_list, node))
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
            slab_dealloc(worker->slab, node->data);
        }
        slab_dealloc(worker->slab, node);
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
 **     网页存储文件在此fopen(), 在crwl_worker_remove_sock()中fClose().
 **作    者: # Qifeng.zou # 2014.10.15 #
 ******************************************************************************/
int crwl_worker_webpage_creat(crwl_worker_t *worker, socket_t *sck)
{
    struct tm loctm;
    char path[FILE_NAME_MAX_LEN];
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    localtime_r(&sck->crtm.time, &loctm);

    data->webpage.size = 0;
    data->webpage.idx = ++worker->down_webpage_total;

    snprintf(data->webpage.fname, sizeof(data->webpage.fname),
            "%02d-%08ld-%04d%02d%02d%02d%02d%02d%03d",
            worker->tidx, data->webpage.idx,
            loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
            loctm.tm_hour, loctm.tm_min, loctm.tm_sec, sck->crtm.millitm);

    snprintf(path, sizeof(path), "%s/%s.html",
            worker->conf->download.path, data->webpage.fname);

    Mkdir2(path, 0777);

    data->webpage.fp = fopen(path, "w");
    if (NULL == data->webpage.fp)
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
int crwl_worker_webpage_finfo(crwl_worker_t *worker, socket_t *sck)
{
    FILE *fp;
    struct tm loctm;
    char path[FILE_NAME_MAX_LEN],
         temp[FILE_NAME_MAX_LEN];
    crwl_worker_socket_data_t *data = (crwl_worker_socket_data_t *)sck->data;

    localtime_r(&sck->crtm.time, &loctm);

    snprintf(temp, sizeof(temp), "%s/wpi/.temp/%s.wpi",
            worker->conf->download.path, data->webpage.fname);

    snprintf(path, sizeof(path), "%s/wpi/%s.wpi",
            worker->conf->download.path, data->webpage.fname);

    Mkdir2(temp, 0777);

    /* 1. 新建文件 */
    fp = fopen(temp, "w");
    if (NULL == fp)
    {
        log_error(worker->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return CRWL_ERR;
    }

    /* 2. 写入校验内容 */
    crwl_write_webpage_finfo(fp, data);

    fclose(fp);

    rename(temp, path);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_socket_alloc
 **功    能: 为Socket对象申请空间
 **输入参数: 
 **     worker: 爬虫对象
 **输出参数: NONE
 **返    回: Socket对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.27 #
 ******************************************************************************/
socket_t *crwl_worker_socket_alloc(crwl_worker_t *worker)
{
    socket_t *sck;

    sck = slab_alloc(worker->slab, sizeof(socket_t));
    if (NULL == sck)
    {
        return NULL;
    }

    sck->data = slab_alloc(worker->slab, sizeof(crwl_worker_socket_data_t));
    if (NULL == sck->data)
    {
        return NULL;
    }

    return sck;
}

#include "search.h"
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "hash.h"
#include "list.h"
#include "common.h"
#include "syscall.h"
#include "xml_tree.h"
#include "srch_agent.h"
#include "xds_socket.h"
#include "thread_pool.h"

static srch_agent_t *srch_agent_get(srch_cntx_t *ctx);
static int srch_agent_add_conn(srch_cntx_t *ctx, srch_agent_t *agt);
int srch_agent_socket_cmp_cb(const void *pkey, const void *data);
static int srch_agent_del_conn(srch_agent_t *agt, socket_t *sck);

static int srch_agent_recv(srch_agent_t *agt, socket_t *sck);
static int srch_agent_send(srch_agent_t *agt, socket_t *sck);

static int srch_agent_event_hdl(srch_agent_t *agt);
static int srch_agent_event_timeout_hdl(srch_agent_t *agt);

/******************************************************************************
 **函数名称: srch_agent_routine
 **功    能: 运行Agent线程
 **输入参数:
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
void *srch_agent_routine(void *_ctx)
{
    srch_agent_t *agt;
    srch_cntx_t *ctx = (srch_cntx_t *)_ctx;

    /* 1. 获取Agent对象 */
    agt = srch_agent_get(ctx);
    if (NULL == agt)
    {
        log_error(agt->log, "Get agent failed!");
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    while (1)
    {
        /* 2. 从连接队列取数据 */
        if (srch_connq_used(ctx, agt->tidx))
        {
            if (srch_agent_add_conn(ctx, agt))
            {
                log_error(agt->log, "Add connection failed!");
            }
        }

        /* 3. 等待事件通知 */
        agt->fds = epoll_wait(agt->ep_fd, agt->events,
                SRCH_AGENT_EVENT_MAX_NUM, SRCH_AGENT_TMOUT_MSEC);
        if (agt->fds < 0)
        {
            if (EINTR == errno)
            {
                usleep(500);
                continue;
            }

            /* 异常情况 */
            log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == agt->fds)
        {
            agt->ctm = time(NULL);
            if (agt->ctm - agt->scan_tm > SRCH_TMOUT_SCAN_SEC)
            {
                agt->scan_tm = agt->ctm;

                srch_agent_event_timeout_hdl(agt);
            }
            continue;
        }

        /* 4. 处理事件通知 */
        srch_agent_event_hdl(agt);
    }

    return NULL;
}

/******************************************************************************
 **函数名称: srch_agent_init
 **功    能: 初始化Agent线程
 **输入参数:
 **     ctx: 全局信息
 **     r: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建epoll对象
 **     2. 创建命令套接字
 **     3. 创建套接字队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
int srch_agent_init(srch_cntx_t *ctx, srch_agent_t *agt)
{
    void *addr;
    char path[FILE_NAME_MAX_LEN];

    agt->ctx = ctx;
    agt->log = ctx->log;
    agt->conf = ctx->conf;

    /* 1. 创建SLAB内存池 */
    addr = calloc(1, SRCH_SLAB_SIZE);
    if (NULL == addr)
    {
        log_fatal(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SRCH_ERR;
    }

    agt->slab = slab_init(addr, SRCH_SLAB_SIZE);
    if (NULL == agt->slab)
    {
        free(addr);
        log_error(agt->log, "Initialize slab pool failed!");
        return SRCH_ERR;
    }

    /* 2. 创建连接红黑树 */
    agt->connections = rbt_creat(agt->slab,
            (mem_alloc_cb_t)slab_alloc, (mem_dealloc_cb_t)slab_dealloc);
    if (NULL == agt->connections)
    {
        log_error(agt->log, "Create socket hash table failed!");
        return SRCH_ERR;
    }

    do
    {
        /* 2. 创建epoll对象 */
        agt->ep_fd = epoll_create(SRCH_AGENT_EVENT_MAX_NUM);
        if (agt->ep_fd < 0)
        {
            log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        agt->events = slab_alloc(agt->slab,
                SRCH_AGENT_EVENT_MAX_NUM * sizeof(struct epoll_event));
        if (NULL == agt->events)
        {
            log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* 3. 创建命令套接字 */
        snprintf(path, sizeof(path), SRCH_RCV_CMD_PATH, agt->tidx);

        agt->cmd_sck_id = unix_udp_creat(path);
        if (agt->cmd_sck_id < 0)
        {
            log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        return SRCH_OK;
    } while(0);

    slab_destroy(agt->slab);
    Close(agt->ep_fd);
    Close(agt->cmd_sck_id);
    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_agent_destroy
 **功    能: 销毁Agent线程
 **输入参数:
 **     agt: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int srch_agent_destroy(srch_agent_t *agt)
{
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_get
 **功    能: 获取Agent对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: Agent对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.26 #
 ******************************************************************************/
static srch_agent_t *srch_agent_get(srch_cntx_t *ctx)
{
    int tidx;

    tidx = thread_pool_get_tidx(ctx->agents);

    return (srch_agent_t *)ctx->agents->data + tidx;
}

/******************************************************************************
 **函数名称: srch_agent_event_hdl
 **功    能: 事件通知处理
 **输入参数: 
 **     r: Agent对象
 **输出参数: NONE
 **返    回: Agent对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int srch_agent_event_hdl(srch_agent_t *agt)
{
    int idx, ret;
    socket_t *sck;

    agt->ctm = time(NULL);

    /* 1. 依次遍历套接字, 判断是否可读可写 */
    for (idx=0; idx<agt->fds; ++idx)
    {
        sck = (socket_t *)agt->events[idx].data.ptr;

        /* 1.1 判断是否可读 */
        if (agt->events[idx].events & EPOLLIN)
        {
            /* 接收网络数据 */
            ret = sck->recv_cb(agt, sck);
            if (SRCH_SCK_AGAIN != ret)
            {
                log_info(agt->log, "Delete connection! fd:%d", sck->fd);
                srch_agent_del_conn(agt, sck);
                continue; /* 异常-关闭SCK: 不必判断是否可写 */
            }
        }

        /* 1.2 判断是否可写 */
        if (agt->events[idx].events & EPOLLOUT)
        {
            /* 发送网络数据 */
            ret = sck->send_cb(agt, sck);
            if (SRCH_ERR == ret)
            {
                log_info(agt->log, "Delete connection! fd:%d", sck->fd);
                srch_agent_del_conn(agt, sck);
                continue; /* 异常: 套接字已关闭 */
            }
        }
    }

    /* 2. 超时扫描 */
    if (agt->ctm - agt->scan_tm > SRCH_TMOUT_SCAN_SEC)
    {
        agt->scan_tm = agt->ctm;

        srch_agent_event_timeout_hdl(agt);
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_get_timeout_conn_list
 **功    能: 将超时连接加入链表
 **输入参数: 
 **     node: 平衡二叉树结点
 **     timeout: 超时链表
 **输出参数: NONE
 **返    回: Agent对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.24 #
 ******************************************************************************/
static int srch_agent_get_timeout_conn_list(socket_t *sck, srch_conn_timeout_list_t *timeout)
{
    list_node_t *node;

    /* 判断是否超时，则加入到timeout链表中 */
    if ((timeout->ctm - sck->rdtm <= 15)
        || (timeout->ctm - sck->wrtm <= 15))
    {
        return SRCH_OK; /* 未超时 */
    }

    node = mem_pool_alloc(timeout->pool, sizeof(list_node_t));
    if (NULL == node)
    {
        return SRCH_ERR;
    }

    node->data = (void *)sck;

    list_insert_head(&timeout->list, node);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_event_timeout_hdl
 **功    能: 事件超时处理
 **输入参数: 
 **     agt: Agent对象
 **输出参数: NONE
 **返    回: Agent对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int srch_agent_event_timeout_hdl(srch_agent_t *agt)
{
    int ret;
    socket_t *sck;
    list_node_t *node;
    srch_conn_timeout_list_t timeout;
    
    memset(&timeout, 0, sizeof(timeout));

    /* 1. 创建内存池 */
    timeout.pool = mem_pool_creat(1 * KB);
    if (NULL == timeout.pool)
    {
        log_error(agt->log, "Create memory pool failed!");
        return SRCH_ERR;
    }

    timeout.ctm = agt->ctm;
    
    /* 2. 获取超时连接 */
    ret = rbt_trav(agt->connections,
            (rbt_trav_cb_t)srch_agent_get_timeout_conn_list, (void *)&timeout);
    if (0 != ret)
    {
        log_error(agt->log, "Traverse hash table failed!");
    }

    log_debug(agt->log, "Timeout num:%d!", timeout.list.num);

    /* 3. 删除超时连接 */
    node = timeout.list.head;
    for (; NULL != node; node = node->next)
    {
        sck = (socket_t *)node->data;

        srch_agent_del_conn(agt, sck);
    }

    /* 4. 释放内存空间 */
    mem_pool_destroy(timeout.pool);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_add_conn
 **功    能: 添加新的连接
 **输入参数: 
 **     ctx: 全局信息
 **     agt: Agent对象
 **输出参数: NONE
 **返    回: Agent对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_agent_add_conn(srch_cntx_t *ctx, srch_agent_t *agt)
{
    time_t ctm = time(NULL);
    socket_t *sck;
    srch_add_sck_t *add;
    srch_agent_sck_data_t *data;
    struct epoll_event ev;

    while (1)
    {
        /* 1. 取数据 */
        add = queue_pop(ctx->connq[agt->tidx]);
        if (NULL == add)
        {
            return SRCH_OK;
        }

        /* 2. 申请SCK空间 */
        sck = slab_alloc(agt->slab, sizeof(socket_t));
        if (NULL == sck)
        {
            queue_dealloc(ctx->connq[agt->tidx], add);
            log_error(agt->log, "Alloc memory from slab failed!");
            return SRCH_ERR;
        }

        log_debug(agt->log, "Pop data! fd:%d addr:%p sck:%p", add->fd, add, sck);

        memset(sck, 0, sizeof(socket_t));

        data = slab_alloc(agt->slab, sizeof(srch_agent_sck_data_t));
        if (NULL == data)
        {
            slab_dealloc(agt->slab, sck);
            queue_dealloc(ctx->connq[agt->tidx], add);
            log_error(agt->log, "Alloc memory from slab failed!");
            return SRCH_ERR;
        }

        sck->data = data;

        /* 3. 设置SCK信息 */
        sck->fd = add->fd;
        ftime(&sck->crtm);          /* 创建时间 */
        sck->wrtm = sck->rdtm = ctm;/* 记录当前时间 */

        sck->recv.phase = SOCK_PHASE_RECV_INIT;
        sck->recv_cb = (socket_recv_cb_t)srch_agent_recv;  /* Recv回调函数 */
        sck->send_cb = (socket_send_cb_t)srch_agent_send;  /* Send回调函数*/

        data->serial = add->serial;

        queue_dealloc(ctx->connq[agt->tidx], add);  /* 释放连接队列空间 */

        /* 4. 插入红黑树中(以序列号为主键) */
        if (rbt_insert(agt->connections, data->serial, sck))
        {
            log_error(agt->log, "Insert into avl failed! fd:%d seq:%lu", sck->fd, data->serial);

            Close(sck->fd);
            slab_dealloc(agt->slab, sck->data);
            slab_dealloc(agt->slab, sck);
            return SRCH_ERR;
        }

        log_debug(agt->log, "Insert into avl success! fd:%d seq:%lu", sck->fd, data->serial);

        /* 5. 加入epoll监听(首先是接收客户端搜索请求, 所以设置EPOLLIN) */
        memset(&ev, 0, sizeof(ev));

        ev.data.ptr = sck;
        ev.events = EPOLLIN | EPOLLET; /* 边缘触发 */

        epoll_ctl(agt->ep_fd, EPOLL_CTL_ADD, sck->fd, &ev);
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_del_conn
 **功    能: 删除指定套接字
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.06 #
 ******************************************************************************/
static int srch_agent_del_conn(srch_agent_t *agt, socket_t *sck)
{
    void *addr;
    list_node_t *node;
    srch_agent_sck_data_t *data = sck->data;

    log_debug(agt->log, "Call %s()! fd:%d", __func__, sck->fd);

    /* 1. 将套接字从红黑树中剔除 */
    rbt_delete(agt->connections, data->serial, &addr);
    if (addr != sck)
    {
        log_fatal(agt->log, "Serior error! serial:%lu fd:%d addr:%p sck:%p",
                data->serial, sck->fd, addr, sck);
        abort();
    }

    /* 2. 释放套接字空间 */
    Close(sck->fd);
    for (;;)    /* 释放发送链表 */
    {
        node = list_remove_head(&data->send_list);
        if (NULL == node)
        {
            break;
        }

        slab_dealloc(agt->slab, node->data);
        slab_dealloc(agt->slab, node);
    }

    if (NULL != sck->recv.addr)
    {
        queue_dealloc(agt->ctx->recvq[agt->tidx], sck->recv.addr);
    }

    slab_dealloc(agt->slab, sck->data);
    slab_dealloc(agt->slab, sck);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_socket_cmp_cb
 **功    能: 哈希表中查找套接字信息的比较回调函数
 **输入参数:
 **     pkey: 主键
 **     data: 哈希表树中结点所挂载的数据
 **输出参数: NONE
 **返    回:
 **     1. =0: 相等
 **     2. <0: 小于
 **     3. >0: 大于
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
int srch_agent_socket_cmp_cb(const void *pkey, const void *data)
{
    uint64_t serial = *(const uint64_t *)pkey;
    const socket_t *sock = (const socket_t *)data;
    const srch_agent_sck_data_t *sock_data = sock->data;

    return (serial - sock_data->serial);
}

/******************************************************************************
 **函数名称: srch_agent_recv_head
 **功    能: 接收报头
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.01 #
 ******************************************************************************/
static int srch_agent_recv_head(srch_agent_t *agt, socket_t *sck)
{
    int n, left;
    socket_snap_t *recv = &sck->recv;
    srch_mesg_header_t *head;

    /* 1. 计算剩余字节 */
    left = sizeof(srch_mesg_header_t) - recv->off;

    /* 2. 接收报头数据 */
    while (1)
    {
        n = read(sck->fd, recv->addr + recv->off, left);
        if (n == left)
        {
            recv->off += n;
            break; /* 接收完毕 */
        }
        else if (n > 0)
        {
            recv->off += n;
            continue;
        }
        else if (0 == n)
        {
            log_info(agt->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                    errno, strerror(errno), sck->fd, n, left);
            return SRCH_SCK_CLOSE;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return SRCH_SCK_AGAIN; /* 等待下次事件通知 */
        }

        if (EINTR == errno)
        {
            continue; 
        }

        log_error(agt->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SRCH_ERR;
    }

    /* 3. 校验报头数据 */
    head = (srch_mesg_header_t *)sck->recv.addr;

    /* head->type = head->type; */
    /* head->flag = head->flag; */
    head->length = ntohs(head->length);
    head->mark = ntohl(head->mark);

    if (SRCH_MSG_MARK_KEY != head->mark)
    {
        log_error(agt->log, "Check head failed! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->length, head->flag, head->mark, SRCH_MSG_MARK_KEY);
        return SRCH_ERR;
    }

    log_info(agt->log, "Recv head success! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->length, head->flag, head->mark, SRCH_MSG_MARK_KEY);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_recv_body
 **功    能: 接收报体
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.02 #
 ******************************************************************************/
static int srch_agent_recv_body(srch_agent_t *agt, socket_t *sck)
{
    int n, left;
    socket_snap_t *recv = &sck->recv;
    srch_mesg_header_t *head = (srch_mesg_header_t *)recv->addr;

    /* 1. 接收报体 */
    while (1)
    {
        left = recv->total - recv->off;

        n = read(sck->fd, recv->addr + recv->off, left);
        if (n == left)
        {
            recv->off += n;
            break; /* 接收完毕 */
        }
        else if (n > 0)
        {
            recv->off += n;
            continue;
        }
        else if (0 == n)
        {
            log_info(agt->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                    errno, strerror(errno), sck->fd, n, left);
            return SRCH_SCK_CLOSE;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return SRCH_SCK_AGAIN;
        }

        if (EINTR == errno)
        {
            continue;
        }

        log_error(agt->log, "errmsg:[%d] %s!"
                " fd:%d type:%d length:%d n:%d total:%d offset:%d addr:%p",
                errno, strerror(errno), head->type,
                sck->fd, head->length, n, recv->total, recv->off, recv->addr);
        return SRCH_ERR;
    }

    log_trace(agt->log, "Recv body success! fd:%d type:%d length:%d total:%d off:%d",
            sck->fd, head->type, head->length, recv->total, recv->off);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_recv_post
 **功    能: 数据接收完毕，进行数据处理
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.21 #
 ******************************************************************************/
static int srch_agent_recv_post(srch_agent_t *agt, socket_t *sck)
{
    srch_agent_sck_data_t *data = (srch_agent_sck_data_t *)sck->data;

    /* 1. 自定义消息的处理 */
    if (SRCH_MSG_FLAG_USR == data->head->flag)
    {
        log_info(agt->log, "Push into user data queue!");

        return queue_push(agt->ctx->recvq[agt->tidx], sck->recv.addr);
    }

    /* TODO: 2. 系统消息的处理 */
    //return srch_sys_msg_hdl(agt, sck);
    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_agent_recv
 **功    能: 接收数据
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_agent_recv(srch_agent_t *agt, socket_t *sck)
{
    int ret;
    srch_cntx_t *ctx = agt->ctx;
    socket_snap_t *recv = &sck->recv;
    srch_agent_sck_data_t *data = (srch_agent_sck_data_t *)sck->data;

    for (;;)
    {
        switch (recv->phase)
        {
            /* 1. 分配空间 */
            case SOCK_PHASE_RECV_INIT:
            {
                recv->addr = queue_malloc(ctx->recvq[agt->tidx]);
                if (NULL == recv->addr)
                {
                    log_error(agt->log, "Alloc memory from queue failed!");
                    return SRCH_ERR;
                }

                log_info(agt->log, "Alloc memory from queue success!");

                data->head = (srch_mesg_header_t *)recv->addr;
                data->body = (void *)(data->head + 1);
                recv->off = 0;
                recv->total = sizeof(srch_mesg_header_t);

                /* 设置下步 */
                recv->phase = SOCK_PHASE_RECV_HEAD;

                goto RECV_HEAD;
            }
            /* 2. 接收报头 */
            case SOCK_PHASE_RECV_HEAD:
            {
            RECV_HEAD:
                ret = srch_agent_recv_head(agt, sck);
                switch (ret)
                {
                    case SRCH_OK:
                    {
                        if (data->head->length)
                        {
                            recv->phase = SOCK_PHASE_READY_BODY; /* 设置下步 */
                        }
                        else
                        {
                            recv->phase = SOCK_PHASE_RECV_POST; /* 设置下步 */
                            goto RECV_POST;
                        }
                        break;      /* 继续后续处理 */
                    }
                    case SRCH_SCK_AGAIN:
                    {
                        return ret; /* 下次继续处理 */
                    }
                    default:
                    {
                        queue_dealloc(ctx->recvq[agt->tidx], recv->addr);
                        recv->addr = NULL;
                        return ret; /* 异常情况 */
                    }
                }

                goto READY_BODY;
            }
            /* 3. 准备接收报体 */
            case SOCK_PHASE_READY_BODY:
            {
            READY_BODY:
                recv->total += data->head->length;

                /* 设置下步 */
                recv->phase = SOCK_PHASE_RECV_BODY;

                goto RECV_BODY;
            }
            /* 4. 接收报体 */
            case SOCK_PHASE_RECV_BODY:
            {
            RECV_BODY:
                ret = srch_agent_recv_body(agt, sck);
                switch (ret)
                {
                    case SRCH_OK:
                    {
                        recv->phase = SOCK_PHASE_RECV_POST; /* 设置下步 */
                        break;      /* 继续后续处理 */
                    }
                    case SRCH_SCK_AGAIN:
                    {
                        return ret; /* 下次继续处理 */
                    }
                    default:
                    {
                        queue_dealloc(ctx->recvq[agt->tidx], recv->addr);
                        recv->addr = NULL;
                        return ret; /* 异常情况 */
                    }
                }

                goto RECV_POST;
            }
            /* 5. 接收完毕: 数据处理 */
            case SOCK_PHASE_RECV_POST:
            {
            RECV_POST:
                /* 将数据放入接收队列 */
                ret = srch_agent_recv_post(agt, sck);
                switch (ret)
                {
                    case SRCH_OK:
                    {
                        recv->phase = SOCK_PHASE_RECV_INIT;
                        recv->addr = NULL;
                        continue; /* 接收下一条数据 */
                    }
                    default:
                    {
                        queue_dealloc(ctx->recvq[agt->tidx], recv->addr);
                        recv->addr = NULL;
                        return SRCH_ERR;
                    }
                }
                return SRCH_ERR;
            }
        }
    }

    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_agent_fetch_send_data
 **功    能: 取发送数据
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     从链表中取出需要发送的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.22 #
 ******************************************************************************/
static void *srch_agent_fetch_send_data(srch_agent_t *agt, socket_t *sck)
{
    void *addr;
    list_node_t *node;
    srch_agent_sck_data_t *data = sck->data;

    node = list_remove_head(&data->send_list);
    if (NULL == node)
    {
        return NULL;
    }

    addr = node->data;

    slab_dealloc(agt->slab, node);

    return addr;
}

/******************************************************************************
 **函数名称: srch_agent_send
 **功    能: 发送数据
 **输入参数:
 **     agt: Agent对象
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_agent_send(srch_agent_t *agt, socket_t *sck)
{
    int n, left;
    srch_mesg_header_t *head;
    socket_snap_t *send = &sck->send;

    sck->wrtm = time(NULL);

    for (;;)
    {
        /* 1. 取发送的数据 */
        if (NULL == send->addr)
        {
            send->addr = srch_agent_fetch_send_data(agt, sck);
            if (NULL == send->addr)
            {
                return SRCH_OK; /* 无数据 */
            }

            head = (srch_mesg_header_t *)send->addr;

            send->off = 0;
            send->total = head->length + sizeof(srch_mesg_header_t);
        }

        /* 2. 发送数据 */
        left = send->total - send->off;

        n = Writen(sck->fd, send->addr+send->off, left);
        if (n != left)
        {
            if (n > 0)
            {
                send->off += n;
                return SRCH_SCK_AGAIN;
            }

            log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));

            /* 释放空间 */
            slab_dealloc(agt->slab, send->addr);
            send->addr = NULL;
            return SRCH_ERR;
        }

        /* 3. 释放空间 */
        slab_dealloc(agt->slab, send->addr);
        send->addr = NULL;
    }

    return SRCH_ERR;
}

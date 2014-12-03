#include "search.h"
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "hash.h"
#include "common.h"
#include "syscall.h"
#include "xml_tree.h"
#include "srch_agent.h"
#include "xds_socket.h"
#include "thread_pool.h"

static srch_agent_t *srch_agent_get(srch_cntx_t *ctx);
static int srch_agent_add_conn(srch_cntx_t *ctx, srch_agent_t *agt);
static int srch_agent_socket_cmp_cb(const void *pkey, const void *data);

static int srch_agent_recv(srch_agent_t *agt, socket_t *sck);
static int srch_agent_send(srch_agent_t *agt, socket_t *sck);

static int srch_agent_event_hdl(srch_agent_t *agt);
static int srch_agent_timeout_hdl(srch_agent_t *agt);

/******************************************************************************
 **函数名称: srch_agent_routine
 **功    能: 运行代理线程
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
        if (queue_space(&ctx->connq[agt->tidx]->queue))
        {
            if (srch_agent_add_conn(ctx, agt))
            {
                log_error(agt->log, "Fetch new connection failed!");
            }
        }

        /* 3. 等待事件通知 */
        agt->fds = epoll_wait(agt->ep_fd, agt->events,
                SRCH_AGENT_EVENT_MAX_NUM, SRCH_TMOUT_SEC);
        if (agt->fds < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == agt->fds)
        {
            /* 超时处理 */
            srch_agent_timeout_hdl(agt);
            continue;
        }

        /* 3. 处理事件通知 */
        srch_agent_event_hdl(agt);
    }

    return NULL;
}

/******************************************************************************
 **函数名称: srch_agent_init
 **功    能: 初始化代理线程
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

    /* 2. 创建套接字管理表 */
    agt->sock_tab = hash_tab_creat(
            SRCH_AGENT_SCK_HASH_MOD,
            hash_time33_ex,
            srch_agent_socket_cmp_cb);
    if (NULL == agt->sock_tab)
    {
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
 **功    能: 销毁代理线程
 **输入参数:
 **     agent: 接收对象
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
    time_t ctm = time(NULL);

    /* 1. 依次遍历套接字, 判断是否可读可写 */
    for (idx=0; idx<agt->fds; ++idx)
    {
        sck = (socket_t *)agt->events[idx].data.ptr;

        /* 1.1 判断是否可读 */
        if (agt->events[idx].events & EPOLLIN)
        {
            /* 接收网络数据 */
            while (SRCH_OK == (ret = sck->recv_cb(agt, sck))) { NULL; }
            if (SRCH_SCK_AGAIN != ret)
            {
                /* 异常-关闭SCK: 不必判断是否可写 */
                continue; 
            }
        }

        /* 1.2 判断是否可写 */
        if (agt->events[idx].events & EPOLLOUT)
        {
            /* 发送网络数据 */
            while (SRCH_OK == sck->send_cb(agt, sck)) { NULL; }
            if (SRCH_SCK_AGAIN != ret)
            {
                continue; /* 异常: 套接字已关闭 */
            }
        }
    }

    /* 2. 超时扫描 */
    if (ctm - agt->scan_tm > SRCH_TMOUT_SCAN_SEC)
    {
        agt->scan_tm = ctm;

        srch_agent_timeout_hdl(agt);
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_timeout_hdl
 **功    能: 事件超时处理
 **输入参数: 
 **     ctx: 全局信息
 **     r: 
 **输出参数: NONE
 **返    回: Agent对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int srch_agent_timeout_hdl(srch_agent_t *agt)
{
#if 0
    time_t ctm = time(NULL);
    list_node_t *node, *next;
    socket_t *sck;
    srch_agent_sck_data_t *data;

    /* 1. 依次遍历套接字, 判断是否超时 */
    node = agt->sock_list.head;
    for (; NULL != node; node = next)
    {
        next = node->next;

        sck = (socket_t *)node->data;
        if (NULL == sck)
        {
            continue;
        }

        data = (srch_agent_sck_data_t *)sck->data;

        /* 超时未发送或接收数据时, 认为无数据传输, 将直接关闭套接字 */
        if ((ctm - sck->rdtm <= 15)
            || (ctm - sck->wrtm <= 15))
        {
            continue; /* 未超时 */
        }

        log_error(agt->log, "Timeout!");

       srch_agent_remove_sock(agt, sck);
    }
#endif
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_add_conn
 **功    能: 添加新的连接
 **输入参数: 
 **     ctx: 全局信息
 **     r: Agent对象
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

    /* 1. 取数据 */
    add = lqueue_trypop(ctx->connq[agt->tidx]);
    if (NULL == add)
    {
        return SRCH_OK;
    }

    /* 2. 申请SCK空间 */
    sck = slab_alloc(agt->slab, sizeof(socket_t));
    if (NULL == sck)
    {
        lqueue_mem_dealloc(ctx->connq[agt->tidx], add);
        log_error(agt->log, "Alloc memory from slab failed!");
        return SRCH_ERR;
    }

    data = slab_alloc(agt->slab, sizeof(srch_agent_sck_data_t));
    if (NULL == data)
    {
        slab_dealloc(agt->slab, sck);
        lqueue_mem_dealloc(ctx->connq[agt->tidx], add);
        log_error(agt->log, "Alloc memory from slab failed!");
        return SRCH_ERR;
    }

    sck->data = data;

    /* 3. 设置SCK信息 */
    sck->fd = add->fd;
    ftime(&sck->crtm);          /* 创建时间 */
    sck->wrtm = sck->rdtm = ctm;/* 记录当前时间 */
    sck->recv_cb = (socket_recv_cb_t)srch_agent_recv;  /* Recv回调函数 */
    sck->send_cb = (socket_send_cb_t)srch_agent_send;  /* Send回调函数*/

    data->sck_serial = add->sck_serial;

    lqueue_mem_dealloc(ctx->connq[agt->tidx], add);

    /* 4. 哈希表中(以序列号为主键) */
    if (hash_tab_insert(agt->sock_tab, &data->sck_serial, sizeof(data->sck_serial), sck))
    {
        Close(sck->fd);
        slab_dealloc(agt->slab, sck->data);
        slab_dealloc(agt->slab, sck);
        log_error(agt->log, "Insert into hash table failed!");
        return SRCH_ERR;
    }

    /* 5. 加入epoll监听(首先是接收客户端搜索请求, 所以设置EPOLLIN) */
    memset(&ev, 0, sizeof(ev));

    ev.data.ptr = sck;
    ev.events = EPOLLIN | EPOLLET; /* 边缘触发 */

    epoll_ctl(agt->ep_fd, EPOLL_CTL_ADD, sck->fd, &ev);

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
static int srch_agent_socket_cmp_cb(const void *pkey, const void *data)
{
    uint64_t sck_serial = *(const uint64_t *)pkey;
    const socket_t *sock = (const socket_t *)data;
    const srch_agent_sck_data_t *sock_data = sock->data;

    return (sck_serial - sock_data->sck_serial);
}

/******************************************************************************
 **函数名称: srch_agent_init_head
 **功    能: 为接收报头做准备
 **输入参数:
 **     agent: Agent对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.01 #
 ******************************************************************************/
static int srch_agent_init_head(srch_agent_t *agt, socket_t *sck)
{
    srch_agent_sck_data_t *data = sck->data;

    data->header = lqueue_mem_alloc(
            agt->ctx->recvq[agt->tidx], sizeof(srch_msg_header_t));
    if (NULL == data->header)
    {
        log_error(agt->log, "Alloc memory from queue failed!");
        return SRCH_ERR;
    }

    sck->read.addr = (void *)data->header;
    sck->read.off = 0;
    sck->read.total = sizeof(srch_msg_header_t);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_recv_header
 **功    能: 接收报头
 **输入参数:
 **     agent: Agent对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.01 #
 ******************************************************************************/
static int srch_agent_recv_header(srch_agent_t *agt, socket_t *sck)
{
    int n, left;
    socket_snap_t *read = &sck->read;
    srch_msg_header_t *head;

    /* 1. 计算剩余字节 */
    left = sizeof(srch_msg_header_t) - read->off;

    /* 2. 接收报头数据 */
    n = Readn(sck->fd, read->addr + read->off, left);
    if (n != left)
    {
        if (n > 0)
        {
            read->off += n;
            return SRCH_SCK_AGAIN;
        }
        else if (0 == n)
        {
           log_info(agt->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                errno, strerror(errno), sck->fd, n, left);
            return SRCH_SCK_CLOSE;
        }

        log_error(agt->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SRCH_ERR;
    }
    
    read->off += n;

    /* 3. 校验报头数据 */
    head = (srch_msg_header_t *)sck->read.addr;
    if (SRCH_MSG_MARK_KEY != head->mark)
    {
        log_error(agt->log, "Check header failed! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->length, head->flag, head->mark, SRCH_MSG_MARK_KEY);
        return SRCH_ERR;
    }

    read->total = head->length;

    log_info(agt->log, "Recv header success! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->length, head->flag, head->mark, SRCH_MSG_MARK_KEY);

    return SRCH_DONE;
}

/******************************************************************************
 **函数名称: srch_agent_init_body
 **功    能: 为接收报体做准备
 **输入参数:
 **     agent: Agent对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.02 #
 ******************************************************************************/
static int srch_agent_init_body(srch_agent_t *agt, socket_t *sck)
{
    srch_agent_sck_data_t *data = sck->data;

    data->header->body = lqueue_mem_alloc(
            agt->ctx->recvq[agt->tidx], data->header->length);
    if (NULL == data->header->body)
    {
        log_error(agt->log, "Alloc memory from queue failed!");
        return SRCH_ERR;
    }

    sck->read.addr = (void *)data->header->body;
    sck->read.off = 0;
    sck->read.total = data->header->length;

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_recv_body
 **功    能: 接收报体
 **输入参数:
 **     agent: Agent对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.02 #
 ******************************************************************************/
static int srch_agent_recv_body(srch_agent_t *agt, socket_t *sck)
{
    int n, left;
    socket_snap_t *read = &sck->read;
    srch_msg_header_t *head = (srch_msg_header_t *)read->addr;

    /* 1. 接收报体 */
    left = head->length - read->off;

    n = Readn(sck->fd, read->addr + read->off, left);
    if (n != left)
    {
        if (n > 0)
        {
            read->off += n;
            return SRCH_SCK_AGAIN;
        }
        else if (0 == n)
        {
            if (EAGAIN == errno)
            {
                return SRCH_SCK_AGAIN; /* 接完头部时, 缓存中正好无数据... */
            }

            log_info(agt->log, "Client disconnected. errmsg:[%d] %s! "
                "fd:[%d] type:[%d] flag:[%d] bodylen:[%d] total:[%d] left:[%d] offset:[%d]",
                errno, strerror(errno),
                sck->fd, head->type, head->flag,
                head->length, read->total, left, read->off);
            return SRCH_SCK_CLOSE;
        }

        log_error(agt->log, "errmsg:[%d] %s!"
                " fd:%d type:%d length:%d n:%d total:%d offset:%d addr:%p",
                errno, strerror(errno), head->type,
                sck->fd, head->length, n, read->total, read->off, read->addr);
        return SRCH_ERR;
    }

    /* 2. Set flag variables */
    read->off += n;
    read->phase = SOCK_PHASE_RECV_POST;

    log_trace(agt->log, "fd:%d type:%d length:%d total:%d off:%d",
            head->type, sck->fd, head->length, read->total, read->off);

    return SRCH_DONE;
}

/******************************************************************************
 **函数名称: srch_agent_recv_post
 **功    能: 接收完毕的处理
 **输入参数:
 **     agent: Agent对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.02 #
 ******************************************************************************/
static int srch_agent_recv_post(srch_agent_t *agt, socket_t *sck)
{
    return SRCH_DONE;
}

/******************************************************************************
 **函数名称: srch_agent_recv
 **功    能: 接收数据
 **输入参数:
 **     ctx: 全局信息
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 为报头分配空间
 **     2. 接收报头
 **     3. 为报体分配空间
 **     4. 接收报体
 **     5. 对报文进行处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_agent_recv(srch_agent_t *agt, socket_t *sck)
{
    int ret;
    socket_snap_t *read = &sck->read;

    switch (read->phase)
    {
        case SOCK_PHASE_INIT_HEAD:  /* 分配报头空间 */
        {
            if (srch_agent_init_head(agt, sck))
            {
                log_error(agt->log, "Alloc memory for header failed!");
                return SRCH_ERR;
            }

            read->phase = SOCK_PHASE_RECV_HEAD; /* 设置下步 */
            /* # 继续后续处理 # 不用BREAK # */
        }
        case SOCK_PHASE_RECV_HEAD:  /* 接收报头 */
        {
            ret = srch_agent_recv_header(agt, sck);
            if (SRCH_DONE == ret)
            {
                /* 继续后续处理 */
            }
            else
            {
                return ret; /* 其他情况都返回给上一层处理 */
            }

            read->phase = SOCK_PHASE_INIT_BODY;
            /* # 继续后续处理 # 不用BREAK # */
        }
        case SOCK_PHASE_INIT_BODY:  /* 分配报体空间 */
        {
            if (srch_agent_init_body(agt, sck))
            {
                log_error(agt->log, "Alloc memory for body failed!");
                return SRCH_ERR;
            }

            read->phase = SOCK_PHASE_RECV_BODY;
            /* # 继续后续处理 # 不用BREAK # */
        }
        case SOCK_PHASE_RECV_BODY:  /* 接收报体 */
        {
            ret = srch_agent_recv_body(agt, sck);
            if (SRCH_DONE == ret)
            {
                /* 继续后续处理 */
            }
            else
            {
                return ret; /* 其他情况都返回给上一层处理 */
            }

            read->phase = SOCK_PHASE_RECV_POST;
            /* # 继续后续处理 # 不用BREAK # */
        }
        case SOCK_PHASE_RECV_POST:  /* 接收完毕: 数据处理 */
        {
            return srch_agent_recv_post(agt, sck);
        }
    }
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_agent_send
 **功    能: 发送数据
 **输入参数:
 **     ctx: 全局信息
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_agent_send(srch_agent_t *agt, socket_t *sck)
{
    return SRCH_OK;
}



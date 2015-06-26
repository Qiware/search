#include "sck.h"
#include "comm.h"
#include "hash.h"
#include "list.h"
#include "mesg.h"
#include "agent.h"
#include "command.h"
#include "syscall.h"
#include "xml_tree.h"
#include "agent_rsvr.h"
#include "thread_pool.h"

#define AGT_RSVR_DIST_POP_NUM   (128)

static agent_rsvr_t *agent_rsvr_self(agent_cntx_t *ctx);
static int agent_rsvr_add_conn(agent_cntx_t *ctx, agent_rsvr_t *rsvr);
static int agent_rsvr_del_conn(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck);

static int agent_rsvr_recv(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck);
static int agent_rsvr_send(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck);

static int agent_rsvr_dist_send_data(agent_cntx_t *ctx, agent_rsvr_t *rsvr);

static int agent_rsvr_event_hdl(agent_cntx_t *ctx, agent_rsvr_t *rsvr);
static int agent_rsvr_event_timeout_hdl(agent_cntx_t *ctx, agent_rsvr_t *rsvr);

/******************************************************************************
 **函数名称: agent_rsvr_routine
 **功    能: 运行Agent线程
 **输入参数:
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
void *agent_rsvr_routine(void *_ctx)
{
    agent_rsvr_t *rsvr;
    agent_cntx_t *ctx = (agent_cntx_t *)_ctx;

    /* 1. 获取代理对象 */
    rsvr = agent_rsvr_self(ctx);
    if (NULL == rsvr)
    {
        log_error(rsvr->log, "Get agent failed!");
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    while (1)
    {
        /* 3. 等待事件通知 */
        rsvr->fds = epoll_wait(rsvr->epid, rsvr->events,
                AGENT_EVENT_MAX_NUM, AGENT_TMOUT_MSEC);
        if (rsvr->fds < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            /* 异常情况 */
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == rsvr->fds)
        {
            rsvr->ctm = time(NULL);
            if (rsvr->ctm - rsvr->scan_tm > AGENT_TMOUT_SCAN_SEC)
            {
                rsvr->scan_tm = rsvr->ctm;

                agent_rsvr_event_timeout_hdl(ctx, rsvr);
            }
            continue;
        }

        /* 4. 处理事件通知 */
        agent_rsvr_event_hdl(ctx, rsvr);
    }

    return NULL;
}

/* 命令接收处理 */
static int agt_rsvr_recv_cmd_hdl(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck)
{
    cmd_data_t cmd;

    while (1)
    {
        /* > 接收命令 */
        if (unix_udp_recv(sck->fd, &cmd, sizeof(cmd)) < 0)
        {
            return AGENT_SCK_AGAIN;
        }

        /* > 处理命令 */
        switch (cmd.type)
        {
            case CMD_ADD_SCK:
            {
                if (agent_rsvr_add_conn(ctx, rsvr))
                {
                    log_error(rsvr->log, "Add connection failed！");
                }
                break;
            }
            case CMD_DIST_DATA:
            {
                if (agent_rsvr_dist_send_data(ctx, rsvr))
                {
                    log_error(rsvr->log, "Disturibute data failed！");
                }
                break;
            }
            default:
            {
                log_error(rsvr->log, "Unknown command type [%d]！", cmd.type);
                break;
            }
        }
    }
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_init
 **功    能: 初始化Agent线程
 **输入参数:
 **     ctx: 全局信息
 **     rsvr: 接收对象
 **     idx: 线程索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
int agent_rsvr_init(agent_cntx_t *ctx, agent_rsvr_t *rsvr, int idx)
{
    rbt_opt_t opt;
    struct epoll_event ev;
    char path[FILE_NAME_MAX_LEN];
    agent_conf_t *conf = ctx->conf;
    socket_t *cmd_sck = &rsvr->cmd_sck;

    rsvr->tidx = idx;
    rsvr->log = ctx->log;

    /* > 创建SLAB内存池 */
    rsvr->slab = slab_creat_by_calloc(AGENT_SLAB_SIZE, rsvr->log);
    if (NULL == rsvr->slab)
    {
        log_error(rsvr->log, "Initialize slab pool failed!");
        return AGENT_ERR;
    }

    /* > 创建连接红黑树 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = rsvr->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    rsvr->connections = rbt_creat(&opt);
    if (NULL == rsvr->connections)
    {
        log_error(rsvr->log, "Create socket hash table failed!");
        return AGENT_ERR;
    }

    do
    {
        /* > 创建epoll对象 */
        rsvr->epid = epoll_create(AGENT_EVENT_MAX_NUM);
        if (rsvr->epid < 0)
        {
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        rsvr->events = slab_alloc(rsvr->slab,
                AGENT_EVENT_MAX_NUM * sizeof(struct epoll_event));
        if (NULL == rsvr->events)
        {
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建附加信息 */
        cmd_sck->extra = slab_alloc(rsvr->slab, sizeof(agent_socket_extra_t));
        if (NULL == cmd_sck->extra)
        {
            log_error(rsvr->log, "Alloc from slab failed!");
            break;
        }

        /* > 创建命令套接字 */
        agent_rsvr_cmd_usck_path(conf, rsvr->tidx, path, sizeof(path));

        cmd_sck->fd = unix_udp_creat(path);
        if (cmd_sck->fd < 0)
        {
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        ftime(&cmd_sck->crtm);
        cmd_sck->wrtm = cmd_sck->rdtm = cmd_sck->crtm.time;
        cmd_sck->recv_cb = (socket_recv_cb_t)agt_rsvr_recv_cmd_hdl;
        cmd_sck->send_cb = NULL;

        /* > 加入事件侦听 */
        memset(&ev, 0, sizeof(ev));

        ev.data.ptr = cmd_sck;
        ev.events = EPOLLIN | EPOLLET; /* 边缘触发 */

        epoll_ctl(rsvr->epid, EPOLL_CTL_ADD, cmd_sck->fd, &ev);

        return AGENT_OK;
    } while(0);

    agent_rsvr_destroy(rsvr);
    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_rsvr_destroy
 **功    能: 销毁Agent线程
 **输入参数:
 **     rsvr: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次释放所有内存空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int agent_rsvr_destroy(agent_rsvr_t *rsvr)
{
    slab_dealloc(rsvr->slab, rsvr->events);
    rbt_destroy(rsvr->connections);
    free(rsvr->slab);
    CLOSE(rsvr->epid);
    CLOSE(rsvr->cmd_sck.fd);
    slab_dealloc(rsvr->slab, rsvr->cmd_sck.extra);
    slab_destroy(rsvr->slab);
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_self
 **功    能: 获取代理对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 代理对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.26 #
 ******************************************************************************/
static agent_rsvr_t *agent_rsvr_self(agent_cntx_t *ctx)
{
    int tidx;
    agent_rsvr_t *rsvr;

    tidx = thread_pool_get_tidx(ctx->agents);
    if (tidx < 0)
    {
        return NULL;
    }

    rsvr = thread_pool_get_args(ctx->agents);

    return rsvr + tidx;
}

/******************************************************************************
 **函数名称: agent_rsvr_event_hdl
 **功    能: 事件通知处理
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 代理对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int agent_rsvr_event_hdl(agent_cntx_t *ctx, agent_rsvr_t *rsvr)
{
    int idx, ret;
    socket_t *sck;

    rsvr->ctm = time(NULL);

    /* 1. 依次遍历套接字, 判断是否可读可写 */
    for (idx=0; idx<rsvr->fds; ++idx)
    {
        sck = (socket_t *)rsvr->events[idx].data.ptr;

        /* 1.1 判断是否可读 */
        if (rsvr->events[idx].events & EPOLLIN)
        {
            /* 接收网络数据 */
            ret = sck->recv_cb(ctx, rsvr, sck);
            if (AGENT_SCK_AGAIN != ret)
            {
                log_info(rsvr->log, "Delete connection! fd:%d", sck->fd);
                agent_rsvr_del_conn(ctx, rsvr, sck);
                continue; /* 异常-关闭SCK: 不必判断是否可写 */
            }
        }

        /* 1.2 判断是否可写 */
        if (rsvr->events[idx].events & EPOLLOUT)
        {
            /* 发送网络数据 */
            ret = sck->send_cb(ctx, rsvr, sck);
            if (AGENT_ERR == ret)
            {
                log_info(rsvr->log, "Delete connection! fd:%d", sck->fd);
                agent_rsvr_del_conn(ctx, rsvr, sck);
                continue; /* 异常: 套接字已关闭 */
            }
        }
    }

    /* 2. 超时扫描 */
    if (rsvr->ctm - rsvr->scan_tm > AGENT_TMOUT_SCAN_SEC)
    {
        rsvr->scan_tm = rsvr->ctm;

        agent_rsvr_event_timeout_hdl(ctx, rsvr);
    }

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_get_timeout_conn_list
 **功    能: 将超时连接加入链表
 **输入参数: 
 **     node: 平衡二叉树结点
 **     timeout: 超时链表
 **输出参数: NONE
 **返    回: 代理对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.24 #
 ******************************************************************************/
static int agent_rsvr_get_timeout_conn_list(socket_t *sck, agent_conn_timeout_list_t *timeout)
{
#define AGENT_SCK_TIMEOUT_SEC (180)

    /* 判断是否超时，则加入到timeout链表中 */
    if ((timeout->ctm - sck->rdtm <= AGENT_SCK_TIMEOUT_SEC)
        || (timeout->ctm - sck->wrtm <= AGENT_SCK_TIMEOUT_SEC))
    {
        return AGENT_OK; /* 未超时 */
    }

    return list_lpush(timeout->list, sck);
}

/******************************************************************************
 **函数名称: agent_rsvr_conn_timeout
 **功    能: 删除超时连接
 **输入参数: 
 **     ctx: 全局信息
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.11 15:02:33 #
 ******************************************************************************/
static int agent_rsvr_conn_timeout(agent_cntx_t *ctx, agent_rsvr_t *rsvr)
{
    socket_t *sck;
    list_opt_t opt;
    agent_conn_timeout_list_t timeout;

    memset(&timeout, 0, sizeof(timeout));

    /* > 创建内存池 */
    timeout.pool = mem_pool_creat(1 * KB);
    if (NULL == timeout.pool)
    {
        log_error(rsvr->log, "Create memory pool failed!");
        return AGENT_ERR;
    }

    timeout.ctm = rsvr->ctm;

    do
    {
        /* > 创建链表 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)timeout.pool;
        opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

        timeout.list = list_creat(&opt);
        if (NULL == timeout.list)
        {
            log_error(rsvr->log, "Create list failed!");
            break;
        }

        /* > 获取超时连接 */
        rbt_trav(rsvr->connections,
             (rbt_trav_cb_t)agent_rsvr_get_timeout_conn_list, (void *)&timeout);

        log_debug(rsvr->log, "Timeout connections: %d!", timeout.list->num);

        /* > 删除超时连接 */
        for (;;)
        {
            sck = (socket_t *)list_lpop(timeout.list);
            if (NULL == sck)
            {
                break;
            }

            agent_rsvr_del_conn(ctx, rsvr, sck);
        }
    } while(0);

    /* > 释放内存空间 */
    mem_pool_destroy(timeout.pool);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_event_timeout_hdl
 **功    能: 事件超时处理
 **输入参数: 
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     不必依次释放超时链表各结点的空间，只需一次性释放内存池便可释放所有空间.
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int agent_rsvr_event_timeout_hdl(agent_cntx_t *ctx, agent_rsvr_t *rsvr)
{
    agent_rsvr_conn_timeout(ctx, rsvr);
    agent_serial_to_sck_map_timeout(ctx);
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_add_conn
 **功    能: 添加新的连接
 **输入参数: 
 **     ctx: 全局信息
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int agent_rsvr_add_conn(agent_cntx_t *ctx, agent_rsvr_t *rsvr)
{
    time_t ctm = time(NULL);
    socket_t *sck;
    list_opt_t opt;
    agent_add_sck_t *add;
    struct epoll_event ev;
    agent_socket_extra_t *extra;

    while (1)
    {
        /* > 取数据 */
        add = queue_pop(ctx->connq[rsvr->tidx]);
        if (NULL == add)
        {
            return AGENT_OK;
        }

        /* > 申请SCK空间 */
        sck = slab_alloc(rsvr->slab, sizeof(socket_t));
        if (NULL == sck)
        {
            queue_dealloc(ctx->connq[rsvr->tidx], add);
            log_error(rsvr->log, "Alloc memory from slab failed!");
            return AGENT_ERR;
        }

        log_debug(rsvr->log, "Pop data! fd:%d addr:%p sck:%p", add->fd, add, sck);

        memset(sck, 0, sizeof(socket_t));

        /* > 创建SCK关联对象 */
        extra = slab_alloc(rsvr->slab, sizeof(agent_socket_extra_t));
        if (NULL == extra)
        {
            slab_dealloc(rsvr->slab, sck);
            queue_dealloc(ctx->connq[rsvr->tidx], add);
            log_error(rsvr->log, "Alloc memory from slab failed!");
            return AGENT_ERR;
        }

        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)rsvr->slab;
        opt.alloc = (mem_alloc_cb_t)slab_alloc;
        opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

        extra->send_list = list_creat(&opt);
        if (NULL == extra->send_list)
        {
            slab_dealloc(rsvr->slab, sck);
            slab_dealloc(rsvr->slab, extra);
            queue_dealloc(ctx->connq[rsvr->tidx], add);
            log_error(rsvr->log, "Alloc memory from slab failed!");
            return AGENT_ERR;
        }

        sck->extra = extra;

        /* > 设置SCK信息 */
        sck->fd = add->fd;
        ftime(&sck->crtm);          /* 创建时间 */
        sck->wrtm = sck->rdtm = ctm;/* 记录当前时间 */

        sck->recv.phase = SOCK_PHASE_RECV_INIT;
        sck->recv_cb = (socket_recv_cb_t)agent_rsvr_recv;  /* Recv回调函数 */
        sck->send_cb = (socket_send_cb_t)agent_rsvr_send;  /* Send回调函数*/

        extra->serial = add->serial;

        queue_dealloc(ctx->connq[rsvr->tidx], add);  /* 释放连接队列空间 */

        /* > 插入红黑树中(以序列号为主键) */
        if (rbt_insert(rsvr->connections, extra->serial, sck))
        {
            log_error(rsvr->log, "Insert into avl failed! fd:%d seq:%lu", sck->fd, extra->serial);

            CLOSE(sck->fd);
            list_destroy(extra->send_list, rsvr->slab, (mem_dealloc_cb_t)slab_dealloc);
            slab_dealloc(rsvr->slab, sck->extra);
            slab_dealloc(rsvr->slab, sck);
            return AGENT_ERR;
        }

        log_debug(rsvr->log, "Insert into avl success! fd:%d seq:%lu", sck->fd, extra->serial);

        /* > 加入epoll监听(首先是接收客户端搜索请求, 所以设置EPOLLIN) */
        memset(&ev, 0, sizeof(ev));

        ev.data.ptr = sck;
        ev.events = EPOLLIN | EPOLLET; /* 边缘触发 */

        epoll_ctl(rsvr->epid, EPOLL_CTL_ADD, sck->fd, &ev);
        ++rsvr->conn_total;
    }

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_rsvr_del_conn
 **功    能: 删除指定套接字
 **输入参数:
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次释放套接字对象各成员的空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.06 #
 ******************************************************************************/
static int agent_rsvr_del_conn(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck)
{
    void *addr;
    agent_socket_extra_t *extra = sck->extra;

    log_trace(rsvr->log, "Call %s()! fd:%d serial:%ld", __func__, sck->fd, extra->serial);

    /* > 将套接字从红黑树中剔除 */
    rbt_delete(rsvr->connections, extra->serial, &addr);

    /* > 释放套接字空间 */
    CLOSE(sck->fd);
    list_destroy(extra->send_list, rsvr->slab, (mem_dealloc_cb_t)slab_dealloc);
    if (sck->recv.addr)
    {
        queue_dealloc(ctx->recvq[rsvr->tidx], sck->recv.addr);
    }
    slab_dealloc(rsvr->slab, sck->extra);
    slab_dealloc(rsvr->slab, sck);

    --rsvr->conn_total;
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_recv_head
 **功    能: 接收报头
 **输入参数:
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.01 #
 ******************************************************************************/
static int agent_rsvr_recv_head(agent_rsvr_t *rsvr, socket_t *sck)
{
    void *addr;
    int n, left;
    agent_header_t *head;
    socket_snap_t *recv = &sck->recv;

    /* 1. 计算剩余字节 */
    left = sizeof(agent_header_t) - recv->off;

    addr = recv->addr + sizeof(agent_flow_t);

    /* 2. 接收报头数据 */
    while (1)
    {
        n = read(sck->fd, addr + recv->off, left);
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
            log_info(rsvr->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                    errno, strerror(errno), sck->fd, n, left);
            return AGENT_SCK_CLOSE;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return AGENT_SCK_AGAIN; /* 等待下次事件通知 */
        }

        if (EINTR == errno)
        {
            continue; 
        }

        log_error(rsvr->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return AGENT_ERR;
    }

    /* 3. 校验报头数据 */
    head = (agent_header_t *)addr;

    head->type = ntohl(head->type);
    head->flag = ntohl(head->flag);
    head->length = ntohl(head->length);
    head->mark = ntohl(head->mark);

    if (AGENT_MSG_MARK_KEY != head->mark)
    {
        log_error(rsvr->log, "Check head failed! type:%d len:%d flag:%d mark:[0x%X/0x%X]",
            head->type, head->length, head->flag, head->mark, AGENT_MSG_MARK_KEY);
        return AGENT_ERR;
    }

    log_trace(rsvr->log, "Recv head success! type:%d len:%d flag:%d mark:[0x%X/0x%X]",
            head->type, head->length, head->flag, head->mark, AGENT_MSG_MARK_KEY);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_recv_body
 **功    能: 接收报体
 **输入参数:
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.02 #
 ******************************************************************************/
static int agent_rsvr_recv_body(agent_rsvr_t *rsvr, socket_t *sck)
{
    void *addr;
    int n, left;
    agent_header_t *head;
    socket_snap_t *recv = &sck->recv;

    addr = recv->addr + sizeof(agent_flow_t);
    head  = (agent_header_t *)addr;

    /* 1. 接收报体 */
    while (1)
    {
        left = recv->total - recv->off;

        n = read(sck->fd, addr + recv->off, left);
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
            log_info(rsvr->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                    errno, strerror(errno), sck->fd, n, left);
            return AGENT_SCK_CLOSE;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return AGENT_SCK_AGAIN;
        }

        if (EINTR == errno)
        {
            continue;
        }

        log_error(rsvr->log, "errmsg:[%d] %s! fd:%d type:%d length:%d n:%d total:%d offset:%d addr:%p",
                errno, strerror(errno), head->type,
                sck->fd, head->length, n, recv->total, recv->off, recv->addr);
        return AGENT_ERR;
    }

    log_trace(rsvr->log, "Recv body success! fd:%d type:%d length:%d total:%d off:%d",
            sck->fd, head->type, head->length, recv->total, recv->off);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_sys_msg_hdl
 **功    能: 系统消息的处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 #
 ******************************************************************************/
static int agent_sys_msg_hdl(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck)
{
    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_rsvr_cmd_proc_req
 **功    能: 发送处理请求
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     wid: 工作线程ID(与rqid一致)
 **输出参数: NONE
 **返    回: >0:成功 <=0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.26 10:33:12 #
 ******************************************************************************/
static int agent_rsvr_cmd_proc_req(agent_cntx_t *ctx, agent_rsvr_t *rsvr, int widx)
{
    cmd_data_t cmd;
    char path[FILE_PATH_MAX_LEN];

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = CMD_PROC_DATA;

    agent_wsvr_cmd_usck_path(ctx->conf, widx, path, sizeof(path));

    /* > 发送处理命令 */
    return unix_udp_send(rsvr->cmd_sck.fd, path, &cmd, sizeof(cmd_data_t));
}

/******************************************************************************
 **函数名称: agent_rsvr_recv_post
 **功    能: 数据接收完毕，进行数据处理
 **输入参数:
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: TODO: 可向工作线程发送处理请求
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.21 #
 ******************************************************************************/
static int agent_rsvr_recv_post(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck)
{
    agent_socket_extra_t *extra = (agent_socket_extra_t *)sck->extra;

    /* 1. 自定义消息的处理 */
    if (AGENT_MSG_FLAG_USR == extra->head->flag)
    {
        log_info(rsvr->log, "Push into user data queue!");

        queue_push(ctx->recvq[rsvr->tidx], sck->recv.addr);
        agent_rsvr_cmd_proc_req(ctx, rsvr, rand()%ctx->conf->worker_num);
        return AGENT_OK;
    }

    /* 2. 系统消息的处理 */
    return agent_sys_msg_hdl(ctx, rsvr, sck);
}

/******************************************************************************
 **函数名称: agent_rsvr_recv
 **功    能: 接收数据
 **输入参数:
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: TODO: 此处理流程可进一步进行优化
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int agent_rsvr_recv(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck)
{
    int ret;
    socket_snap_t *recv = &sck->recv;
    static volatile uint64_t serial = 0;
    agent_socket_extra_t *extra = (agent_socket_extra_t *)sck->extra;

    for (;;)
    {
        switch (recv->phase)
        {
            /* 1. 分配空间 */
            case SOCK_PHASE_RECV_INIT:
            {
                recv->addr = queue_malloc(ctx->recvq[rsvr->tidx], queue_size(ctx->recvq[0]));
                if (NULL == recv->addr)
                {
                    log_error(rsvr->log, "Alloc from queue failed!");
                    return AGENT_ERR;
                }

                log_info(rsvr->log, "Alloc memory from queue success!");

                extra->flow = (agent_flow_t *)recv->addr;
                extra->head = (agent_header_t *)(extra->flow + 1);
                extra->body = (void *)(extra->head + 1);
                recv->off = 0;
                recv->total = sizeof(agent_header_t);

                extra->flow->sck_serial = extra->serial;
                extra->flow->agt_idx = rsvr->tidx;

                /* 设置下步 */
                recv->phase = SOCK_PHASE_RECV_HEAD;

                goto RECV_HEAD;
            }
            /* 2. 接收报头 */
            case SOCK_PHASE_RECV_HEAD:
            {
            RECV_HEAD:
                ret = agent_rsvr_recv_head(rsvr, sck);
                switch (ret)
                {
                    case AGENT_OK:
                    {
                        extra->flow = (agent_flow_t *)recv->addr;
                        extra->flow->serial = atomic64_inc(&serial); /* 获取流水号 */
                        extra->flow->create_tm = rsvr->ctm;

                        log_info(rsvr->log, "Call %s()! serial:%lu", __func__, extra->flow->serial);

                        if (extra->head->length)
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
                    case AGENT_SCK_AGAIN:
                    {
                        return ret; /* 下次继续处理 */
                    }
                    default:
                    {
                        queue_dealloc(ctx->recvq[rsvr->tidx], recv->addr);
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
                recv->total += extra->head->length;

                /* 设置下步 */
                recv->phase = SOCK_PHASE_RECV_BODY;

                goto RECV_BODY;
            }
            /* 4. 接收报体 */
            case SOCK_PHASE_RECV_BODY:
            {
            RECV_BODY:
                ret = agent_rsvr_recv_body(rsvr, sck);
                switch (ret)
                {
                    case AGENT_OK:
                    {
                        recv->phase = SOCK_PHASE_RECV_POST; /* 设置下步 */
                        break;      /* 继续后续处理 */
                    }
                    case AGENT_SCK_AGAIN:
                    {
                        return ret; /* 下次继续处理 */
                    }
                    default:
                    {
                        queue_dealloc(ctx->recvq[rsvr->tidx], recv->addr);
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
                ret = agent_rsvr_recv_post(ctx, rsvr, sck);
                switch (ret)
                {
                    case AGENT_OK:
                    {
                        recv->phase = SOCK_PHASE_RECV_INIT;
                        recv->addr = NULL;
                        continue; /* 接收下一条数据 */
                    }
                    default:
                    {
                        queue_dealloc(ctx->recvq[rsvr->tidx], recv->addr);
                        recv->addr = NULL;
                        return AGENT_ERR;
                    }
                }
                return AGENT_ERR;
            }
        }
    }

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_rsvr_send
 **功    能: 发送数据
 **输入参数:
 **     rsvr: 接收服务
 **     sck: SCK对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int agent_rsvr_send(agent_cntx_t *ctx, agent_rsvr_t *rsvr, socket_t *sck)
{
    int n, left;
    agent_header_t *head;
    struct epoll_event ev;
    socket_snap_t *send = &sck->send;
    agent_socket_extra_t *extra = (agent_socket_extra_t *)sck->extra;

    sck->wrtm = time(NULL);

    for (;;)
    {
        /* 1. 取发送的数据 */
        if (NULL == send->addr)
        {
            send->addr = list_lpop(extra->send_list);
            if (NULL == send->addr)
            {
                return AGENT_OK; /* 无数据 */
            }

            head = (agent_header_t *)send->addr;

            send->off = 0;
            send->total = head->length + sizeof(agent_header_t);
        }

        /* 2. 发送数据 */
        left = send->total - send->off;

        n = Writen(sck->fd, send->addr+send->off, left);
        if (n != left)
        {
            if (n > 0)
            {
                send->off += n;
                return AGENT_SCK_AGAIN;
            }

            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));

            /* 释放空间 */
            slab_dealloc(rsvr->slab, send->addr);
            send->addr = NULL;
            return AGENT_ERR;
        }

        /* 3. 释放空间 */
        slab_dealloc(rsvr->slab, send->addr);
        send->addr = NULL;

        /* > 设置epoll监听(将EPOLLOUT剔除) */
        if (list_isempty(extra->send_list))
        {
            memset(&ev, 0, sizeof(ev));

            ev.data.ptr = sck;
            ev.events = EPOLLIN | EPOLLET; /* 边缘触发 */

            epoll_ctl(rsvr->epid, EPOLL_CTL_MOD, sck->fd, &ev);
        }
        else
        {
            memset(&ev, 0, sizeof(ev));

            ev.data.ptr = sck;
            ev.events = EPOLLOUT | EPOLLET; /* 边缘触发 */

            epoll_ctl(rsvr->epid, EPOLL_CTL_MOD, sck->fd, &ev);
        }
    }

    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_rsvr_dist_send_data
 **功    能: 分发发送的数据
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-05 17:35:02 #
 ******************************************************************************/
static int agent_rsvr_dist_send_data(agent_cntx_t *ctx, agent_rsvr_t *rsvr)
{
    int total, num, idx;
    queue_t *sendq;
    socket_t *sck;
    rbt_node_t *node;
    agent_flow_t *flow;
    agent_flow_t newest;
    agent_header_t *head;
    struct epoll_event ev;
    agent_socket_extra_t *sck_extra;
    void *addr[AGT_RSVR_DIST_POP_NUM], *data;

    sendq = ctx->sendq[rsvr->tidx];
    while (1)
    {
        num = MIN(queue_used(sendq), AGT_RSVR_DIST_POP_NUM);
        if (0 == num)
        {
            break;
        }

        /* > 弹出应答数据 */
        num = queue_mpop(sendq, addr, num);
        if (0 == num)
        {
            break;
        }

        log_debug(rsvr->log, "Pop data succ! num:%d", num);

        for (idx=0; idx<num; ++idx)
        {
            flow = (agent_flow_t *)addr[idx]; // 流水信息
            head = (agent_header_t *)(flow + 1); // 消息头
            if (AGENT_MSG_MARK_KEY != head->mark)
            {
                log_error(ctx->log, "Check mark [0X%x/0X%x] failed! serial:%lu",
                        head->mark, AGENT_MSG_MARK_KEY, flow->serial);
                queue_dealloc(sendq, addr[idx]);
                continue;
            }

            total = head->length + sizeof(agent_header_t);

            /* > 查找最新SERIAL->SCK映射 */
            if (agent_serial_to_sck_map_query(ctx, flow->serial, &newest))
            {
                log_error(ctx->log, "Query serial->sck map failed! serial:%lu", flow->serial);
                queue_dealloc(sendq, addr[idx]);
                continue;
            }

            //agent_serial_to_sck_map_delete(ctx, flow->serial); /* 查询完成删除 */

            /* 校验映射项合法性 */
            if (flow->agt_idx != newest.agt_idx
                || flow->sck_serial != newest.sck_serial)
            {
                log_error(ctx->log, "Old socket was closed! serial:%lu", flow->sck_serial);
                queue_dealloc(sendq, addr[idx]);
                continue;
            }

            /* 放入发送链表 */
            node = rbt_search(rsvr->connections, (int64_t)flow->sck_serial);
            if (NULL == node
                || NULL == node->data)
            {
                log_error(ctx->log, "Query socket failed! serial:%lu", flow->sck_serial);
                queue_dealloc(sendq, addr[idx]);
                continue;
            }

            sck = (socket_t *)node->data;
            sck_extra = (agent_socket_extra_t *)sck->extra;

            data = slab_alloc(rsvr->slab, total);
            if (NULL == data)
            {
                log_error(ctx->log, "Alloc from slab failed! serial:%lu total:%d",
                        flow->sck_serial, total);
                queue_dealloc(sendq, addr[idx]);
                continue;
            }

            log_debug(rsvr->log, "Call %s()! type:%d len:%d!", __func__, head->type, head->length);

            memcpy(data, (void *)head, total);

            if (list_rpush(sck_extra->send_list, data))
            {
                log_error(ctx->log, "Insert list failed! serial:%lu", flow->sck_serial);
                queue_dealloc(sendq, addr[idx]);
                slab_dealloc(rsvr->slab, data);
                continue;
            }

            queue_dealloc(sendq, addr[idx]);

            /* > 设置epoll监听(添加EPOLLOUT) */
            memset(&ev, 0, sizeof(ev));

            ev.data.ptr = sck;
            ev.events = EPOLLOUT | EPOLLET; /* 边缘触发 */

            epoll_ctl(rsvr->epid, EPOLL_CTL_MOD, sck->fd, &ev);
        }
    }

    return AGENT_OK;
}

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: rttp.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/

#include "syscall.h"
#include "rttp_cmd.h"
#include "rttp_comm.h"
#include "rtrd_recv.h"
#include "thread_pool.h"

/* 静态函数 */
static rtrd_rsvr_t *rtrd_rsvr_get_curr(rtrd_cntx_t *ctx);
static int rtrd_rsvr_event_core_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);
static int rtrd_rsvr_event_timeout_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);

static int rtrd_rsvr_trav_recv(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);
static int rtrd_rsvr_trav_send(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);

static int rtrd_rsvr_recv_proc(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck);
static int rtrd_rsvr_data_proc(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck);

static int rtrd_rsvr_sys_mesg_proc(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck, void *addr);
static int rtrd_rsvr_exp_mesg_proc(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck, void *addr);

static int rtrd_rsvr_keepalive_req_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck);
static int rtrd_rsvr_link_auth_req_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck);

static int rtrd_rsvr_cmd_proc_req(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, int rqid);
static int rtrd_rsvr_cmd_proc_all_req(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);

static rtrd_sck_t *rtrd_rsvr_sck_creat(rtrd_rsvr_t *rsvr, rttp_cmd_add_sck_t *req);
static void rtrd_rsvr_sck_free(rtrd_rsvr_t *rsvr, rtrd_sck_t *sck);

static int rtrd_rsvr_add_conn_hdl(rtrd_rsvr_t *rsvr, rttp_cmd_add_sck_t *req);
static int rtrd_rsvr_del_conn_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, list2_node_t *node);

static int rtrd_rsvr_fill_send_buff(rtrd_rsvr_t *rsvr, rtrd_sck_t *sck);

static int rtrd_rsvr_dist_send_data(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr);

/* 随机选择接收线程 */
#define rttp_rand_recv(ctx) ((ctx)->listen.total++ % (ctx->recvtp->num))

/* 随机选择工作线程 */
#define rttp_rand_work(ctx) (rand() % (ctx->worktp->num))

/******************************************************************************
 **函数名称: rtrd_rsvr_set_rdset
 **功    能: 设置可读集合
 **输入参数:
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 如果超时未接收或发送数据，则关闭连接!
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static void rtrd_rsvr_set_rdset(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    rtrd_sck_t *curr;
    list2_node_t *node, *next, *tail;

    FD_ZERO(&rsvr->rdset);

    FD_SET(rsvr->cmd_sck_id, &rsvr->rdset);
    rsvr->max = rsvr->cmd_sck_id;

    node = rsvr->conn_list->head;
    if (NULL != node)
    {
        tail = node->prev;
    }
    while (NULL != node)
    {
        curr = (rtrd_sck_t *)node->data;
        if ((rsvr->ctm - curr->rdtm > 30)
            && (rsvr->ctm - curr->wrtm > 30))
        {
            log_error(rsvr->log, "Didn't active for along time! fd:%d ip:%s",
                    curr->fd, curr->ipaddr);

            if (node == tail)
            {
                rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
                break;
            }
            next = node->next;
            rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
            node = next;
            continue;
        }

        FD_SET(curr->fd, &rsvr->rdset);
        rsvr->max = MAX(rsvr->max, curr->fd);

        if (node == tail)
        {
            break;
        }
        node = node->next;
    }
}

/******************************************************************************
 **函数名称: rtrd_rsvr_set_wrset
 **功    能: 设置可写集合
 **输入参数:
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 只有发送链表中存在数据时，才将该套接字加入到可写侦听集合!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static void rtrd_rsvr_set_wrset(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    rtrd_sck_t *curr;
    list2_node_t *node, *tail;

    FD_ZERO(&rsvr->wrset);

    node = rsvr->conn_list->head;
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        curr = (rtrd_sck_t *)node->data;

        if (list_isempty(curr->mesg_list)
            && (curr->send.optr == curr->send.iptr))
        {
            if (node == tail)
            {
                break;
            }
            node = node->next;
            continue;
        }

        FD_SET(curr->fd, &rsvr->wrset);

        if (node == tail)
        {
            break;
        }
        node = node->next;
    }
}

/******************************************************************************
 **函数名称: rtrd_rsvr_routine
 **功    能: 运行接收服务线程
 **输入参数:
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 获取接收服务
 **     2. 等待事件通知
 **     3. 进行事件处理
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void *rtrd_rsvr_routine(void *_ctx)
{
    int ret;
    rtrd_rsvr_t *rsvr;
    struct timeval timeout;
    rtrd_cntx_t *ctx = (rtrd_cntx_t *)_ctx;

    /* 1. 获取接收服务 */
    rsvr = rtrd_rsvr_get_curr(ctx);
    if (NULL == rsvr)
    {
        log_fatal(rsvr->log, "Get recv server failed!");
        abort();
        return (void *)RTTP_ERR;
    }

    for (;;)
    {
        /* 2. 等待事件通知 */
        rtrd_rsvr_set_rdset(ctx, rsvr);
        rtrd_rsvr_set_wrset(ctx, rsvr);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        ret = select(rsvr->max+1, &rsvr->rdset, &rsvr->wrset, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)RTTP_ERR;
        }
        else if (0 == ret)
        {
            rtrd_rsvr_event_timeout_hdl(ctx, rsvr);
            continue;
        }

        /* 3. 进行事件处理 */
        rtrd_rsvr_event_core_hdl(ctx, rsvr);
    }

    log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_get_curr
 **功    能: 获取当前线程对应的接收服务
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 当前接收服务
 **实现描述:
 **     1. 获取当前线程的索引
 **     2. 返回当前线程对应的接收服务
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static rtrd_rsvr_t *rtrd_rsvr_get_curr(rtrd_cntx_t *ctx)
{
    int tidx;

    /* 1. 获取当前线程的索引 */
    tidx = thread_pool_get_tidx(ctx->recvtp);
    if (tidx < 0)
    {
        log_error(ctx->log, "Get index of current thread failed!");
        return NULL;
    }

    /* 2. 返回当前线程对应的接收服务 */
    return (rtrd_rsvr_t *)(ctx->recvtp->data + tidx * sizeof(rtrd_rsvr_t));
}

/******************************************************************************
 **函数名称: rtrd_rsvr_init
 **功    能: 初始化接收服务
 **输入参数:
 **     ctx: 全局对象
 **     tidx: 接收服务编号
 **输出参数:
 **     rsvr: 接收服务
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 获取当前线程的索引
 **     2. 返回当前线程对应的接收服务
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
int rtrd_rsvr_init(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, int tidx)
{
    list2_opt_t opt;
    char path[FILE_PATH_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;

    rsvr->tidx = tidx;
    rsvr->log = ctx->log;
    rsvr->ctm = time(NULL);

    /* > 创建CMD套接字 */
    rtrd_rsvr_usck_path(conf, path, rsvr->tidx);

    rsvr->cmd_sck_id = unix_udp_creat(path);
    if (rsvr->cmd_sck_id < 0)
    {
        log_error(rsvr->log, "Create unix-udp socket failed!");
        return RTTP_ERR;
    }

    /* > 创建SLAB内存池 */
    rsvr->pool = slab_creat_by_calloc(RTTP_MEM_POOL_SIZE, rsvr->log);
    if (NULL == rsvr->pool)
    {
        log_error(rsvr->log, "Initialize slab mem-pool failed!");
        CLOSE(rsvr->cmd_sck_id);
        return RTTP_ERR;
    }

    /* > 创建套接字链表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)rsvr->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    rsvr->conn_list = list2_creat(&opt);
    if (NULL == rsvr->conn_list)
    {
        log_error(rsvr->log, "Create list2 failed!");
        FREE(rsvr->pool);
        CLOSE(rsvr->cmd_sck_id);
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_recv_cmd
 **功    能: 接收命令数据
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 接收命令数据
 **     2. 进行命令处理
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_recv_cmd(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    rttp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令数据 */
    if (unix_udp_recv(rsvr->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(rsvr->log, "Recv command failed!");
        return RTTP_ERR_RECV_CMD;
    }

    /* 2. 进行命令处理 */
    switch (cmd.type)
    {
        case RTTP_CMD_ADD_SCK:      /* 添加套接字 */
        {
            return rtrd_rsvr_add_conn_hdl(rsvr, (rttp_cmd_add_sck_t *)&cmd.args);
        }
        case RTTP_CMD_DIST_REQ:     /* 分发发送数据 */
        {
            return rtrd_rsvr_dist_send_data(ctx, rsvr);
        }
        default:
        {
            log_error(rsvr->log, "Unknown command! type:%d", cmd.type);
            return RTTP_ERR_UNKNOWN_CMD;
        }
    }

    return RTTP_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_trav_recv
 **功    能: 遍历接收数据
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 遍历判断套接字是否可读，并接收数据!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_trav_recv(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    rtrd_sck_t *curr;
    list2_node_t *node, *next, *tail;

    rsvr->ctm = time(NULL);

    node = rsvr->conn_list->head;
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        curr = (rtrd_sck_t *)node->data;

        if (FD_ISSET(curr->fd, &rsvr->rdset))
        {
            curr->rdtm = rsvr->ctm;

            /* 进行接收处理 */
            if (rtrd_rsvr_recv_proc(ctx, rsvr, curr))
            {
                log_error(rsvr->log, "Recv proc failed! fd:%d ip:%s", curr->fd, curr->ipaddr);
                if (node == tail)
                {
                    rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
                    break;
                }
                next = node->next;
                rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
                node = next;
                continue;
            }
        }

        if (node == tail)
        {
            break;
        }

        node = node->next;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_trav_send
 **功    能: 遍历发送数据
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 遍历判断套接字是否可写，并发送数据!
 **注意事项:
 **       ------------------------------------------------
 **      | 已发送 |     待发送     |       剩余空间       |
 **       ------------------------------------------------
 **      |XXXXXXXX|////////////////|                      |
 **      |XXXXXXXX|////////////////|         left         |
 **      |XXXXXXXX|////////////////|                      |
 **       ------------------------------------------------
 **      ^        ^                ^                      ^
 **      |        |                |                      |
 **     addr     optr             iptr                   end
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_trav_send(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    int n, len;
    rtrd_sck_t *curr;
    rttp_snap_t *send;
    list2_node_t *node, *tail;

    rsvr->ctm = time(NULL);

    node = rsvr->conn_list->head;
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        curr = (rtrd_sck_t *)node->data;

        if (FD_ISSET(curr->fd, &rsvr->wrset))
        {
            curr->wrtm = rsvr->ctm;
            send = &curr->send;

            for (;;)
            {
                /* 1. 填充发送缓存 */
                if (send->iptr == send->optr)
                {
                    rtrd_rsvr_fill_send_buff(rsvr, curr);
                }

                /* 2. 发送缓存数据 */
                len = send->iptr - send->optr;
                if (0 == len)
                {
                    break;
                }

                n = Writen(curr->fd, send->optr, len);
                if (n != len)
                {
                    if (n > 0)
                    {
                        send->optr += n;
                        break;
                    }

                    log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));

                    rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
                    return RTTP_ERR;
                }

                /* 3. 重置标识量 */
                rttp_snap_reset(send);
            }
        }

        if (node == tail)
        {
            break;
        }

        node = node->next;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_recv_proc
 **功    能: 接收数据并做相应处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 初始化接收
 **     2. 接收数据头
 **     3. 接收数据体
 **     4. 进行数据处理
 **注意事项:
 **      | 已处理 |     未处理     |       剩余空间       |
 **       ------------------------------------------------
 **      |XXXXXXXX|////////////////|                      |
 **      |XXXXXXXX|////////////////|         left         |
 **      |XXXXXXXX|////////////////|                      |
 **       ------------------------------------------------
 **      ^        ^                ^                      ^
 **      |        |                |                      |
 **     addr     optr             iptr                   end
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_recv_proc(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    int n, left;
    rttp_snap_t *recv = &sck->recv;

    while (1)
    {
        /* 1. 接收网络数据 */
        left = (int)(recv->end - recv->iptr);

        n = read(sck->fd, recv->iptr, left);
        if (n > 0)
        {
            recv->iptr += n;

            /* 2. 进行数据处理 */
            if (rtrd_rsvr_data_proc(ctx, rsvr, sck))
            {
                log_error(rsvr->log, "Proc data failed! fd:%d", sck->fd);
                return RTTP_ERR;
            }
            continue;
        }
        else if (0 == n)
        {
            log_info(rsvr->log, "Client disconnected. fd:%d n:%d/%d", sck->fd, n, left);
            return RTTP_SCK_DISCONN;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return RTTP_OK; /* Again */
        }

        if (EINTR == errno)
        {
            continue;
        }

        log_error(rsvr->log, "errmsg:[%d] %s. fd:%d", errno, strerror(errno), sck->fd);
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_recv_post
 **功    能: 数据接收完成后的处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 系统数据处理
 **     2. 自定义数据处理
 **注意事项:
 **      | 已处理 |     未处理     |       剩余空间       |
 **       ------------------------------------------------
 **      |XXXXXXXX|////////////////|                      |
 **      |XXXXXXXX|////////////////|         left         |
 **      |XXXXXXXX|////////////////|                      |
 **       ------------------------------------------------
 **      ^        ^                ^                      ^
 **      |        |                |                      |
 **     addr     optr             iptr                   end
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_data_proc(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    bool flag = false;
    rttp_header_t *head;
    uint32_t len, one_mesg_len;
    rttp_snap_t *recv = &sck->recv;

    while (1)
    {
        flag = false;
        head = (rttp_header_t *)recv->optr;

        len = (uint32_t)(recv->iptr - recv->optr);
        if (len >= sizeof(rttp_header_t))
        {
            if (RTTP_CHECK_SUM != ntohl(head->checksum))
            {
                log_error(rsvr->log, "Header is invalid! nodeid:%d Mark:%X/%X type:%d len:%d flag:%d",
                        ntohl(head->nodeid), ntohl(head->checksum), RTTP_CHECK_SUM,
                        ntohs(head->type), ntohl(head->length), head->flag);
                return RTTP_ERR;
            }

            one_mesg_len = sizeof(rttp_header_t) + ntohl(head->length);
            if (len >= one_mesg_len)
            {
                flag = true;
            }
        }

        /* 1. 不足一条数据时 */
        if (!flag)
        {
            if (recv->iptr == recv->end)
            {
                /* 防止OverWrite的情况发生 */
                if ((recv->optr - recv->addr) < (recv->end - recv->iptr))
                {
                    log_error(rsvr->log, "Data length is invalid!");
                    return RTTP_ERR;
                }

                memcpy(recv->addr, recv->optr, len);
                recv->optr = recv->addr;
                recv->iptr = recv->optr + len;
                return RTTP_OK;
            }
            return RTTP_OK;
        }


        /* 2. 至少一条数据时 */
        /* 2.1 转化字节序 */
        head->type = ntohs(head->type);
        head->nodeid = ntohl(head->nodeid);
        head->flag = head->flag;
        head->length = ntohl(head->length);
        head->checksum = ntohl(head->checksum);

        /* 2.2 校验合法性 */
        if (!RTTP_HEAD_ISVALID(head))
        {
            ++rsvr->err_total;
            log_error(rsvr->log, "Header is invalid! Mark:%u/%u type:%d len:%d flag:%d",
                    head->checksum, RTTP_CHECK_SUM, head->type, head->length, head->flag);
            return RTTP_ERR;
        }

        /* 2.3 进行数据处理 */
        if (RTTP_SYS_MESG == head->flag)
        {
            rtrd_rsvr_sys_mesg_proc(ctx, rsvr, sck, recv->optr);
        }
        else
        {
            rtrd_rsvr_exp_mesg_proc(ctx, rsvr, sck, recv->optr);
        }

        recv->optr += one_mesg_len;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_sys_mesg_proc
 **功    能: 系统消息处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_sys_mesg_proc(rtrd_cntx_t *ctx,
        rtrd_rsvr_t *rsvr, rtrd_sck_t *sck, void *addr)
{
    rttp_header_t *head = (rttp_header_t *)addr;

    switch (head->type)
    {
        case RTTP_KPALIVE_REQ:
        {
            return rtrd_rsvr_keepalive_req_hdl(ctx, rsvr, sck);
        }
        case RTTP_LINK_AUTH_REQ:
        {
            return rtrd_rsvr_link_auth_req_hdl(ctx, rsvr, sck);
        }
        default:
        {
            log_error(rsvr->log, "Unknown message type! [%d]", head->type);
            return RTTP_ERR;
        }
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_exp_mesg_proc
 **功    能: 自定义消息处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 是否在NULL空间: 直接丢弃
 **     2. 放入队列中
 **     3. 发送处理请求
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_exp_mesg_proc(rtrd_cntx_t *ctx,
        rtrd_rsvr_t *rsvr, rtrd_sck_t *sck, void *data)
{
    int rqid;
    void *addr;
    rttp_header_t *head = (rttp_header_t *)data;

    ++rsvr->recv_total; /* 总数 */

    /* > 合法性验证 */
    if (head->nodeid != sck->nodeid)
    {
        ++rsvr->drop_total;
        log_error(rsvr->log, "Devid isn't right! nodeid:%d/%d", head->nodeid, sck->nodeid);
        return RTTP_ERR;
    }

    /* > 从队列申请空间 */
    rqid = rand() % ctx->conf.rqnum;

    addr = queue_malloc(ctx->recvq[rqid]);
    if (NULL == addr)
    {
        ++rsvr->drop_total; /* 丢弃计数 */
        rtrd_rsvr_cmd_proc_all_req(ctx, rsvr);

        log_warn(rsvr->log, "Recv queue was full! Perhaps lock conflicts too much!"
                "recv:%llu drop:%llu error:%llu",
                rsvr->recv_total, rsvr->drop_total, rsvr->err_total);
        return RTTP_ERR;
    }

    /* > 进行数据拷贝 */
    memcpy(addr, data, head->length + sizeof(rttp_header_t));

    queue_push(ctx->recvq[rqid], addr);         /* 放入处理队列 */

    rtrd_rsvr_cmd_proc_req(ctx, rsvr, rqid);    /* 发送处理请求 */

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_event_core_hdl
 **功    能: 事件核心处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 接收命令数据
 **     2. 遍历接收数据
 **     3. 遍历发送数据
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_event_core_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    /* 1. 接收命令数据 */
    if (FD_ISSET(rsvr->cmd_sck_id, &rsvr->rdset))
    {
        rtrd_rsvr_recv_cmd(ctx, rsvr);
    }

    /* 2. 遍历接收数据 */
    rtrd_rsvr_trav_recv(ctx, rsvr);

    /* 3. 遍历发送数据 */
    rtrd_rsvr_trav_send(ctx, rsvr);

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_event_timeout_hdl
 **功    能: 事件超时处理
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 检测超时连接
 **     2. 删除超时连接
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_event_timeout_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    bool is_end = false;
    rtrd_sck_t *curr;
    list2_node_t *node, *next, *tail;

    rsvr->ctm = time(NULL);

    /* > 检测超时连接 */
    node = rsvr->conn_list->head;
    if (NULL == node)
    {
        return RTTP_OK;
    }

    tail = node->prev;
    while ((NULL != node) && (false == is_end))
    {
        if (tail == node)
        {
            is_end = true;
        }

        curr = (rtrd_sck_t *)node->data;

        if (rsvr->ctm - curr->rdtm >= 60)
        {
            log_trace(rsvr->log, "Didn't active for along time! fd:%d ip:%s",
                    curr->fd, curr->ipaddr);

            /* 释放数据 */
            FREE(curr->recv.addr);

            /* 删除连接 */
            if (node == tail)
            {
                rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
                break;
            }

            next = node->next;
            rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
            node = next;
            continue;
        }

        node = node->next;
    }

    /* > 重复发送处理命令 */
    rtrd_rsvr_cmd_proc_all_req(ctx, rsvr);

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_keepalive_req_hdl
 **功    能: 保活请求处理
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_keepalive_req_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    void *addr;
    rttp_header_t *head;

    /* > 分配消息空间 */
    addr = slab_alloc(rsvr->pool, sizeof(rttp_header_t));
    if (NULL == addr)
    {
        log_error(rsvr->log, "Alloc memory from slab failed!");
        return RTTP_ERR;
    }

    /* > 回复消息内容 */
    head = (rttp_header_t *)addr;

    head->type = RTTP_KPALIVE_REP;
    head->nodeid = ctx->conf.nodeid;
    head->length = 0;
    head->flag = RTTP_SYS_MESG;
    head->checksum = RTTP_CHECK_SUM;

    /* > 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr))
    {
        slab_dealloc(rsvr->pool, addr);
        log_error(rsvr->log, "Insert into list failed!");
        return RTTP_ERR;
    }

    log_debug(rsvr->log, "Add respond of keepalive request!");

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_link_auth_rep
 **功    能: 链路鉴权应答
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将链路应答信息放入发送队列中
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int rtrd_rsvr_link_auth_rep(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    void *addr;
    rttp_header_t *head;
    rttp_link_auth_rep_t *link_auth_rep;

    /* > 分配消息空间 */
    addr = slab_alloc(rsvr->pool, sizeof(rttp_header_t));
    if (NULL == addr)
    {
        log_error(rsvr->log, "Alloc memory from slab failed!");
        return RTTP_ERR;
    }

    /* > 回复消息内容 */
    head = (rttp_header_t *)addr;
    link_auth_rep = (rttp_link_auth_rep_t *)(addr + sizeof(rttp_header_t));

    head->type = RTTP_LINK_AUTH_REP;
    head->nodeid = ctx->conf.nodeid;
    head->length = sizeof(rttp_link_auth_rep_t);
    head->flag = RTTP_SYS_MESG;
    head->checksum = RTTP_CHECK_SUM;

    link_auth_rep->is_succ = htonl(sck->auth_succ);

    /* > 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr))
    {
        slab_dealloc(rsvr->pool, addr);
        log_error(rsvr->log, "Insert into list failed!");
        return RTTP_ERR;
    }

    log_debug(rsvr->log, "Add respond of link-auth request!");

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_link_auth_req_hdl
 **功    能: 链路鉴权请求处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 校验鉴权是否通过, 并应答鉴权请求
 **注意事项: TODO: 待注册DEVID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int rtrd_rsvr_link_auth_req_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    rttp_snap_t *recv = &sck->recv;
    rttp_link_auth_req_t *link_auth_req;

    /* > 字节序转换 */
    link_auth_req = (rttp_link_auth_req_t *)(recv->addr + sizeof(rttp_header_t));

    link_auth_req->nodeid = ntohl(link_auth_req->nodeid);

    /* > 验证鉴权合法性 */
    sck->auth_succ = rtrd_link_auth_check(ctx, link_auth_req);
    if (sck->auth_succ)
    {
        sck->nodeid = link_auth_req->nodeid;

        /* > 插入DEV与SCK的映射 */
        if (rtrd_node_to_svr_map_add(ctx, link_auth_req->nodeid, rsvr->tidx))
        {
            log_error(rsvr->log, "Insert into sck2dev table failed! fd:%d serial:%ld nodeid:%d",
                    sck->fd, sck->serial, link_auth_req->nodeid);
            return RTTP_ERR;
        }
    }

    /* > 应答鉴权请求 */
    return rtrd_rsvr_link_auth_rep(ctx, rsvr, sck);
}

/******************************************************************************
 **函数名称: rtrd_rsvr_sck_creat
 **功    能: 创建套接字对象
 **输入参数:
 **     rsvr: 接收服务
 **     req: 添加套接字请求
 **输出参数: NONE
 **返    回: 套接字对象
 **实现描述: 创建套接字对象, 并依次初始化其成员变量
 **注意事项: 套接字关闭时, 记得释放空间, 防止内存泄露!
 **作    者: # Qifeng.zou # 2015.06.11 #
 ******************************************************************************/
static rtrd_sck_t *rtrd_rsvr_sck_creat(rtrd_rsvr_t *rsvr, rttp_cmd_add_sck_t *req)
{
    void *addr;
    list_opt_t opt;
    rtrd_sck_t *sck;

    /* > 分配连接空间 */
    sck = slab_alloc(rsvr->pool, sizeof(rtrd_sck_t));
    if (NULL == sck)
    {
        log_error(rsvr->log, "Alloc memory failed!");
        CLOSE(req->sckid);
        return NULL;
    }

    memset(sck, 0, sizeof(rtrd_sck_t));

    sck->fd = req->sckid;
    sck->nodeid = -1;
    sck->serial = req->sck_serial;
    sck->ctm = time(NULL);
    sck->rdtm = sck->ctm;
    sck->wrtm = sck->ctm;
    snprintf(sck->ipaddr, sizeof(sck->ipaddr), "%s", req->ipaddr);

    do
    {
        /* > 创建发送链表 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)rsvr->pool;
        opt.alloc = (mem_alloc_cb_t)slab_alloc;
        opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

        sck->mesg_list = list_creat(&opt);
        if (NULL == sck->mesg_list)
        {
            log_error(rsvr->log, "Create list failed!");
            break;
        }

        /* > 申请接收缓存 */
        addr = (void *)calloc(1, RTTP_BUFF_SIZE);
        if (NULL == addr)
        {
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        rttp_snap_setup(&sck->recv, addr, RTTP_BUFF_SIZE);

        /* > 申请发送缓存 */
        addr = (void *)calloc(1, RTTP_BUFF_SIZE);
        if (NULL == addr)
        {
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        rttp_snap_setup(&sck->send, addr, RTTP_BUFF_SIZE);

        return sck;
    } while (0);

    /* > 释放套接字对象 */
    rtrd_rsvr_sck_free(rsvr, sck);
    return NULL;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_sck_free
 **功    能: 释放指定套接字对象的空间
 **输入参数:
 **     rsvr: 接收服务
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 释放该套接字对象所有相关内存, 防止内存泄露!
 **作    者: # Qifeng.zou # 2015.06.11 23:31:48 #
 ******************************************************************************/
static void rtrd_rsvr_sck_free(rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    if (NULL == sck) { return; }
    FREE(sck->send.addr);
    FREE(sck->recv.addr);
    if (sck->mesg_list) {
        list_destroy(sck->mesg_list, rsvr->pool, (mem_dealloc_cb_t)slab_dealloc);
    }
    CLOSE(sck->fd);
    slab_dealloc(rsvr->pool, sck);
}

/******************************************************************************
 **函数名称: rtrd_rsvr_add_conn_hdl
 **功    能: 添加网络连接
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将套接字对象加入到套接字链表中
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_add_conn_hdl(rtrd_rsvr_t *rsvr, rttp_cmd_add_sck_t *req)
{
    rtrd_sck_t *sck;

    /* > 创建套接字对象 */
    sck = rtrd_rsvr_sck_creat(rsvr, req);
    if (NULL == sck)
    {
        log_error(rsvr->log, "Create socket object failed!");
        return RTTP_ERR;
    }

    /* > 加入套接字链尾 */
    if (list2_rpush(rsvr->conn_list, (void *)sck))
    {
        log_error(rsvr->log, "Insert into list failed!");
        rtrd_rsvr_sck_free(rsvr, sck);
        return RTTP_ERR;
    }

    ++rsvr->connections; /* 统计TCP连接数 */

    log_trace(rsvr->log, "Tidx [%d] insert sckid [%d] success! ip:%s",
            rsvr->tidx, req->sckid, req->ipaddr);

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_del_conn_hdl
 **功    能: 删除网络连接
 **输入参数:
 **     rsvr: 接收服务
 **     node: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 释放接收缓存和发送缓存空间!
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_del_conn_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, list2_node_t *node)
{
    rtrd_sck_t *curr = (rtrd_sck_t *)node->data;

    /* > 从链表剔除结点 */
    list2_delete(rsvr->conn_list, node);

    /* > 从SCK<->DEV映射表中剔除 */
    rtrd_node_to_svr_map_del(ctx, curr->nodeid, rsvr->tidx);

    /* > 释放数据空间 */
    rtrd_rsvr_sck_free(rsvr, curr);

    --rsvr->connections; /* 统计TCP连接数 */

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_del_all_conn_hdl
 **功    能: 删除接收线程所有的套接字
 **输入参数:
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void rtrd_rsvr_del_all_conn_hdl(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    list2_node_t *node, *next, *tail;

    node = rsvr->conn_list->head;
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        if (node == tail)
        {
            rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
            break;
        }

        next = node->next;
        rtrd_rsvr_del_conn_hdl(ctx, rsvr, node);
        node = next;
    }

    rsvr->connections = 0; /* 统计TCP连接数 */
    return;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_cmd_proc_req
 **功    能: 发送处理请求
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     rqid: 队列ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_cmd_proc_req(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr, int rqid)
{
    int widx;
    rttp_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;
    rttp_cmd_proc_req_t *req = (rttp_cmd_proc_req_t *)&cmd.args;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTTP_CMD_PROC_REQ;
    req->ori_svr_tidx = rsvr->tidx;
    req->num = -1;
    req->rqidx = rqid;

    /* 1. 随机选择Work线程 */
    /* widx = rttp_rand_work(ctx); */
    widx = rqid / RTTP_WORKER_HDL_QNUM;

    rtrd_worker_usck_path(conf, path, widx);

    /* 2. 发送处理命令 */
    if (unix_udp_send(rsvr->cmd_sck_id, path, &cmd, sizeof(rttp_cmd_t)) < 0)
    {
        log_debug(rsvr->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                errno, strerror(errno), path);
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_cmd_proc_all_req
 **功    能: 重复发送处理请求
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtrd_rsvr_cmd_proc_all_req(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    int idx;

    /* 依次遍历滞留总数 */
    for (idx=0; idx<ctx->conf.rqnum; ++idx)
    {
        rtrd_rsvr_cmd_proc_req(ctx, rsvr, idx);
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_fill_send_buff
 **功    能: 填充发送缓冲区
 **输入参数:
 **     rsvr: 接收线程
 **     sck: 连接对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 从消息链表取数据
 **     2. 从发送队列取数据
 **注意事项:
 **       ------------------------------------------------
 **      | 已发送 |     待发送     |       剩余空间       |
 **       ------------------------------------------------
 **      |XXXXXXXX|////////////////|                      |
 **      |XXXXXXXX|////////////////|         left         |
 **      |XXXXXXXX|////////////////|                      |
 **       ------------------------------------------------
 **      ^        ^                ^                      ^
 **      |        |                |                      |
 **     addr     optr             iptr                   end
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtrd_rsvr_fill_send_buff(rtrd_rsvr_t *rsvr, rtrd_sck_t *sck)
{
    int left, mesg_len;
    rttp_header_t *head;
    rttp_snap_t *send = &sck->send;

    /* > 从消息链表取数据 */
    for (;;)
    {
        /* 1 是否有数据 */
        head = (rttp_header_t *)list_lpop(sck->mesg_list);;
        if (NULL == head)
        {
            break; /* 无数据 */
        }

        /* 2 判断剩余空间 */
        if (RTTP_CHECK_SUM != head->checksum)
        {
            assert(0);
        }

        left = (int)(send->end - send->iptr);

        mesg_len = sizeof(rttp_header_t) + head->length;
        if (left < mesg_len)
        {
            list_lpush(sck->mesg_list, head);
            break; /* 空间不足 */
        }

        /* 3 取发送的数据 */
        head->type = htons(head->type);
        head->nodeid = htonl(head->nodeid);
        head->flag = head->flag;
        head->length = htonl(head->length);
        head->checksum = htonl(head->checksum);

        /* 1.4 拷贝至发送缓存 */
        memcpy(send->iptr, (void *)head, mesg_len);

        /* 1.5 释放数据空间 */
        slab_dealloc(rsvr->pool, head);

        send->iptr += mesg_len;
        continue;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_rsvr_conn_list_with_same_nodeid
 **功    能: 获取相同的DEVID的连接链表
 **输入参数:
 **     sck: 套接字数据
 **     c: 连接链表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.02 #
 ******************************************************************************/
typedef struct
{
    int nodeid;                 /* 结点ID */
    list_t *list;               /* 拥有相同DEVID的套接字链表 */
} conn_list_with_same_nodeid_t;

static int rtrd_rsvr_conn_list_with_same_nodeid(rtrd_sck_t *sck, conn_list_with_same_nodeid_t *c)
{
    if (sck->nodeid != c->nodeid)
    {
        return -1;
    }

    return list_rpush(c->list, sck); /* 注意: 销毁c->list时, 不必释放sck空间 */
}

/******************************************************************************
 **函数名称: rtrd_rsvr_dist_send_data
 **功    能: 分发连接队列中的数据
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.02 #
 ******************************************************************************/
static int rtrd_rsvr_dist_send_data(rtrd_cntx_t *ctx, rtrd_rsvr_t *rsvr)
{
    int len;
    queue_t *sendq;
    void *data, *addr;
    rttp_frwd_t *frwd;
    list_opt_t opt;
    rtrd_sck_t *sck;
    rttp_header_t *head;
    conn_list_with_same_nodeid_t conn;

    sendq = ctx->sendq[rsvr->tidx];

    while (1)
    {
        /* > 弹出队列数据 */
        data = queue_pop(sendq);
        if (NULL == data)
        {
            break; /* 无数据 */
        }

        frwd = (rttp_frwd_t *)data;

        /* > 查找发送连接 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)rsvr->pool;
        opt.alloc = (mem_alloc_cb_t)slab_alloc;
        opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

        conn.nodeid = frwd->dest_nodeid;
        conn.list = list_creat(&opt);
        if (NULL == conn.list)
        {
            queue_dealloc(sendq, data);
            log_error(rsvr->log, "Create list failed!");
            continue;
        }

        list2_trav(rsvr->conn_list, (list2_trav_cb_t)rtrd_rsvr_conn_list_with_same_nodeid, &conn);
        if (0 == conn.list->num)
        {
            queue_dealloc(sendq, data);
            list_destroy(conn.list, NULL, mem_dummy_dealloc);
            log_error(rsvr->log, "Didn't find connection by nodeid [%d]!", conn.nodeid);
            continue;
        }

        sck = (rtrd_sck_t *)conn.list->head->data; /* TODO: 暂时选择第一个 */
        
        /* > 设置发送数据 */
        len = sizeof(rttp_header_t) + frwd->length;

        addr = slab_alloc(rsvr->pool, len);
        if (NULL == addr)
        {
            queue_dealloc(sendq, data);
            list_destroy(conn.list, NULL, mem_dummy_dealloc);
            log_error(rsvr->log, "Alloc memory from slab failed!");
            continue;
        }

        head = (rttp_header_t *)addr;

        head->type = frwd->type;
        head->nodeid = frwd->dest_nodeid;
        head->flag = RTTP_EXP_MESG;
        head->checksum = RTTP_CHECK_SUM;
        head->length = frwd->length;

        memcpy(addr+sizeof(rttp_header_t), data+sizeof(rttp_frwd_t), head->length);

        queue_dealloc(sendq, data);

        /* > 放入发送链表 */
        if (list_rpush(sck->mesg_list, addr))
        {
            slab_dealloc(rsvr->pool, addr);
            log_error(rsvr->log, "Push input list failed!");
        }

        list_destroy(conn.list, NULL, mem_dummy_dealloc);
    }

    return RTTP_OK;
}

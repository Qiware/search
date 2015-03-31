/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/
#include <memory.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sdtp.h"
#include "syscall.h"
#include "xml_tree.h"
#include "sdtp_cmd.h"
#include "sdtp_priv.h"
#include "thread_pool.h"

/* 静态函数 */
static sdtp_rsvr_t *sdtp_rsvr_get_curr(sdtp_cntx_t *ctx);
static int sdtp_rsvr_event_core_hdl(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr);
static int sdtp_rsvr_event_timeout_hdl(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr);

static int sdtp_rsvr_trav_recv(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr);
static int sdtp_rsvr_trav_send(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr);

static int sdtp_rsvr_recv_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);
static int sdtp_rsvr_data_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);

static int sdtp_rsvr_sys_mesg_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);
static int sdtp_rsvr_exp_mesg_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);

static int sdtp_rsvr_keepalive_req_hdl(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);
static int sdtp_rsvr_cmd_proc_req(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, int rqid);
static int sdtp_rsvr_cmd_proc_all_req(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr);

static int sdtp_rsvr_add_conn_hdl(sdtp_rsvr_t *rsvr, sdtp_cmd_add_sck_t *req);
static int sdtp_rsvr_del_conn_hdl(sdtp_rsvr_t *rsvr, list2_node_t *node);

static int sdtp_rsvr_fill_send_buff(sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);
static int sdtp_rsvr_clear_mesg(sdtp_rsvr_t *rsvr, sdtp_sck_t *sck);

static int sdtp_rsvr_queue_alloc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr);
#define sdtp_rsvr_queue_push(ctx, rsvr) /* 将输入推入队列 */\
    queue_push(ctx->recvq[rsvr->queue.rqid], rsvr->queue.start)
#define sdtp_rsvr_queue_reset(rsvr) \
{ \
    (rsvr)->queue.rqid = -1; \
    (rsvr)->queue.start = NULL; \
    (rsvr)->queue.addr = NULL; \
    (rsvr)->queue.end = NULL; \
    (rsvr)->queue.num = NULL; \
    (rsvr)->queue.size = 0; \
}

/* 随机选择接收线程 */
#define sdtp_rand_recv(ctx) ((ctx)->listen.total++ % (ctx->recvtp->num))

/* 随机选择工作线程 */
#define sdtp_rand_work(ctx) (rand() % (ctx->worktp->num))

/******************************************************************************
 **函数名称: sdtp_rsvr_set_rdset
 **功    能: 设置可读集合
 **输入参数: 
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     如果超时未接收或发送数据，则关闭连接!
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
#define sdtp_rsvr_set_rdset(rsvr) \
{ \
    sdtp_sck_t *curr; \
    list2_node_t *node, *next, *tail; \
    \
    FD_ZERO(&rsvr->rdset); \
    \
    FD_SET(rsvr->cmd_sck_id, &rsvr->rdset); \
    rsvr->max = rsvr->cmd_sck_id; \
    \
    node = rsvr->conn_list.head; \
    if (NULL != node) \
    { \
        tail = node->prev; \
    } \
    while (NULL != node) \
    { \
        curr = (sdtp_sck_t *)node->data; \
        if ((rsvr->ctm - curr->rdtm > 30) \
            && (rsvr->ctm - curr->wrtm > 30)) \
        { \
            log_trace(rsvr->log, "Didn't active for along time! fd:%d ip:%s", \
                    curr->fd, curr->ipaddr); \
            \
            if (node == tail) \
            { \
                sdtp_rsvr_del_conn_hdl(rsvr, node); \
                break; \
            } \
            next = node->next; \
            sdtp_rsvr_del_conn_hdl(rsvr, node); \
            node = next; \
            continue; \
        } \
        \
        FD_SET(curr->fd, &rsvr->rdset); \
        rsvr->max = (rsvr->max > curr->fd)? rsvr->max : curr->fd; \
        \
        if (node == tail) \
        { \
            break; \
        } \
        node = node->next; \
    } \
}

/******************************************************************************
 **函数名称: sdtp_rsvr_set_wrset
 **功    能: 设置可写集合
 **输入参数: 
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     只有发送链表中存在数据时，才将该套接字加入到可写侦听集合!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
#define sdtp_rsvr_set_wrset(rsvr) \
{ \
    sdtp_sck_t *curr; \
    list2_node_t *node, *tail; \
    \
    FD_ZERO(&rsvr->wrset); \
    \
    node = rsvr->conn_list.head; \
    if (NULL != node) \
    { \
        tail = node->prev; \
    } \
    while (NULL != node) \
    { \
        curr = (sdtp_sck_t *)node->data; \
        \
        if (list_isempty(curr->mesg_list) \
            && (curr->send.optr == curr->send.iptr)) \
        { \
            if (node == tail) \
            { \
                break; \
            } \
            node = node->next; \
            continue; \
        } \
        \
        FD_SET(curr->fd, &rsvr->wrset); \
        \
        if (node == tail) \
        { \
            break; \
        } \
        node = node->next; \
    } \
}

/******************************************************************************
 **函数名称: sdtp_rsvr_routine
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
void *sdtp_rsvr_routine(void *_ctx)
{
    int ret;
    sdtp_rsvr_t *rsvr;
    struct timeval timeout;
    sdtp_cntx_t *ctx = (sdtp_cntx_t *)_ctx;

    /* 1. 获取接收服务 */
    rsvr = sdtp_rsvr_get_curr(ctx);
    if (NULL == rsvr)
    {
        log_fatal(rsvr->log, "Get recv server failed!");
        abort();
        return (void *)SDTP_ERR;
    }

    for (;;)
    {
        /* 2. 等待事件通知 */
        sdtp_rsvr_set_rdset(rsvr);
        sdtp_rsvr_set_wrset(rsvr);

        timeout.tv_sec = 30;
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
            return (void *)SDTP_ERR;
        }
        else if (0 == ret)
        {
            sdtp_rsvr_event_timeout_hdl(ctx, rsvr);
            continue;
        }

        /* 3. 进行事件处理 */
        sdtp_rsvr_event_core_hdl(ctx, rsvr);
    }

    log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_get_curr
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
static sdtp_rsvr_t *sdtp_rsvr_get_curr(sdtp_cntx_t *ctx)
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
    return (sdtp_rsvr_t *)(ctx->recvtp->data + tidx * sizeof(sdtp_rsvr_t));
}

/******************************************************************************
 **函数名称: sdtp_rsvr_init
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
int sdtp_rsvr_init(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, int tidx)
{
    void *addr;
    char path[FILE_PATH_MAX_LEN];
    sdtp_conf_t *conf = &ctx->conf;

    rsvr->tidx = tidx;
    rsvr->log = ctx->log;
    rsvr->ctm = time(NULL);

    /* > 创建CMD套接字 */
    sdtp_rsvr_usck_path(conf, path, rsvr->tidx);
    
    rsvr->cmd_sck_id = unix_udp_creat(path);
    if (rsvr->cmd_sck_id < 0)
    {
        log_error(rsvr->log, "Create unix-udp socket failed!");
        return SDTP_ERR;
    }

    /* > 创建SLAB内存池 */
    addr = calloc(1, SDTP_MEM_POOL_SIZE);
    if (NULL == addr)
    {
        log_fatal(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    rsvr->pool = slab_init(addr, SDTP_MEM_POOL_SIZE);
    if (NULL == rsvr->pool)
    {
        log_error(rsvr->log, "Initialize slab mem-pool failed!");
        return SDTP_ERR;
    }

    /* > 初始化队列设置 */
    sdtp_rsvr_queue_reset(rsvr);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_recv_cmd
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
static int sdtp_rsvr_recv_cmd(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    sdtp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令数据 */
    if (unix_udp_recv(rsvr->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(rsvr->log, "Recv command failed!");
        return SDTP_ERR_RECV_CMD;
    }

    /* 2. 进行命令处理 */
    switch (cmd.type)
    {
        case SDTP_CMD_ADD_SCK:
        {
            return sdtp_rsvr_add_conn_hdl(rsvr, (sdtp_cmd_add_sck_t *)&cmd.args);
        }
        default:
        {
            log_error(rsvr->log, "Unknown command! type:%d", cmd.type);
            return SDTP_ERR_UNKNOWN_CMD;
        }
    }

    return SDTP_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_trav_recv
 **功    能: 遍历接收数据
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     遍历判断套接字是否可读，并接收数据!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int sdtp_rsvr_trav_recv(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    sdtp_sck_t *curr;
    list2_node_t *node, *next, *tail;

    rsvr->ctm = time(NULL);

    node = rsvr->conn_list.head;
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        curr = (sdtp_sck_t *)node->data;

        if (FD_ISSET(curr->fd, &rsvr->rdset))
        {
            curr->rdtm = rsvr->ctm;

            /* 进行接收处理 */
            if (sdtp_rsvr_recv_proc(ctx, rsvr, curr))
            {
                log_error(rsvr->log, "Recv proc failed! fd:%d ip:%s", curr->fd, curr->ipaddr);
                if (node == tail)
                {
                    sdtp_rsvr_del_conn_hdl(rsvr, node);
                    break;
                }
                next = node->next;
                sdtp_rsvr_del_conn_hdl(rsvr, node);
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

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_trav_send
 **功    能: 遍历发送数据
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     遍历判断套接字是否可写，并发送数据!
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
static int sdtp_rsvr_trav_send(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    int n, len;
    sdtp_sck_t *curr;
    sdtp_snap_t *send;
    list2_node_t *node, *tail;

    rsvr->ctm = time(NULL);

    node = rsvr->conn_list.head;
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        curr = (sdtp_sck_t *)node->data;

        if (FD_ISSET(curr->fd, &rsvr->wrset))
        {
            curr->wrtm = rsvr->ctm;
            send = &curr->send;

            for (;;)
            {
                /* 1. 填充发送缓存 */
                if (send->iptr == send->optr)
                {
                    sdtp_rsvr_fill_send_buff(rsvr, curr);
                }

                /* 2. 发送缓存数据 */
                len = send->iptr - send->optr;

                n = Writen(curr->fd, send->optr, len);
                if (n != len)
                {
                    if (n > 0)
                    {
                        send->optr += n;
                        break;
                    }

                    log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));

                    sdtp_rsvr_del_conn_hdl(rsvr, node);
                    return SDTP_ERR;
                }

                /* 3. 重置标识量 */
                sdtp_snap_reset(send);
            }
        }

        if (node == tail)
        {
            break;
        }

        node = node->next;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_recv_proc
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
static int sdtp_rsvr_recv_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    int n, left;
    sdtp_snap_t *recv = &sck->recv;

    while (1)
    {
        /* 1. 接收网络数据 */
        left = (int)(recv->end - recv->iptr);

        n = read(sck->fd, recv->iptr, left);
        if (n > 0)
        {
            recv->iptr += n;

            /* 2. 进行数据处理 */
            if (sdtp_rsvr_data_proc(ctx, rsvr, sck))
            {
                log_error(rsvr->log, "Proc data failed! fd:%d", sck->fd);
                return SDTP_ERR;
            }
            continue;
        }
        else if (0 == n)
        {
            log_info(rsvr->log, "Client disconnected. fd:%d n:%d/%d", sck->fd, n, left);
            return SDTP_DISCONN;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return SDTP_OK; /* Again */
        }

        if (EINTR == errno)
        {
            continue;
        }

        log_error(rsvr->log, "errmsg:[%d] %s. fd:%d", errno, strerror(errno), sck->fd);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_recv_post
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
static int sdtp_rsvr_data_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    bool flag = false;
    sdtp_header_t *head;
    uint32_t len, one_mesg_len;
    sdtp_snap_t *recv = &sck->recv;

    while (1)
    {
        flag = false;
        head = (sdtp_header_t *)recv->optr;

        len = (uint32_t)(recv->iptr - recv->optr);
        if (len >= sizeof(sdtp_header_t))
        {
            if (SDTP_CHECK_SUM != ntohl(head->checksum))
            {
                log_error(rsvr->log, "Header is invalid! Mark:%X/%X type:%d len:%d flag:%d",
                        ntohl(head->checksum), SDTP_CHECK_SUM, ntohs(head->type),
                        ntohl(head->length), head->flag);
                return SDTP_ERR;
            }

            one_mesg_len = sizeof(sdtp_header_t) + ntohl(head->length);
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
                    return SDTP_ERR;
                }

                memcpy(recv->addr, recv->optr, len);
                recv->optr = recv->addr;
                recv->iptr = recv->optr + len;
                return SDTP_OK;
            }
            return SDTP_OK;
        }


        /* 2. 至少一条数据时 */
        /* 2.1 转化字节序 */
        head->type = ntohs(head->type);
        head->flag = head->flag;
        head->length = ntohl(head->length);
        head->checksum = ntohl(head->checksum);

        /* 2.2 校验合法性 */
        if (!SDTP_HEAD_ISVALID(head))
        {
            ++rsvr->err_total;
            log_error(rsvr->log, "Header is invalid! Mark:%u/%u type:%d len:%d flag:%d",
                    head->checksum, SDTP_CHECK_SUM, head->type, head->length, head->flag);
            return SDTP_ERR;
        }

        /* 2.3 进行数据处理 */
        if (SDTP_SYS_MESG == head->flag)
        {
            sdtp_rsvr_sys_mesg_proc(ctx, rsvr, sck);
        }
        else
        {
            sdtp_rsvr_exp_mesg_proc(ctx, rsvr, sck);
        }

        recv->optr += one_mesg_len;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_sys_mesg_proc
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
static int sdtp_rsvr_sys_mesg_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    sdtp_snap_t *recv = &sck->recv;
    sdtp_header_t *head = (sdtp_header_t *)recv->addr;

    switch (head->type)
    {
        case SDTP_KPALIVE_REQ:
        {
            return sdtp_rsvr_keepalive_req_hdl(ctx, rsvr, sck);
        }
        default:
        {
            log_error(rsvr->log, "Unknown message type! [%d]", head->type);
            return SDTP_ERR;
        }
    }
    
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_queue_alloc
 **功    能: 申请队列空间
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.01 #
 ******************************************************************************/
static int sdtp_rsvr_queue_alloc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    rsvr->queue.rqid = rand() % ctx->conf.rqnum;

    rsvr->queue.start = queue_malloc(ctx->recvq[rsvr->queue.rqid]);
    if (NULL == rsvr->queue.start)
    {
        rsvr->queue.rqid = -1;
        sdtp_rsvr_cmd_proc_all_req(ctx, rsvr);

        log_warn(rsvr->log, "Recv queue was full! Perhaps lock conflicts too much!"
                "recv:%llu drop:%llu error:%llu",
                rsvr->recv_total, rsvr->drop_total, rsvr->err_total);
        return SDTP_ERR;
    }

    rsvr->queue.num = (int *)rsvr->queue.start;
    *rsvr->queue.num = 0;
    rsvr->queue.addr = rsvr->queue.start + sizeof(int);
    rsvr->queue.end = rsvr->queue.start + queue_size(ctx->recvq[rsvr->queue.rqid]);
    rsvr->queue.size = queue_size(ctx->recvq[rsvr->queue.rqid]);
    rsvr->queue.alloc_tm = time(NULL);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_exp_mesg_proc
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
static int sdtp_rsvr_exp_mesg_proc(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    int len;
    sdtp_snap_t *recv = &sck->recv;
    sdtp_header_t *head = (sdtp_header_t *)recv->optr;

    ++rsvr->recv_total; /* 总数 */

    while (1)
    {
        /* > 从队列申请空间 */
        if (-1 == rsvr->queue.rqid)
        {
            if (sdtp_rsvr_queue_alloc(ctx, rsvr))
            {
                ++rsvr->drop_total; /* 丢弃计数 */
                log_error(rsvr->log, "Alloc from queue failed!");
                return SDTP_ERR;
            }
        }

        /* > 进行数据拷贝 */
        len = head->length + sizeof(sdtp_header_t);
        if (rsvr->queue.end - rsvr->queue.addr >= len)
        {
            memcpy(rsvr->queue.addr, recv->optr, len);
            rsvr->queue.addr += len;
            ++(*rsvr->queue.num);
            return SDTP_OK;
        }

        /* > 将数据放入队列 */
        sdtp_rsvr_queue_push(ctx, rsvr);

        /* > 发送处理请求 */
        sdtp_rsvr_cmd_proc_req(ctx, rsvr, rsvr->queue.rqid);

        sdtp_rsvr_queue_reset(rsvr);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_event_core_hdl
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
static int sdtp_rsvr_event_core_hdl(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    /* 1. 接收命令数据 */
    if (FD_ISSET(rsvr->cmd_sck_id, &rsvr->rdset))
    {
        sdtp_rsvr_recv_cmd(ctx, rsvr);
    }

    /* 2. 遍历接收数据 */
    sdtp_rsvr_trav_recv(ctx, rsvr);

    /* 3. 遍历发送数据 */
    sdtp_rsvr_trav_send(ctx, rsvr);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_event_timeout_hdl
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
static int sdtp_rsvr_event_timeout_hdl(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    bool is_end = false;
    sdtp_sck_t *curr;
    list2_node_t *node, *next, *tail;

    rsvr->ctm = time(NULL);

    /* > 将数据放入队列 */
    if (-1 != rsvr->queue.rqid
        && (rsvr->ctm - rsvr->queue.alloc_tm > 5))
    {
        sdtp_rsvr_queue_push(ctx, rsvr);
        sdtp_rsvr_queue_reset(rsvr);
    }

    /* > 检测超时连接 */
    node = rsvr->conn_list.head;
    if (NULL == node)
    {
        return SDTP_OK;
    }

    tail = node->prev;
    while ((NULL != node) && (false == is_end))
    {
        if (tail == node)
        {
            is_end = true;
        }

        curr = (sdtp_sck_t *)node->data;

        if (rsvr->ctm - curr->rdtm >= 60)
        {
            log_trace(rsvr->log, "Didn't active for along time! fd:%d ip:%s",
                    curr->fd, curr->ipaddr);

            /* 释放数据 */
            Free(curr->recv.addr);

            /* 删除连接 */
            if (node == tail)
            {
                sdtp_rsvr_del_conn_hdl(rsvr, node);
                break;
            }

            next = node->next;
            sdtp_rsvr_del_conn_hdl(rsvr, node);
            node = next;
            continue;
        }

        node = node->next;
    }

    /* 2. 重复发送处理命令 */
    sdtp_rsvr_cmd_proc_all_req(ctx, rsvr);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_keepalive_req_hdl
 **功    能: 保活请求处理
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int sdtp_rsvr_keepalive_req_hdl(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    void *addr;
    sdtp_header_t *head;

    /* >> 分配消息空间 */
    addr = slab_alloc(rsvr->pool, sizeof(sdtp_header_t));
    if (NULL == addr)
    {
        log_error(rsvr->log, "Alloc memory from slab failed!");
        return SDTP_ERR;
    }

    /* >> 回复消息内容 */
    head = (sdtp_header_t *)addr;

    head->type = SDTP_KPALIVE_REP;
    head->length = 0;
    head->flag = SDTP_SYS_MESG;
    head->checksum = SDTP_CHECK_SUM;
    
    /* >> 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr))
    {
        slab_dealloc(rsvr->pool, addr);
        log_error(rsvr->log, "Insert into list failed!");
        return SDTP_ERR;
    }

    log_debug(rsvr->log, "Add respond of keepalive request!");

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_add_conn_hdl
 **功    能: 添加网络连接
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int sdtp_rsvr_add_conn_hdl(sdtp_rsvr_t *rsvr, sdtp_cmd_add_sck_t *req)
{
    void *addr;
    sdtp_sck_t *sck;
    list2_node_t *node;

    /* 1. 分配连接空间 */
    sck = slab_alloc(rsvr->pool, sizeof(sdtp_sck_t));
    if (NULL == sck)
    {
        log_error(rsvr->log, "Alloc memory failed!");
        return SDTP_ERR;
    }

    sck->fd = req->sckid;
    sck->ctm = time(NULL);
    sck->rdtm = sck->ctm;
    sck->wrtm = sck->ctm;
    snprintf(sck->ipaddr, sizeof(sck->ipaddr), "%s", req->ipaddr);

    /* 2. 申请接收缓存 */
    addr = (void *)calloc(1, SDTP_BUFF_SIZE);
    if (NULL == addr)
    {
        log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        slab_dealloc(rsvr->pool, sck);
        return SDTP_ERR;
    }

    sdtp_snap_setup(&sck->recv, addr, SDTP_BUFF_SIZE);

    /* 3. 申请发送缓存 */
    addr = (void *)calloc(1, SDTP_BUFF_SIZE);
    if (NULL == addr)
    {
        log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        free(sck->recv.addr);
        slab_dealloc(rsvr->pool, sck);
        return SDTP_ERR;
    }

    sdtp_snap_setup(&sck->send, addr, SDTP_BUFF_SIZE);

    /* 4. 加入链尾 */
    node = slab_alloc(rsvr->pool, sizeof(list2_node_t));
    if (NULL == node)
    {
        log_error(rsvr->log, "Alloc memory failed!");
        free(sck->recv.addr);
        free(sck->send.addr);
        slab_dealloc(rsvr->pool, sck);
        return SDTP_ERR;
    }

    node->data = (void *)sck;

    list2_insert_tail(&rsvr->conn_list, node);

    ++rsvr->connections; /* 统计TCP连接数 */

    log_trace(rsvr->log, "Tidx [%d] insert sckid [%d] success! ip:%s",
            rsvr->tidx, req->sckid, req->ipaddr);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_del_conn_hdl
 **功    能: 删除网络连接
 **输入参数: 
 **     rsvr: 接收服务
 **     node: 套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     释放接收缓存和发送缓存空间!
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int sdtp_rsvr_del_conn_hdl(sdtp_rsvr_t *rsvr, list2_node_t *node)
{
    sdtp_sck_t *curr = (sdtp_sck_t *)node->data;

    /* 1. 从链表剔除结点 */
    list2_delete(&rsvr->conn_list, node);

    slab_dealloc(rsvr->pool, node);

    /* 2. 释放数据空间 */
    Close(curr->fd);

    Free(curr->recv.addr);
    Free(curr->send.addr);
    sdtp_rsvr_clear_mesg(rsvr, curr);
    curr->recv_total = 0;
    slab_dealloc(rsvr->pool, curr);

    --rsvr->connections; /* 统计TCP连接数 */

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_del_all_conn_hdl
 **功    能: 删除接收线程所有的套接字
 **输入参数: 
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void sdtp_rsvr_del_all_conn_hdl(sdtp_rsvr_t *rsvr)
{
    list2_node_t *node, *next, *tail;

    node = rsvr->conn_list.head; 
    if (NULL != node)
    {
        tail = node->prev;
    }

    while (NULL != node)
    {
        if (node == tail)
        {
            sdtp_rsvr_del_conn_hdl(rsvr, node);
            break;
        }

        next = node->next;
        sdtp_rsvr_del_conn_hdl(rsvr, node);
        node = next;
    }

    rsvr->connections = 0; /* 统计TCP连接数 */
    return;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_clear_mesg
 **功    能: 清空发送消息
 **输入参数: 
 **    rsvr: 接收服务
 **    sck: 将要清空的套接字对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     TODO: 待补充对list->tail的处理
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int sdtp_rsvr_clear_mesg(sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    void *data;

    while (1)
    {
        data = list_lpop(sck->mesg_list);
        if (NULL == data)
        {
            return SDTP_OK;
        }

        slab_dealloc(rsvr->pool, data);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_cmd_proc_req
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
static int sdtp_rsvr_cmd_proc_req(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr, int rqid)
{
    int widx;
    sdtp_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    sdtp_conf_t *conf = &ctx->conf;
    sdtp_cmd_proc_req_t *req = (sdtp_cmd_proc_req_t *)&cmd.args;

    cmd.type = SDTP_CMD_PROC_REQ;
    req->ori_rsvr_tidx = rsvr->tidx;
    req->num = -1;
    req->rqidx = rqid;

    /* 1. 随机选择Work线程 */
    /* widx = sdtp_rand_work(ctx); */
    widx = rqid / SDTP_WORKER_HDL_QNUM;

    sdtp_worker_usck_path(conf, path, widx);

    /* 2. 发送处理命令 */
    if (unix_udp_send(rsvr->cmd_sck_id, path, &cmd, sizeof(sdtp_cmd_t)) < 0)
    {
        log_debug(rsvr->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                errno, strerror(errno), path);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_cmd_proc_all_req
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
static int sdtp_rsvr_cmd_proc_all_req(sdtp_cntx_t *ctx, sdtp_rsvr_t *rsvr)
{
    int idx;

    /* 依次遍历滞留总数 */
    for (idx=0; idx<ctx->conf.rqnum; ++idx)
    {
        sdtp_rsvr_cmd_proc_req(ctx, rsvr, idx);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_rsvr_fill_send_buff
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
static int sdtp_rsvr_fill_send_buff(sdtp_rsvr_t *rsvr, sdtp_sck_t *sck)
{
    int left, mesg_len;
    sdtp_header_t *head;
    sdtp_snap_t *send = &sck->send;

    /* >> 从消息链表取数据 */
    for (;;)
    {
        /* 1 是否有数据 */
        head = (sdtp_header_t *)list_lpop(sck->mesg_list);;
        if (NULL == head)
        {
            break; /* 无数据 */
        }

        /* 2 判断剩余空间 */
        if (SDTP_CHECK_SUM != head->checksum)
        {
            assert(0);
        }

        left = (int)(send->end - send->iptr);

        mesg_len = sizeof(sdtp_header_t) + head->length;
        if (left < mesg_len)
        {
            list_lpush(sck->mesg_list, head);
            break; /* 空间不足 */
        }

        /* 3 取发送的数据 */
        head->type = htons(head->type);
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

    return SDTP_OK;
}

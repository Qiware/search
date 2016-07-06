/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: rtmq_rsvr.c
 ** 版本号: 1.0
 ** 描  述: 实时消息队列(REAL-TIME MESSAGE QUEUE)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/

#include "redo.h"
#include "mem_ref.h"
#include "rtmq_mesg.h"
#include "rtmq_comm.h"
#include "rtmq_recv.h"
#include "thread_pool.h"

/* 静态函数 */
static rtmq_rsvr_t *rtmq_rsvr_get_curr(rtmq_cntx_t *ctx);
static int rtmq_rsvr_event_core_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr);
static int rtmq_rsvr_event_timeout_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr);

static int rtmq_rsvr_trav_recv(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr);
static int rtmq_rsvr_trav_send(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr);

static int rtmq_rsvr_recv_proc(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck);
static int rtmq_rsvr_data_proc(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck);

static int rtmq_rsvr_sys_mesg_proc(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr);
static int rtmq_rsvr_exp_mesg_proc(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr);

static int rtmq_rsvr_keepalive_req_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr);
static int rtmq_rsvr_link_auth_req_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr);
static int rtmq_rsvr_sub_req_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr);

static int rtmq_rsvr_cmd_proc_req(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, int rqid);
static int rtmq_rsvr_cmd_proc_all_req(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr);

static rtmq_sck_t *rtmq_rsvr_sck_creat(rtmq_rsvr_t *rsvr, rtmq_cmd_add_sck_t *req);
static void rtmq_rsvr_sck_free(rtmq_rsvr_t *rsvr, rtmq_sck_t *sck);

static int rtmq_rsvr_add_conn_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_cmd_add_sck_t *req);
static int rtmq_rsvr_del_conn_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, list2_node_t *node);

static int rtmq_rsvr_fill_send_buff(rtmq_rsvr_t *rsvr, rtmq_sck_t *sck);

static int rtmq_rsvr_dist_data(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr);

/* 随机选择接收线程 */
#define rtmq_rand_recv(ctx) ((ctx)->listen.total++ % (ctx->recvtp->num))

/* 随机选择工作线程 */
#define rtmq_rand_work(ctx) (rand() % (ctx->worktp->num))

/******************************************************************************
 **函数名称: rtmq_rsvr_set_rdset
 **功    能: 设置可读集合
 **输入参数:
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 如果超时未接收或发送数据，则关闭连接!
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static void rtmq_rsvr_set_rdset(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    rtmq_sck_t *curr;
    list2_node_t *node, *next, *tail;

    FD_ZERO(&rsvr->rdset);

    FD_SET(rsvr->cmd_sck_id, &rsvr->rdset);
    rsvr->max = rsvr->cmd_sck_id;

    node = rsvr->conn_list->head;
    if (NULL != node) {
        tail = node->prev;
    }
    while (NULL != node) {
        curr = (rtmq_sck_t *)node->data;
        if ((rsvr->ctm - curr->rdtm > 30)
            && (rsvr->ctm - curr->wrtm > 30))
        {
            log_error(rsvr->log, "Didn't active for along time! fd:%d ip:%s",
                    curr->fd, curr->ipaddr);

            if (node == tail) {
                rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
                break;
            }
            next = node->next;
            rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
            node = next;
            continue;
        }

        FD_SET(curr->fd, &rsvr->rdset);
        rsvr->max = MAX(rsvr->max, curr->fd);

        if (node == tail) {
            break;
        }
        node = node->next;
    }
}

/******************************************************************************
 **函数名称: rtmq_rsvr_set_wrset
 **功    能: 设置可写集合
 **输入参数:
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 只有发送链表中存在数据时，才将该套接字加入到可写侦听集合!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static void rtmq_rsvr_set_wrset(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    rtmq_sck_t *curr;
    list2_node_t *node, *tail;

    FD_ZERO(&rsvr->wrset);

    node = rsvr->conn_list->head;
    if (NULL != node) {
        tail = node->prev;
    }

    while (NULL != node) {
        curr = (rtmq_sck_t *)node->data;

        if (list_empty(curr->mesg_list)
            && wiov_isempty(&curr->send))
        {
            if (node == tail) {
                break;
            }
            node = node->next;
            continue;
        }

        FD_SET(curr->fd, &rsvr->wrset);

        if (node == tail) {
            break;
        }
        node = node->next;
    }
}

/******************************************************************************
 **函数名称: rtmq_rsvr_routine
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
void *rtmq_rsvr_routine(void *_ctx)
{
    int ret;
    rtmq_rsvr_t *rsvr;
    struct timeval timeout;
    rtmq_cntx_t *ctx = (rtmq_cntx_t *)_ctx;

    nice(-20);

    /* 1. 获取接收服务 */
    rsvr = rtmq_rsvr_get_curr(ctx);
    if (NULL == rsvr) {
        log_fatal(rsvr->log, "Get recv server failed!");
        abort();
        return (void *)RTMQ_ERR;
    }

    for (;;) {
        /* 2. 等待事件通知 */
        rtmq_rsvr_set_rdset(ctx, rsvr);
        rtmq_rsvr_set_wrset(ctx, rsvr);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        ret = select(rsvr->max+1, &rsvr->rdset, &rsvr->wrset, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)RTMQ_ERR;
        }
        else if (0 == ret) {
            rtmq_rsvr_event_timeout_hdl(ctx, rsvr);
            continue;
        }

        /* 3. 进行事件处理 */
        rtmq_rsvr_event_core_hdl(ctx, rsvr);
    }

    log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_get_curr
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
static rtmq_rsvr_t *rtmq_rsvr_get_curr(rtmq_cntx_t *ctx)
{
    int id;

    /* 1. 获取当前线程的索引 */
    id = thread_pool_get_tidx(ctx->recvtp);
    if (id < 0) {
        log_error(ctx->log, "Get index of current thread failed!");
        return NULL;
    }

    /* 2. 返回当前线程对应的接收服务 */
    return (rtmq_rsvr_t *)(ctx->recvtp->data + id * sizeof(rtmq_rsvr_t));
}

/******************************************************************************
 **函数名称: rtmq_rsvr_init
 **功    能: 初始化接收服务
 **输入参数:
 **     ctx: 全局对象
 **     id: 接收服务编号
 **输出参数:
 **     rsvr: 接收服务
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 获取当前线程的索引
 **     2. 返回当前线程对应的接收服务
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
int rtmq_rsvr_init(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, int id)
{
    char path[FILE_PATH_MAX_LEN];
    rtmq_conf_t *conf = &ctx->conf;

    rsvr->id = id;
    rsvr->log = ctx->log;
    rsvr->ctm = time(NULL);
    rsvr->ctx = (void *)ctx;

    /* > 创建CMD套接字 */
    rtmq_rsvr_usck_path(conf, path, rsvr->id);

    rsvr->cmd_sck_id = unix_udp_creat(path);
    if (rsvr->cmd_sck_id < 0) {
        log_error(rsvr->log, "Create unix-udp socket failed!");
        return RTMQ_ERR;
    }

    /* > 创建套接字链表 */
    rsvr->conn_list = list2_creat(NULL);
    if (NULL == rsvr->conn_list) {
        log_error(rsvr->log, "Create list2 failed!");
        CLOSE(rsvr->cmd_sck_id);
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_recv_cmd
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
static int rtmq_rsvr_recv_cmd(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    rtmq_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令数据 */
    if (unix_udp_recv(rsvr->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0) {
        log_error(rsvr->log, "Recv command failed!");
        return RTMQ_ERR_RECV_CMD;
    }

    /* 2. 进行命令处理 */
    switch (cmd.type) {
        case RTMQ_CMD_ADD_SCK:      /* 添加套接字 */
        {
            return rtmq_rsvr_add_conn_hdl(ctx, rsvr, (rtmq_cmd_add_sck_t *)&cmd.param);
        }
        case RTMQ_CMD_DIST_REQ:     /* 分发发送数据 */
        {
            return rtmq_rsvr_dist_data(ctx, rsvr);
        }
        default:
        {
            log_error(rsvr->log, "Unknown command! type:%d", cmd.type);
            return RTMQ_ERR_UNKNOWN_CMD;
        }
    }

    return RTMQ_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_trav_recv
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
static int rtmq_rsvr_trav_recv(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    rtmq_sck_t *curr;
    list2_node_t *node, *next, *tail;

    rsvr->ctm = time(NULL);

    node = rsvr->conn_list->head;
    if (NULL != node) {
        tail = node->prev;
    }

    while (NULL != node) {
        curr = (rtmq_sck_t *)node->data;

        if (FD_ISSET(curr->fd, &rsvr->rdset)) {
            curr->rdtm = rsvr->ctm;

            /* 进行接收处理 */
            if (rtmq_rsvr_recv_proc(ctx, rsvr, curr)) {
                log_error(rsvr->log, "Recv proc failed! fd:%d ip:%s", curr->fd, curr->ipaddr);
                if (node == tail) {
                    rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
                    break;
                }
                next = node->next;
                rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
                node = next;
                continue;
            }
        }

        if (node == tail) {
            break;
        }

        node = node->next;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_wiov_add
 **功    能: 追加发送数据(无数据拷贝)
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将发送链表中的数据指针放到iov中.
 **注意事项: 数据发送完毕之后, 必须释放内存空间!
 **作    者: # Qifeng.zou # 2015.12.26 #
 ******************************************************************************/
static int rtmq_rsvr_wiov_add(rtmq_rsvr_t *rsvr, rtmq_sck_t *sck)
{
    int len;
    rtmq_header_t *head;
    wiov_t *send = &sck->send;

    /* > 从消息链表取数据 */
    while (!wiov_isfull(send)) {
        /* 1 是否有数据 */
        head = (rtmq_header_t *)list_lpop(sck->mesg_list);;
        if (NULL == head) {
            break; /* 无数据 */
        }
        else if (RTMQ_CHKSUM_VAL != head->chksum) { /* 合法性校验 */
            assert(0);
        }

        len = sizeof(rtmq_header_t) + head->length; /* 当前消息总长度 */

        /* 3 设置头部数据 */
        RTMQ_HEAD_HTON(head, head);

        /* 4 设置发送信息 */
        wiov_item_add(send, (char *)head, len, NULL, mem_ref_dealloc);
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_trav_send
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
static int rtmq_rsvr_trav_send(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    ssize_t n;
    wiov_t *send;
    rtmq_sck_t *curr;
    list2_node_t *node, *tail;

    rsvr->ctm = time(NULL);

    node = rsvr->conn_list->head;
    if (NULL != node) {
        tail = node->prev;
    }

    while (NULL != node) {
        curr = (rtmq_sck_t *)node->data;

        if (FD_ISSET(curr->fd, &rsvr->wrset)) {
            log_trace(ctx->log, "Stream is writable! fd:%d nid:%d sid:%d",
                    curr->fd, curr->nid, curr->sid);
            curr->wrtm = rsvr->ctm;
            send = &curr->send;

            for (;;) {
                /* 1. 追加发送内容 */
                if (!wiov_isfull(send)) {
                    rtmq_rsvr_wiov_add(rsvr, curr);
                } 

                if (wiov_isempty(send)) {
                    break;
                }

                /* 2. 发送缓存数据 */
                n = writev(curr->fd, wiov_item_begin(send), wiov_item_num(send));
                if (n < 0) {
                    log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
                    rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
                    return RTMQ_ERR;
                }
                else {
                    log_trace(ctx->log, "Stream is writable! fd:%d nid:%d sid:%d n:%d",
                            curr->fd, curr->nid, curr->sid, n);
                    /* 删除已发送内容 */
                    wiov_item_adjust(send, n);
                    break;
                }
            }
        }

        if (node == tail) {
            break;
        }

        node = node->next;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_recv_proc
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
static int rtmq_rsvr_recv_proc(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck)
{
    int n, left;
    rtmq_snap_t *recv = &sck->recv;

    while (1) {
        /* 1. 接收网络数据 */
        left = (int)(recv->end - recv->iptr);

        n = read(sck->fd, recv->iptr, left);
        if (n > 0) {
            recv->iptr += n;

            /* 2. 进行数据处理 */
            if (rtmq_rsvr_data_proc(ctx, rsvr, sck)) {
                log_error(rsvr->log, "Proc data failed! nid:%u", sck->nid);
                return RTMQ_ERR;
            }
            continue;
        }
        else if (0 == n) {
            log_info(rsvr->log, "Client disconnected. nid:%u n:%d/%d",
                    sck->nid, n, left);
            return RTMQ_SCK_DISCONN;
        }
        else if ((n < 0) && (EAGAIN == errno)) {
            return RTMQ_OK; /* Again */
        }
        else if (EINTR == errno) {
            continue;
        }

        log_error(rsvr->log, "errmsg:[%d] %s. nid:%u", errno, strerror(errno), sck->nid);
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_recv_post
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
static int rtmq_rsvr_data_proc(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck)
{
    bool flag = false;
    rtmq_header_t *head;
    uint32_t len, one_mesg_len;
    rtmq_snap_t *recv = &sck->recv;

    while (1) {
        flag = false;
        head = (rtmq_header_t *)recv->optr;

        len = (uint32_t)(recv->iptr - recv->optr);
        if (len >= sizeof(rtmq_header_t)) {
            if (RTMQ_CHKSUM_VAL != ntohl(head->chksum)) {
                log_error(rsvr->log, "Header is invalid! nid:%d Mark:%X/%X type:%d len:%d flag:%d",
                        ntohl(head->nid), ntohl(head->chksum), RTMQ_CHKSUM_VAL,
                        ntohl(head->type), ntohl(head->length), head->flag);
                return RTMQ_ERR;
            }

            one_mesg_len = sizeof(rtmq_header_t) + ntohl(head->length);
            if (len >= one_mesg_len) {
                flag = true;
            }
        }

        /* 1. 不足一条数据时 */
        if (!flag) {
            if (recv->iptr == recv->end) {
                /* 防止OverWrite的情况发生 */
                if ((recv->optr - recv->addr) < (recv->end - recv->iptr)) {
                    log_error(rsvr->log, "Data length is invalid!");
                    return RTMQ_ERR;
                }

                memcpy(recv->addr, recv->optr, len);
                recv->optr = recv->addr;
                recv->iptr = recv->optr + len;
                return RTMQ_OK;
            }
            return RTMQ_OK;
        }


        /* 2. 至少一条数据时 */
        /* 2.1 转化字节序 */
        RTMQ_HEAD_NTOH(head, head);

        /* 2.2 校验合法性 */
        if (!RTMQ_HEAD_ISVALID(head)) {
            ++rsvr->err_total;
            log_error(rsvr->log, "Header is invalid! Mark:%u/%u type:%d len:%d flag:%d",
                    head->chksum, RTMQ_CHKSUM_VAL, head->type, head->length, head->flag);
            return RTMQ_ERR;
        }

        /* 2.3 进行数据处理 */
        if (RTMQ_SYS_MESG == head->flag) {
            rtmq_rsvr_sys_mesg_proc(ctx, rsvr, sck, recv->optr);
        }
        else {
            rtmq_rsvr_exp_mesg_proc(ctx, rsvr, sck, recv->optr);
        }
        recv->optr += one_mesg_len;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_sys_mesg_proc
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
static int rtmq_rsvr_sys_mesg_proc(rtmq_cntx_t *ctx,
        rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr)
{
    rtmq_header_t *head = (rtmq_header_t *)addr;

    log_debug(rsvr->log, "type:%u nid:%u chksum:0x%X",
            head->type, head->nid, head->chksum);

    switch (head->type) {
        case RTMQ_CMD_LINK_AUTH_REQ:
            return rtmq_rsvr_link_auth_req_hdl(ctx, rsvr, sck, addr);
        case RTMQ_CMD_SUB_REQ:
            return rtmq_rsvr_sub_req_hdl(ctx, rsvr, sck, addr);
        case RTMQ_CMD_KPALIVE_REQ:
            return rtmq_rsvr_keepalive_req_hdl(ctx, rsvr, sck, addr);
        default:
            log_error(rsvr->log, "Unknown message type! [%d]", head->type);
            return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_exp_mesg_proc
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
static int rtmq_rsvr_exp_mesg_proc(rtmq_cntx_t *ctx,
        rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *data)
{
    int rqid, len;
    void *addr;
    rtmq_header_t *head = (rtmq_header_t *)data;

    ++rsvr->recv_total; /* 总数 */
    len = sizeof(rtmq_header_t) + head->length;

    /* > 合法性验证 */
    if (head->nid != sck->nid) {
        ++rsvr->drop_total;
        log_error(rsvr->log, "Devid isn't right! nid:%d/%d", head->nid, sck->nid);
        return RTMQ_ERR;
    }

    /* > 从队列申请空间 */
    rqid = rand() % ctx->conf.recvq_num;

    addr = queue_malloc(ctx->recvq[rqid], len);
    if (NULL == addr) {
        ++rsvr->drop_total; /* 丢弃计数 */
        rtmq_rsvr_cmd_proc_all_req(ctx, rsvr);

        log_error(rsvr->log, "Alloc from queue failed! recv:%llu drop:%llu error:%llu len:%d/%d",
                rsvr->recv_total, rsvr->drop_total, rsvr->err_total, len, queue_size(ctx->recvq[rqid]));
        return RTMQ_ERR;
    }

    /* > 进行数据拷贝 */
    memcpy(addr, data, len);

    queue_push(ctx->recvq[rqid], addr);         /* 放入处理队列 */

    rtmq_rsvr_cmd_proc_req(ctx, rsvr, rqid);    /* 发送处理请求 */

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_event_core_hdl
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
static int rtmq_rsvr_event_core_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    /* 1. 接收命令数据 */
    if (FD_ISSET(rsvr->cmd_sck_id, &rsvr->rdset)) {
        rtmq_rsvr_recv_cmd(ctx, rsvr);
    }

    /* 2. 遍历接收数据 */
    rtmq_rsvr_trav_recv(ctx, rsvr);

    /* 3. 遍历发送数据 */
    rtmq_rsvr_trav_send(ctx, rsvr);

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_event_timeout_hdl
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
static int rtmq_rsvr_event_timeout_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    bool is_end = false;
    rtmq_sck_t *curr;
    list2_node_t *node, *next, *tail;

    rsvr->ctm = time(NULL);

    /* > 检测超时连接 */
    node = rsvr->conn_list->head;
    if (NULL == node) {
        return RTMQ_OK;
    }

    tail = node->prev;
    while ((NULL != node) && (false == is_end)) {
        if (tail == node) {
            is_end = true;
        }

        curr = (rtmq_sck_t *)node->data;

        if (rsvr->ctm - curr->rdtm >= 60) {
            log_trace(rsvr->log, "Didn't active for along time! fd:%d ip:%s",
                    curr->fd, curr->ipaddr);
            /* 释放数据 */
            FREE(curr->recv.addr);
            /* 删除连接 */
            if (node == tail) {
                rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
                break;
            }

            next = node->next;
            rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
            node = next;
            continue;
        }
        node = node->next;
    }

    /* > 重复发送处理命令 */
    rtmq_rsvr_cmd_proc_all_req(ctx, rsvr);

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_keepalive_req_hdl
 **功    能: 保活请求处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收服务
 **     sck: 套接字对象
 **     addr: 请求地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtmq_rsvr_keepalive_req_hdl(rtmq_cntx_t *ctx,
        rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr)
{
    void *rsp;
    rtmq_header_t *head;

    /* > 分配消息空间 */
    rsp = (void *)mem_ref_alloc(sizeof(rtmq_header_t), NULL,
            (mem_alloc_cb_t)mem_alloc, (mem_dealloc_cb_t)mem_dealloc);
    if (NULL == rsp) {
        log_error(rsvr->log, "Alloc memory failed!");
        return RTMQ_ERR;
    }

    /* > 回复消息内容 */
    head = (rtmq_header_t *)rsp;

    head->type = RTMQ_CMD_KPALIVE_RSP;
    head->nid = ctx->conf.nid;
    head->length = 0;
    head->flag = RTMQ_SYS_MESG;
    head->chksum = RTMQ_CHKSUM_VAL;

    /* > 加入发送列表 */
    if (list_rpush(sck->mesg_list, rsp)) {
        mem_ref_decr(rsp);
        log_error(rsvr->log, "Insert into list failed!");
        return RTMQ_ERR;
    }

    log_debug(rsvr->log, "Add respond of keepalive request!");

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_link_auth_rsp
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
static int rtmq_rsvr_link_auth_rsp(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck)
{
    int len;
    void *addr;
    rtmq_header_t *head;
    rtmq_link_auth_rsp_t *link_auth_rsp;

    /* > 分配消息空间 */
    len = sizeof(rtmq_header_t) + sizeof(rtmq_link_auth_rsp_t);

    addr = (void *)mem_ref_alloc(len, NULL,
            (mem_alloc_cb_t)mem_alloc,
            (mem_dealloc_cb_t)mem_dealloc);
    if (NULL == addr) {
        log_error(rsvr->log, "Alloc memory failed!");
        return RTMQ_ERR;
    }

    /* > 回复消息内容 */
    head = (rtmq_header_t *)addr;
    link_auth_rsp = (rtmq_link_auth_rsp_t *)(head + 1);

    head->type = RTMQ_CMD_LINK_AUTH_RSP;
    head->nid = ctx->conf.nid;
    head->length = sizeof(rtmq_link_auth_rsp_t);
    head->flag = RTMQ_SYS_MESG;
    head->chksum = RTMQ_CHKSUM_VAL;

    link_auth_rsp->is_succ = htonl(sck->auth_succ);

    /* > 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr)) {
        mem_ref_decr(addr);
        log_error(rsvr->log, "Insert into list failed!");
        return RTMQ_ERR;
    }

    log_debug(rsvr->log, "Add respond of link-auth request!");

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_link_auth_req_hdl
 **功    能: 链路鉴权请求处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 套接字对象
 **     addr: 请求地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 校验鉴权是否通过, 并应答鉴权请求
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int rtmq_rsvr_link_auth_req_hdl(rtmq_cntx_t *ctx,
        rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr)
{
    rtmq_header_t *head;
    rtmq_link_auth_req_t *link_auth_req;

    head = (rtmq_header_t *)addr;
    link_auth_req = (rtmq_link_auth_req_t *)(head + 1);

    /* > 验证鉴权合法性 */
    sck->auth_succ = rtmq_link_auth_check(ctx, link_auth_req);
    if (sck->auth_succ) {
        sck->nid = head->nid;
        /* > 插入DEV与SCK的映射 */
        if (rtmq_node_to_svr_map_add(ctx, head->nid, rsvr->id)) {
            log_error(rsvr->log, "Insert into sck2dev table failed! fd:%d serial:%ld nid:%d",
                    sck->fd, sck->sid, head->nid);
            return RTMQ_ERR;
        }
    }

    /* > 应答鉴权请求 */
    return rtmq_rsvr_link_auth_rsp(ctx, rsvr, sck);
}

/******************************************************************************
 **函数名称: rtmq_sub_list_creat
 **功    能: 创建订阅列表
 **输入参数:
 **     type: 消息类型
 **输出参数: NONE
 **返    回: 订阅列表
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 07:07:26 #
 ******************************************************************************/
static rtmq_sub_list_t *rtmq_sub_list_creat(mesg_type_e type)
{
    rtmq_sub_list_t *list;

    list = (rtmq_sub_list_t *)calloc(1, sizeof(rtmq_sub_list_t));
    if (NULL == list) {
        return NULL;
    }

    list->nodes = (vector_t *)vector_creat(RTMQ_SUB_VEC_LEN, RTMQ_SUB_VEC_INCR);
    if (NULL == list->nodes) {
        free(list);
        return NULL;
    }

    list->type = type;

    return list;
}

/* Free sub list memory */
static void rtmq_sub_list_free(rtmq_sub_list_t *list)
{
    vector_destroy(list->nodes, mem_dealloc, NULL);
    free(list);
}

static bool rtmq_find_node_for_vec_cb(rtmq_sub_node_t *node, uint64_t *sid)
{
    return (node->sid == *sid)? true : false;
}

static int rtmq_rsvr_sub_find_or_add(rtmq_cntx_t *ctx, rtmq_sck_t *sck, int type)
{
    int ret;
    rtmq_sub_mgr_t *sub;
    rtmq_sub_list_t *list;
    rtmq_sub_node_t *node;

    /* > Find or add sub node */
    sub = &ctx->sub_mgr;
    pthread_rwlock_wrlock(&sub->lock);
    do {
        list = (rtmq_sub_list_t *)avl_query(sub->tab, &type, sizeof(type));
        if (NULL == list) {
            list = (rtmq_sub_list_t *)rtmq_sub_list_creat(type);
            if (NULL == list) {
                log_error(ctx->log, "Create sub list failed!");
                break;
            }

            if (avl_insert(sub->tab, &type, sizeof(type), (void *)list)) {
                rtmq_sub_list_free(list);
                log_error(ctx->log, "Insert sub table failed!");
                break;
            }
        }

        node = vector_find(list->nodes, (find_cb_t)rtmq_find_node_for_vec_cb, (void *)&sck->sid);
        if (NULL != node) {
            ret = RTMQ_OK;
            break;
        }

        /* > Add new node */
        node = (rtmq_sub_node_t *)calloc(1, sizeof(rtmq_sub_node_t));
        if (NULL == node) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        node->sid = sck->sid;
        node->nid = sck->nid;

        if (vector_append(list->nodes, (void *)node)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            free(node);
            break;
        }
        ret = RTMQ_OK;
    } while(0);
    pthread_rwlock_unlock(&sub->lock);

    log_debug(ctx->log, "Sub req handler! type:%u ret:%d", type, ret);

    return ret;
}

/* 删除订阅数据 */
static int rtmq_sub_del(rtmq_cntx_t *ctx, rtmq_sck_t *sck, int type)
{
    void *addr;
    rtmq_sub_mgr_t *sub;
    rtmq_sub_list_t *list;
    rtmq_sub_node_t *node;

    sub = &ctx->sub_mgr;

    pthread_rwlock_wrlock(&sub->lock);
    do {
        list = (rtmq_sub_list_t *)avl_query(sub->tab, &type, sizeof(type));
        if (NULL == list) {
            break;
        }

        node = vector_find(list->nodes, (find_cb_t)rtmq_find_node_for_vec_cb, (void *)&sck->sid);
        if (NULL == node) {
            break;
        }

        vector_delete(list->nodes, node);
        free(node);

        if (0 == vector_len(list->nodes)) {
            avl_delete(sub->tab, &type, sizeof(type), &addr);
            free(list);
        }
    } while(0);
    pthread_rwlock_unlock(&sub->lock);

    return 0;
}

/* 添加订阅数据 */
static int rtmq_rsvr_sck_add_sub(rtmq_cntx_t *ctx, rtmq_sck_t *sck, int type)
{
    rtmq_sub_req_t *req;

    req = (rtmq_sub_req_t *)avl_query(sck->sub_list, &type, sizeof(type));
    if (NULL != req) {
        log_warn(ctx->log, "Socket sub [%u] repeat!", type);
        return 0;
    }

    req = (rtmq_sub_req_t *)calloc(1, sizeof(rtmq_sub_req_t));
    if (NULL == req) {
        return -1;
    }

    req->type = type;

    if (avl_insert(sck->sub_list, &type, sizeof(type), (void *)req)) {
        free(req);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_sub_req_hdl
 **功    能: 订阅请求处理
 **输入参数:
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 套接字对象
 **     addr: 请求地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.04.13 00:35:15 #
 ******************************************************************************/
static int rtmq_rsvr_sub_req_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_sck_t *sck, void *addr)
{
    rtmq_header_t *head = (rtmq_header_t *)addr;
    rtmq_sub_req_t *req = (rtmq_sub_req_t *)(head + 1);

    /* > Net to host */
    RTMQ_SUB_REQ_NTOH(req, req);

    /* > Find or add sub node */
    if (rtmq_rsvr_sub_find_or_add(ctx, sck, req->type)) {
        log_debug(ctx->log, "Sub find or add failed! type:%u", req->type);
        return -1;
    }

    /* > Add item into socket sub list */
    if (rtmq_rsvr_sck_add_sub(ctx, sck, req->type)) {
        rtmq_sub_del(ctx, sck, req->type);
        log_error(ctx->log, "Add item into sub list failed! type:%u", req->type);
        return -1;
    }

    log_debug(ctx->log, "Sub req handler success! type:%u", req->type);

    return 0;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_sck_creat
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
static rtmq_sck_t *rtmq_rsvr_sck_creat(rtmq_rsvr_t *rsvr, rtmq_cmd_add_sck_t *req)
{
    void *addr;
    rtmq_sck_t *sck;

    /* > 分配连接空间 */
    sck = (rtmq_sck_t *)calloc(1, sizeof(rtmq_sck_t));
    if (NULL == sck) {
        log_error(rsvr->log, "Alloc memory failed!");
        CLOSE(req->sckid);
        return NULL;
    }

    memset(sck, 0, sizeof(rtmq_sck_t));

    sck->fd = req->sckid;
    sck->nid = -1;
    sck->sid = req->sid;
    sck->ctm = time(NULL);
    sck->rdtm = sck->ctm;
    sck->wrtm = sck->ctm;
    snprintf(sck->ipaddr, sizeof(sck->ipaddr), "%s", req->ipaddr);

    do {
        /* > 创建订阅列表 */
        sck->sub_list = avl_creat(NULL, (key_cb_t)key_cb_int32, (cmp_cb_t)cmp_cb_int32);
        if (NULL == sck->sub_list) {
            log_error(rsvr->log, "Create sub list failed!");
            break;
        }

        /* > 创建发送链表 */
        sck->mesg_list = list_creat(NULL);
        if (NULL == sck->mesg_list) {
            log_error(rsvr->log, "Create list failed!");
            break;
        }

        /* > 申请接收缓存 */
        addr = (void *)calloc(1, RTMQ_BUFF_SIZE);
        if (NULL == addr) {
            log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        rtmq_snap_setup(&sck->recv, addr, RTMQ_BUFF_SIZE);

        return sck;
    } while (0);

    /* > 释放套接字对象 */
    rtmq_rsvr_sck_free(rsvr, sck);
    return NULL;
}

typedef struct {
    rtmq_sck_t *sck;
    rtmq_cntx_t *ctx;
} rtmq_rsvr_sck_sub_trav_t;

int rtmq_rsvr_sck_sub_item_free(rtmq_rsvr_sck_sub_trav_t *args, rtmq_sub_req_t *req)
{
    rtmq_sub_list_t *list;
    rtmq_sub_node_t *node;
    rtmq_sck_t *sck = args->sck;
    rtmq_cntx_t *ctx = args->ctx;
    rtmq_sub_mgr_t *sub = &ctx->sub_mgr;

    pthread_rwlock_wrlock(&sub->lock);
    do {
        list = avl_query(sub->tab, &req->type, sizeof(req->type));
        if (NULL == list) {
            break;
        }

        node = vector_find(list->nodes, (find_cb_t)rtmq_find_node_for_vec_cb, &sck->sid);
        if (NULL == node) {
            break;
        }

        vector_delete(list->nodes, node);
        free(node);
        free(req);
    } while(0);
    pthread_rwlock_unlock(&sub->lock);

    return 0;
}

static int rtmq_rsvr_sck_sub_free(rtmq_rsvr_t *rsvr, rtmq_sck_t *sck)
{
    rtmq_rsvr_sck_sub_trav_t args;
    rtmq_cntx_t *ctx = (rtmq_cntx_t *)rsvr->ctx;

    args.sck = sck;
    args.ctx = ctx;

    if (sck->sub_list) {
        avl_destroy(sck->sub_list, (mem_dealloc_cb_t)rtmq_rsvr_sck_sub_item_free, &args);
    }

    return 0;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_sck_free
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
static void rtmq_rsvr_sck_free(rtmq_rsvr_t *rsvr, rtmq_sck_t *sck)
{
    if (NULL == sck) { return; }

    FREE(sck->recv.addr);

    /* 释放订阅列表空间 */
    rtmq_rsvr_sck_sub_free(rsvr, sck);

    /* 释放发送链表空间 */
    if (sck->mesg_list) {
        list_destroy(sck->mesg_list, (mem_dealloc_cb_t)mem_dealloc, NULL);
    }

    /* 释放iov的空间 */
    wiov_destroy(&sck->send);

    CLOSE(sck->fd);
    FREE(sck);
}

/******************************************************************************
 **函数名称: rtmq_rsvr_add_conn_hdl
 **功    能: 添加网络连接
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将套接字对象加入到套接字链表中
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int rtmq_rsvr_add_conn_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, rtmq_cmd_add_sck_t *req)
{
    rtmq_sck_t *sck;
    rtmq_conf_t *conf = &ctx->conf;

    /* > 创建套接字对象 */
    sck = rtmq_rsvr_sck_creat(rsvr, req);
    if (NULL == sck) {
        log_error(rsvr->log, "Create socket object failed!");
        return RTMQ_ERR;
    }

    /* > 加入套接字链尾 */
    if (list2_rpush(rsvr->conn_list, (void *)sck)) {
        log_error(rsvr->log, "Insert into list failed!");
        rtmq_rsvr_sck_free(rsvr, sck);
        return RTMQ_ERR;
    }

    /* > 初始化发送IOV */
    if (wiov_init(&sck->send, 2 * conf->sendq.max)) {
        log_error(rsvr->log, "Init wiov failed!");
        rtmq_rsvr_sck_free(rsvr, sck);
        return RTMQ_ERR;
    }

    ++rsvr->connections; /* 统计TCP连接数 */

    log_trace(rsvr->log, "Tidx [%d] insert sckid [%d] success! ip:%s",
            rsvr->id, req->sckid, req->ipaddr);

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_del_conn_hdl
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
static int rtmq_rsvr_del_conn_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, list2_node_t *node)
{
    rtmq_sck_t *curr = (rtmq_sck_t *)node->data;

    /* > 从链表剔除结点 */
    list2_delete(rsvr->conn_list, node);

    /* > 从SCK <<=>> DEV映射表中剔除 */
    rtmq_node_to_svr_map_del(ctx, curr->nid, rsvr->id);

    /* > 释放数据空间 */
    rtmq_rsvr_sck_free(rsvr, curr);

    --rsvr->connections; /* 统计TCP连接数 */

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_del_all_conn_hdl
 **功    能: 删除接收线程所有的套接字
 **输入参数:
 **     rsvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void rtmq_rsvr_del_all_conn_hdl(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    list2_node_t *node, *next, *tail;

    node = rsvr->conn_list->head;
    if (NULL != node) {
        tail = node->prev;
    }

    while (NULL != node) {
        if (node == tail) {
            rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
            break;
        }

        next = node->next;
        rtmq_rsvr_del_conn_hdl(ctx, rsvr, node);
        node = next;
    }

    rsvr->connections = 0; /* 统计TCP连接数 */
    return;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_cmd_proc_req
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
static int rtmq_rsvr_cmd_proc_req(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr, int rqid)
{
    int widx;
    rtmq_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    rtmq_conf_t *conf = &ctx->conf;
    rtmq_cmd_proc_req_t *req = (rtmq_cmd_proc_req_t *)&cmd.param;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTMQ_CMD_PROC_REQ;
    req->ori_svr_id = rsvr->id;
    req->num = -1;
    req->rqidx = rqid;

    /* 1. 随机选择Work线程 */
    /* widx = rtmq_rand_work(ctx); */
    widx = rqid / RTMQ_WORKER_HDL_QNUM;

    rtmq_worker_usck_path(conf, path, widx);

    /* 2. 发送处理命令 */
    if (unix_udp_send(rsvr->cmd_sck_id, path, &cmd, sizeof(rtmq_cmd_t)) < 0) {
        if (EAGAIN != errno) {
            log_error(rsvr->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                      errno, strerror(errno), path);
        }
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_cmd_proc_all_req
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
static int rtmq_rsvr_cmd_proc_all_req(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
    int idx;

    /* 依次遍历滞留总数 */
    for (idx=0; idx<ctx->conf.recvq_num; ++idx) {
        rtmq_rsvr_cmd_proc_req(ctx, rsvr, idx);
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_rsvr_get_conn_list_by_nodeid
 **功    能: 通过结点ID获取连接链表
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
    uint32_t nid;               /* 结点ID */
    list_t *list;               /* 拥有相同结点ID的套接字链表 */
} _conn_list_t;

static int rtmq_rsvr_get_conn_list_by_nodeid(rtmq_sck_t *sck, _conn_list_t *cl)
{
    if (sck->nid != cl->nid) {
        return -1;
    }

    return list_rpush(cl->list, sck); /* 注意: 销毁cl->list时, 不必释放sck空间 */
}

/******************************************************************************
 **函数名称: rtmq_rsvr_dist_data
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
static int rtmq_rsvr_dist_data(rtmq_cntx_t *ctx, rtmq_rsvr_t *rsvr)
{
#define RTRD_POP_MAX_NUM (1024)
    int idx, num;
    ring_t *sendq;
    void *data[RTRD_POP_MAX_NUM];
    rtmq_frwd_t *frwd;
    rtmq_sck_t *sck;
    _conn_list_t cl;

    sendq = ctx->sendq[rsvr->id];

    while (1) {
        /* > 弹出队列数据 */
        num = MIN(ring_get_num(sendq), RTRD_POP_MAX_NUM);
        if (0 == num) {
            break;
        }

        num = ring_mpop(sendq, data, num);
        if (0 == num) {
            continue;
        }

        log_trace(ctx->log, "Multi-pop num:%d!", num);

        /* > 逐条处理数据 */
        for (idx=0; idx<num; ++idx) {
            frwd = (rtmq_frwd_t *)data[idx];

            /* > 查找发送连接 */
            cl.nid = frwd->dest;
            cl.list = list_creat(NULL);
            if (NULL == cl.list) {
                mem_ref_decr(data[idx]);
                log_error(rsvr->log, "Create list failed!");
                continue;
            }

            list2_trav(rsvr->conn_list, (trav_cb_t)rtmq_rsvr_get_conn_list_by_nodeid, &cl);
            if (0 == cl.list->num) {
                mem_ref_decr(data[idx]);
                list_destroy(cl.list, mem_dummy_dealloc, NULL);
                log_error(rsvr->log, "Didn't find connection by nid [%d]!", cl.nid);
                continue;
            }

            sck = (rtmq_sck_t *)list_fetch(cl.list, rand()%cl.list->num);
            
            /* > 回收内存空间[注: 无需释放结点数据空间] */
            list_destroy(cl.list, mem_dummy_dealloc, NULL);

            log_trace(ctx->log, "Select upstream! fd:%d nid:%d sid:%d",
                    sck->fd, sck->nid, sck->sid);


            /* > 放入发送链表 */
            if (list_rpush(sck->mesg_list, data[idx])) {
                mem_ref_decr(data[idx]);
                log_error(rsvr->log, "Push input list failed!");
                continue;
            }
        }
    }

    return RTMQ_OK;
}

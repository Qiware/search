#include <memory.h>
#include <assert.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "smti.h"
#include "xml_tree.h"
#include "smti_cmd.h"
#include "smti_comm.h"
#include "smti_recv.h"
#include "thread_pool.h"

/* 静态函数 */
static smti_rsvr_t *smti_rsvr_get_curr(smti_cntx_t *ctx);
static int smti_rsvr_init(smti_cntx_t *ctx, smti_rsvr_t *rsvr);
static int smti_rsvr_event_core_hdl(smti_cntx_t *ctx, smti_rsvr_t *rsvr);
static int smti_rsvr_event_timeout_hdl(smti_cntx_t *ctx, smti_rsvr_t *rsvr);

static int smti_rsvr_trav_recv(smti_cntx_t *ctx, smti_rsvr_t *rsvr);
static int smti_rsvr_trav_send(smti_cntx_t *ctx, smti_rsvr_t *rsvr);

static int smti_rsvr_recv_proc(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_read_init(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_recv_header(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_recv_body(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);

static int smti_rsvr_check_header(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static void smti_rsvr_read_release(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);

static int smti_rsvr_proc_data(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_sys_data_proc(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_exp_data_proc(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);

static int smti_rsvr_link_info_report_hdl(smti_cntx_t *ctx, smti_sck_t *sck);
static int smti_rsvr_keepalive_req_hdl(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_send_work_cmd(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck);
static void smti_rsvr_send_work_all_cmd(smti_cntx_t *ctx, smti_rsvr_t *rsvr);

static int smti_rsvr_add_sck_hdl(smti_rsvr_t *rsvr, smti_cmd_add_sck_t *req);
static int smti_rsvr_del_sck_hdl(smti_rsvr_t *rsvr, smti_sck_t *sck);
static void smti_rsvr_del_all_sck_hdl(smti_rsvr_t *rsvr);

static int smti_rsvr_add_msg(smti_rsvr_t *rsvr, smti_sck_t *sck, void *addr);
static void *smti_rsvr_get_msg(smti_rsvr_t *rsvr, smti_sck_t *sck);
static int smti_rsvr_clear_msg(smti_rsvr_t *rsvr, smti_sck_t *sck);

/* Random select a rsvr thread */
#define smti_rand_recv(ctx) ((ctx)->listen.total++ % (ctx->recvtp->num))
/* Random select a work thread */
#define smti_rand_work(ctx) (rand() % (ctx->worktp->num))

/******************************************************************************
 **函数名称: smti_rsvr_set_rdset
 **功    能: 设置可读集合
 **输入参数: 
 **     rsvr: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     如果超时未接收或发送数据，则关闭连接!
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
#define smti_rsvr_set_rdset(rsvr) \
{ \
    smti_sck_t *curr, *next; \
    \
    FD_ZERO(&rsvr->rdset); \
    \
    FD_SET(rsvr->cmd_sck_id, &rsvr->rdset); \
    rsvr->max = rsvr->cmd_sck_id; \
    \
    curr = rsvr->sck; \
    while (NULL != curr) \
    { \
        if ((rsvr->ctm - curr->rtm > 30) \
            && (rsvr->ctm - curr->wtm > 30)) \
        { \
            log_trace(rsvr->log, "Didn't active for along time! fd:%d ip:%s", \
                    curr->fd, curr->ipaddr); \
            \
            next = curr->next; \
            smti_rsvr_del_sck_hdl(rsvr, curr); \
            \
            curr = next; \
            continue; \
        } \
        \
        FD_SET(curr->fd, &rsvr->rdset); \
        rsvr->max = (rsvr->max > curr->fd)? rsvr->max : curr->fd; \
        \
        curr = curr->next; \
    } \
}

/******************************************************************************
 **函数名称: smti_rsvr_set_wrset
 **功    能: 设置可写集合
 **输入参数: 
 **     rsvr: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     只有发送链表中存在数据时，才将该套接字加入到可写侦听集合!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
#define smti_rsvr_set_wrset(rsvr) \
{ \
    smti_sck_t *curr; \
    \
    FD_ZERO(&rsvr->wrset); \
    \
    curr = rsvr->sck; \
    while (NULL != curr) \
    { \
        if (NULL == curr->message_list \
            && NULL == curr->send.addr) \
        { \
            curr = curr->next; \
            continue; \
        } \
        \
        FD_SET(curr->fd, &rsvr->wrset); \
        \
        curr = curr->next; \
    } \
}

/******************************************************************************
 **函数名称: smti_rsvr_routine
 **功    能: 运行接收服务线程
 **输入参数: 
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 获取接收对象
 **     2. 等待事件通知
 **     3. 进行事件处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
void *smti_rsvr_routine(void *_ctx)
{
    int ret;
    smti_rsvr_t *rsvr;
    struct timeval timeout;
    smti_cntx_t *ctx = (smti_cntx_t *)_ctx;

    /* 1. 获取接收对象 */
    rsvr = smti_rsvr_get_curr(ctx);
    if (NULL == rsvr)
    {
        log_fatal(rsvr->log, "Get recv server failed!");
        abort();
        return (void *)SMTI_ERR;
    }

    for (;;)
    {
        /* 2. 等待事件通知 */
        smti_rsvr_set_rdset(rsvr);
        smti_rsvr_set_wrset(rsvr);

        timeout.tv_sec = SMTI_TMOUT_SEC;
        timeout.tv_usec = SMTI_TMOUT_USEC;
        ret = select(rsvr->max+1, &rsvr->rdset, &rsvr->wrset, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)SMTI_ERR;
        }
        else if (0 == ret)
        {
            smti_rsvr_event_timeout_hdl(ctx, rsvr);
            continue;
        }

        /* 3. 进行事件处理 */
        smti_rsvr_event_core_hdl(ctx, rsvr);
    }

    log_fatal(rsvr->log, "errmsg:[%d] %s", errno, strerror(errno));
    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: smti_rsvr_get_curr
 **功    能: 获取当前线程对应的接收对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 当前接收对象
 **实现描述: 
 **     1. 获取当前线程的索引
 **     2. 返回当前线程对应的接收对象
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static smti_rsvr_t *smti_rsvr_get_curr(smti_cntx_t *ctx)
{
    int tidx;

    /* 1. 获取当前线程的索引 */
    tidx = thread_pool_get_tidx(ctx->recvtp);
    if (tidx < 0)
    {
        log_error(rsvr->log, "Get index of current thread failed!");
        return NULL;
    }

    /* 2. 返回当前线程对应的接收对象 */
    return (smti_rsvr_t *)(ctx->recvtp->data + tidx * sizeof(smti_rsvr_t));
}

/******************************************************************************
 **函数名称: smti_rsvr_init
 **功    能: 初始化接收对象
 **输入参数: 
 **     ctx: 全局对象
 **输出参数:
 **     rsvr: 接收对象
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 获取当前线程的索引
 **     2. 返回当前线程对应的接收对象
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_init(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    char path[FILE_PATH_MAX_LEN];
    smti_conf_t *conf = &ctx->conf;

    rsvr->log = ctx->log;
    rsvr->ctm = time(NULL);

    /* 1. 创建各队列滞留条数数组 */
    rsvr->delay_total = calloc(ctx->conf.rqnum, sizeof(uint64_t));
    if (NULL == rsvr->delay_total)
    {
        log_fatal(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTI_ERR;
    }

    /* 2. 创建CMD套接字 */
    smti_rsvr_usck_path(conf, path, rsvr->tidx);
    
    rsvr->cmd_sck_id = usck_udp_creat(path);
    if (rsvr->cmd_sck_id < 0)
    {
        log_error(rsvr->log, "Create unix-udp socket failed!");
        return SMTI_ERR;
    }

    /* 3. 创建SLAB内存池 */
    rsvr->pool = slab_init(SMTI_MEM_POOL_SIZE)
    if (NULL == rsvr->pool)
    {
        log_error(rsvr->log, "Initialize slab mem-pool failed!");
        return SMTI_ERR;
    }

    rsvr->sck = NULL;
    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_recv_cmd
 **功    能: 接收命令数据
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收命令数据
 **     2. 进行命令处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_recv_cmd(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    smti_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令数据 */
    if (usck_udp_recv(rsvr->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(rsvr->log, "Recv command failed!");
        return SMTI_ERR_RECV_CMD;
    }

    /* 2. 进行命令处理 */
    switch (cmd.type)
    {
        case SMTI_CMD_ADD_SCK:
        {
            return smti_rsvr_add_sck_hdl(rsvr, (smti_cmd_add_sck_t *)&cmd.args);
        }
        default:
        {
            log_error(rsvr->log, "Unknown command! type:%d", cmd.type);
            return SMTI_ERR_UNKNOWN_CMD;
        }
    }

    return SMTI_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: smti_rsvr_trav_recv
 **功    能: 遍历接收数据
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     遍历判断套接字是否可读，并接收数据!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_trav_recv(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    smti_sck_t *curr, *next;

    rsvr->ctm = time(NULL);

    curr = rsvr->sck;
    while (NULL != curr)
    {
        if (FD_ISSET(curr->fd, &rsvr->rdset))
        {
            curr->rtm = rsvr->ctm;

            /* Recv data */
            if (smti_rsvr_recv_proc(ctx, recv, curr))
            {
                log_error(rsvr->log, "Read proc failed! fd:%d ip:%s", curr->fd, curr->ipaddr);
                next = curr->next;
                smti_rsvr_del_sck_hdl(rsvr, curr);
                curr = next;
                continue;
            }
        }

        curr = curr->next;
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_trav_send
 **功    能: 遍历发送数据
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     遍历判断套接字是否可写，并发送数据!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_trav_send(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    int n;
    smti_header_t *head;
    smti_sck_t *curr;
    smti_send_snap_t *send;

    rsvr->ctm = time(NULL);
    curr = rsvr->sck;

    while (NULL != curr)
    {
        if (FD_ISSET(curr->fd, &rsvr->wrset))
        {
            curr->wtm = rsvr->ctm;
            send = &curr->send;

            for (;;)
            {
                /* 1. 获取需要发送的数据 */
                if (NULL == send->addr)
                {
                    send->addr = smti_rsvr_get_msg(rsvr, curr);
                    if (NULL == send->addr)
                    {
                        break;
                    }

                    head = (smti_header_t *)send->addr;

                    send->loc = SMTI_DATA_LOC_SLAB;
                    send->off = 0;
                    send->total = head->length + sizeof(smti_header_t);
                    send->left = send->total;
                }

                /* 2. 发送数据 */
                n = Writen(curr->fd, send->addr+send->off, send->left);
                if (n != (int) send->left)
                {
                    if (n > 0)
                    {
                        send->off += n;
                        send->left -= n;
                        break;
                    }

                    log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));

                    if (SMTI_DATA_LOC_SLAB == send->loc)
                    {
                        slab_dealloc(rsvr->pool, send->addr);
                    }

                    smti_reset_send_snap(send);

                    /* 关闭套接字　并清空发送队列 */
                    smti_rsvr_del_sck_hdl(rsvr, curr);
                    return SMTI_ERR;
                }

                /* 3. 释放空间 */
                if (SMTI_DATA_LOC_SLAB == send->loc)
                {
                    slab_dealloc(rsvr->pool, send->addr);
                }

                smti_reset_send_snap(send);
            }
        }

        curr = curr->next;
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_recv_proc
 **功    能: 接收数据并做相应处理
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化接收
 **     2. 接收数据头
 **     3. 接收数据体
 **     4. 进行数据处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_recv_proc(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    int ret;
    smti_header_t *head;
    smti_read_snap_t *read = &sck->read;

    switch (read->phase)
    {
        /* 1. 初始化接收 */
        case SMTI_PHASE_READ_INIT:
        {
            if (smti_rsvr_read_init(ctx, recv, sck))
            {
                log_error(rsvr->log, "Init read failed!");
                return SMTI_ERR;
            }

            /* 注意: 继续后续处理, 不执行break语句... */
        }
        /* 2. 接收数据头 */
        case SMTI_PHASE_READ_HEAD:
        {
            ret = smti_rsvr_recv_header(ctx, recv, sck);
            if (SMTI_DONE == ret)
            {
                head = (smti_header_t *)read->addr;
                if (head->length > 0)
                {
                    smti_set_read_phase(read, SMTI_PHASE_READ_BODY);
                    return SMTI_OK;
                }

                smti_set_read_phase(read, SMTI_PHASE_READ_POST);
                goto PHASE_READ_POST;
            }
            else if (SMTI_AGAIN == ret)  /* incomplete */
            {
                /* Note: Continue rsvr head at next loop */
                return SMTI_OK;
            }
            else if (SMTI_SCK_CLOSED == ret)
            {
                log_debug(rsvr->log, "Client disconnect!");
                break;
            }
            else
            {
                log_error(rsvr->log, "Recv head failed!");
                break; /* error */
            }
            /* 注意: 继续后续处理, 不执行break语句... */
        }
        /* 3. 接收数据体 */
        case SMTI_PHASE_READ_BODY:
        {
            ret = smti_rsvr_recv_body(ctx, recv, sck);
            if (SMTI_DONE == ret)
            {
                /* NULL;  Note: Continue handle */
            }
            else if (SMTI_AGAIN == ret)
            {
                /* Note: Continue rsvr body at next loop */
                return SMTI_OK;
            }
            else if (SMTI_HDL_DISCARD == ret)
            {
                smti_rsvr_read_release(ctx, recv, sck);
                return SMTI_OK;
            }
            else if (SMTI_SCK_CLOSED == ret)
            {
                log_debug(rsvr->log, "Client disconnect!");
                break;
            }
            else
            {
                log_error(rsvr->log, "Recv body failed!");
                break; /* error */
            }
            /* 注意: 继续后续处理, 不执行break语句... */
        }
        /* 4. 进行数据处理 */
        case SMTI_PHASE_READ_POST:
        {
        PHASE_READ_POST:
            ret = smti_rsvr_proc_data(ctx, recv, sck);
            if (SMTI_OK == ret)
            {
                smti_reset_read_snap(read);
                return SMTI_OK;
            }
            else if ((SMTI_HDL_DONE == ret)
                || (SMTI_HDL_DISCARD == ret))
            {
                smti_rsvr_read_release(ctx, recv, sck);
                return SMTI_OK;
            }

            break;
        }
        default:
        {
            log_error(rsvr->log, "Unknown phase!");
            break;
        }
    }

    smti_rsvr_read_release(ctx, recv, sck);

    return SMTI_ERR;
}

/******************************************************************************
 **函数名称: smti_rsvr_read_init
 **功    能: 初始化数据接收
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 选择接收队列
 **     2. 为新数据申请空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_read_init(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
#define SMTI_RETRY_TIMES    (3)
    int times = 0;
    smti_read_snap_t *read = &sck->read;

AGAIN:
    /* 1. 随机选择Recv队列 */
    read->rqidx = rand() % ctx->conf.rqnum;

    /* 2. 从队列申请空间 */
    read->dataid = orm_queue_data_malloc(ctx->recvq[read->rqidx], &read->addr);
    if (NULLID == read->dataid)
    {
        smti_rsvr_send_work_all_cmd(ctx, rsvr);

        if (times++ < SMTI_RETRY_TIMES)
        {
            goto AGAIN;
        }

        log_error(rsvr->log, "Recv queue was full! Perhaps lock conflicts too much!"
                "recv:%llu delay:%llu drop:%llu error:%llu",
                rsvr->recv_total, rsvr->delay_total[read->rqidx],
                rsvr->drop_total, rsvr->err_total);

        /* 创建NULL空间 */
        if (NULL == sck->null)
        {
            sck->null = slab_alloc(rsvr->pool, ctx->conf.recvq.size);
            if (NULL == sck->null)
            {
                log_error(rsvr->log, "Alloc memory from slab failed!");
                return SMTI_ERR;
            }
        }

        /* 指向NULL空间 */
        read->rqidx = 0;
        read->dataid = 0;
        read->addr = sck->null;
    }

    /* 3. 设置标识量 */
    read->off = 0;
    smti_set_read_phase(read, SMTI_PHASE_READ_HEAD);
    
    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_read_release
 **功    能: 释放数据接收
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static void smti_rsvr_read_release(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;

    /* 1. 释放内存 */
    if (read->addr != sck->null)
    {
        queue_dealloc(ctx->recvq[read->rqidx], read->addr);
    }

    /* 2. 重置标识量 */
    smti_reset_read_snap(read);
}

/******************************************************************************
 **函数名称: smti_rsvr_recv_header
 **功    能: 接收数据头
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_recv_header(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    int n, left;
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;


    /* 1. 接收数据 */
    while (1)
    {
        /* 1.1 计算剩余数 */
        left = sizeof(smti_header_t) - read->off;

        /* 1.2 接收数据头 */
        n = read(sck->fd, read->addr + read->off,  left);
        if (n == left)
        {
            read->off += n;
            break;
        }
        else if (n > 0)
        {
            read->off += n;
            continue;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return SMTI_AGAIN;
        }
        else if (0 == n)
        {
            log_error(rsvr->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                    errno, strerror(errno), sck->fd, n, left);
            return SMTI_SCK_CLOSED;
        }
        
        if (EINTR == errno)
        {
            continue; 
        }

        ++rsvr->err_total; /* 错误计数 */

        log_error(rsvr->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SMTI_ERR;
    }

    /* 3. 校验数据头 */
    if (smti_rsvr_check_header(ctx, recv, sck))
    {
        ++rsvr->err_total; /* 错误计数 */

        log_error(rsvr->log, "Check header failed! type:%d len:%d flag:%d mark:[%u/%u]",
                head->type, head->length, head->flag, head->mark, SMTI_MSG_MARK_KEY);
        return SMTI_ERR;
    }

    read->total = sizeof(smti_header_t) + head->length;

    log_debug(rsvr->log, "Recv header success! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->length, head->flag, head->mark, SMTI_MSG_MARK_KEY);

    return SMTI_DONE;
}

/******************************************************************************
 **函数名称: smti_rsvr_check_header
 **功    能: 校验数据头
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:合法 !0:不合法
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_check_header(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;


    /* 1. 检查校验值 */
    if (SMTI_MSG_MARK_KEY != head->mark)
    {
        log_error(rsvr->log, "Mark [%u/%u] isn't right! type:%d len:%d flag:%d",
                head->mark, SMTI_MSG_MARK_KEY, head->type, head->length, head->flag);
        return SMTI_ERR;
    }

    /* 2. 检查类型 */
    if (!smti_is_type_valid(head->type))
    {
        log_error(rsvr->log, "Data type is invalid! type:%d len:%d", head->type, head->length);
        return SMTI_ERR;
    }
 
    /* 3. 检查长度: 因所有队列长度一致 因此使用[0]判断 */
    if (!smti_is_len_valid(ctx->recvq[0], head->length))
    {
        log_error(rsvr->log, "Length is too long! type:%d len:%d", head->type, head->length);
        return SMTI_ERR;
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_recv_body
 **功    能: 接收数据体
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **     sck: 被操作的套接字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_recv_body(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    int n, left;
    smti_reg_t *reg;
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;


    while (1)
    {
        /* 1. 接收报体 */
        left = head->length + sizeof(smti_header_t) - read->off;

        n = read(sck->fd, read->addr + read->off, left);
        if (n == left)
        {
            read->off += n;
            break; 
        }
        else if (n > 0)
        {
            read->off += n;
            continue;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return SMTI_AGAIN;
        }
        else if (0 == n)
        {
            log_error(rsvr->log, "Client disconnected. errmsg:[%d] %s! "
                    "fd:%d type:%d flag:%d bodylen:%d total:%d left:%d off:%d",
                    errno, strerror(errno),
                    sck->fd, head->type, head->flag, head->length, read->total, left, read->off);
            return SMTI_SCK_CLOSED;
        }

        if (EINTR == errno)
        {
            continue;
        }

        ++rsvr->err_total; /* 错误计数 */

        log_error(rsvr->log, "errmsg:[%d] %s! type:%d len:%d n:%d fd:%d total:%d off:%d addr:%p",
                errno, strerror(errno), head->type,
                head->length, n, sck->fd, read->total, read->off, read->addr);

        return SMTI_ERR;
    }

    /* 2. 设置标志变量 */
    smti_set_read_phase(read, SMTI_PHASE_READ_POST);

    log_trace(rsvr->log, "Recv success! type:%d len:%d", head->type, head->length);
    
    return SMTI_DONE;
}

/******************************************************************************
 ** Name : smti_rsvr_sys_data_proc
 ** Desc : Handle system data
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.14 #
 ******************************************************************************/
static int smti_rsvr_sys_data_proc(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;

    switch (head->type)
    {
        case SMTI_KPALIVE_REQ:
        {
            return smti_rsvr_keepalive_req_hdl(ctx, recv, sck);
        }
        case SMTI_LINK_INFO_REPORT:
        {
            return smti_rsvr_link_info_report_hdl(ctx, sck);
        }
        default:
        {
            log_error(rsvr->log, "Give up handle this type [%d]!", head->type);
            return SMTI_HDL_DISCARD;
        }
    }
    
    return SMTI_HDL_DISCARD;
}

/******************************************************************************
 ** Name : smti_rsvr_send_work_cmd
 ** Desc : Send WORK REQ to work-thread
 ** Input: 
 **     ctx: Global context
 **     recv: Rcv-SVR
 ** Output: 
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.11 #
 ******************************************************************************/
static int smti_rsvr_send_work_cmd(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    int ret = 0, times = 0, work_tidx = 0;
    smti_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    smti_cmd_work_t *work_cmd = (smti_cmd_work_t *)&cmd.args;
    smti_conf_t *conf = &ctx->conf;

    cmd.type = SMTI_CMD_WORK;
    work_cmd->ori_recv_tidx = rsvr->tidx;
    work_cmd->num = ++rsvr->delay_total[sck->read.rqidx]; /* +1 */
    work_cmd->rqidx = sck->read.rqidx;

    /* 1. 随机选择Work线程 */
    /* work_tidx = smti_rand_work(ctx); */
    work_tidx = sck->read.rqidx / SMTI_WORKER_HDL_QNUM;

    smti_wsvr_usck_path(conf, path, work_tidx);

    /* 2. 发送处理命令 */
    ret = usck_udp_send(rsvr->cmd_sck_id, path, &cmd, sizeof(smti_cmd_t));
    if (ret < 0)
    {
        log_debug(rsvr->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                errno, strerror(errno), path);
        return SMTI_ERR;
    }

    rsvr->delay_total[sck->read.rqidx] = 0;

    return SMTI_OK;
}

/******************************************************************************
 ** Name : smti_rsvr_resend_work_cmd
 ** Desc : Resend work command
 ** Input: 
 **     recv: RcvSvr
 ** Output: NONE
 ** Return: 
 **     0:Succ
 **     !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smti_rsvr_resend_work_cmd(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    int ret = 0, work_tidx = 0, times = 0, idx = 0;
    smti_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    smti_cmd_work_t *work_cmd = (smti_cmd_work_t *)&cmd.args;
    smti_conf_t *conf = &ctx->conf;


    /* 依次检测各Recv队列的滞留数据 */
    for (idx=0; idx<ctx->conf.rqnum; ++idx)
    {
        if (rsvr->delay_total[idx] > 0)
        {
            cmd.type = SMTI_CMD_WORK;
            work_cmd->ori_recv_tidx = rsvr->tidx;
            work_cmd->rqidx = idx;
            work_cmd->num = rsvr->delay_total[idx];

            /* 1. 随机选择Work线程 */
            /* work_tidx = smti_rand_work(ctx); */
            work_tidx = idx / SMTI_WORKER_HDL_QNUM;

            smti_wsvr_usck_path(conf, path, work_tidx);

            /* 2. 发送处理命令 */
            ret = usck_udp_send(rsvr->cmd_sck_id, path, &cmd, sizeof(smti_cmd_t));
            if (ret < 0)
            {
                log_debug(rsvr->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                        errno, strerror(errno), path);
                continue;
            }

            rsvr->delay_total[idx] = 0;
        }
    }

    return 0;
}

/******************************************************************************
 ** Name : smti_rsvr_send_work_all_cmd
 ** Desc : Resend work all command
 ** Input: 
 **     recv: RcvSvr
 ** Output: NONE
 ** Return: void
 ** Proc : 
 **     给所有Work线程都发送处理命令
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static void smti_rsvr_send_work_all_cmd(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    int work_idx = 0, ret = 0, times = 0, idx = 0;
    smti_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    smti_conf_t *conf = &ctx->conf;
    smti_cmd_work_t *work_cmd = (smti_cmd_work_t *)&cmd.args;

     /* 1. 设置命令参数 */
    cmd.type = SMTI_CMD_WORK;
    work_cmd->ori_recv_tidx = rsvr->tidx;
    work_cmd->num = -1; /* 取出所有数据 */

     /* 2. 依次遍历所有Recv队列 让Work线程处理其中的数据 */
    for (idx=0; idx<conf->rqnum; ++idx)
    {
        work_cmd->rqidx = idx;

    AGAIN:
        /* 2.1 随机选择Work线程 */
        work_idx = rand() % conf->wrk_thd_num;

        smti_wsvr_usck_path(conf, path, work_idx);

        /* 2.2 发送处理命令 */
        ret = usck_udp_send(rsvr->cmd_sck_id, path, &cmd, sizeof(smti_cmd_t));
        if (0 != ret)
        {
            if (++times < 3)
            {
                goto AGAIN;
            }

            log_debug(rsvr->log, "Send command failed! errmsg:[%d] %s! path:%s",
                    errno, strerror(errno), path);
            continue;
        }

        rsvr->delay_total[idx] = 0;
    }

    return;
}

/******************************************************************************
 ** Name : smti_rsvr_exp_data_proc
 ** Desc : Forward expand data
 ** Input: 
 **     ctx: Global context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Send "Work REQ" command to worker thread
 **     2. Send fail commands again
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smti_rsvr_exp_data_proc(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    int ret;
    smti_read_snap_t *read = &sck->read;

    ++rsvr->recv_total; /* 总数 */

    /* 1. 是否在NULL空间: 直接丢弃 */
    if (read->addr == sck->null)
    {
        ++rsvr->drop_total;  /* 丢弃计数 */

        log_error(rsvr->log, "Lost data! tidx:[%d] fd:[%d] recv:%llu drop:%llu error:%llu",
                rsvr->tidx, sck->fd, rsvr->recv_total,
                rsvr->drop_total, rsvr->err_total);
        return SMTI_OK;
    }

    /* 2. 放入队列中 */
    ret = orm_queue_push(ctx->recvq[read->rqidx], read->dataid);
    if (ret < 0)
    {
        orm_queue_data_free(ctx->recvq[read->rqidx], read->dataid);

        ++rsvr->drop_total;  /* 丢弃计数 */

        log_error(rsvr->log, "Push failed! tidx:[%d] dataid:[%d] recv:%llu drop:%llu error:%llu",
                rsvr->tidx, read->dataid, rsvr->recv_total,
                rsvr->drop_total, rsvr->err_total);
        return SMTI_OK;  /* Note: Don't return error */
    }

    /* 1. Notify work-thread */
    ret = smti_rsvr_send_work_cmd(ctx, recv, sck);
    if (SMTI_OK != ret)
    {
        /* log_error(rsvr->log, "errmsg:[%d] %s!", errno, strerror(errno)); */
        return SMTI_OK;  /* Note: Don't return error, resend at next time */
    }

    return SMTI_OK;
}

/******************************************************************************
 ** Name : smti_rsvr_proc_data
 ** Desc : Process data from client.
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Handle system data
 **     2. Forward expand data
 ** Note : 
 ** Author: # Qifeng.zou # 2014.05.13 #
 ******************************************************************************/
static int smti_rsvr_proc_data(smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = read->addr;

    /* 1. Handle system data */
    if (SMTI_SYS_DATA == head->flag)
    {
        return smti_rsvr_sys_data_proc(ctx, recv, sck);
    }

    /* 2. Forward expand data */
    return smti_rsvr_exp_data_proc(ctx, recv, sck);
}

/******************************************************************************
 **函数名称: smti_rsvr_event_core_hdl
 **功    能: 事件核心处理
 **输入参数: 
 **     ctx: 全局对象
 **     rsvr: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.01 #
 ******************************************************************************/
static int smti_rsvr_event_core_hdl(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    /* 1. 接收命令数据 */
    if (!FD_ISSET(rsvr->cmd_sck_id, &rsvr->rdset))
    {
        smti_rsvr_recv_cmd(ctx, rsvr);
    }

    /* 2. 遍历接收数据 */
    smti_rsvr_trav_recv(ctx, rsvr);

    /* 3. 遍历发送数据 */
    smti_rsvr_trav_send(ctx, rsvr);

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_event_timeout_hdl
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
static int smti_rsvr_event_timeout_hdl(smti_cntx_t *ctx, smti_rsvr_t *rsvr)
{
    smti_sck_t *curr, *next;

    /* 1. 检测超时连接 */
    curr = rsvr->sck;
    rsvr->ctm = time(NULL);
    while (NULL != curr)
    {
        if (rsvr->ctm - curr->rtm >= 2*SMTI_SCK_KPALIVE_SEC)
        {
            log_trace(rsvr->log, "Didn't active for along time! fd:%d ip:%s",
                curr->fd, curr->ipaddr);

            /* 释放数据 */
            smti_rsvr_read_release(ctx, recv, curr);

            /* 删除连接 */
            next = curr->next;
            smti_rsvr_del_sck_hdl(rsvr, curr);

            curr = next;
            continue;
        }

        curr = curr->next;
    }

    /* 2. 重复发送处理命令 */
    smti_rsvr_resend_work_cmd(ctx, rsvr);
    return SMTI_OK;
}

/******************************************************************************
 ** Name : smti_rsvr_keepalive_req_hdl
 ** Desc : Keepalive request hdl
 ** Input: 
 **     ctx: Global context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     > Done : SMTI_HDL_DONE
 **     > Fail : SMTI_FAILED
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.01 #
 ******************************************************************************/
static int smti_rsvr_keepalive_req_hdl(
        smti_cntx_t *ctx, smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    void *addr;
    smti_header_t *head;

    addr = slab_alloc(rsvr->pool, sizeof(smti_header_t));
    if (NULL == addr)
    {
        log_error(rsvr->log, "Alloc memory from slab failed!");
        return SMTI_ERR;
    }

    head = (smti_header_t *)addr;

    head->type = SMTI_KPALIVE_REP;
    head->length = 0;
    head->flag = SMTI_SYS_DATA;
    head->mark = SMTI_MSG_MARK_KEY;
    
    smti_rsvr_add_msg(rsvr, sck, addr);

    log_debug(rsvr->log, "Add respond of keepalive request!");

    return SMTI_HDL_DONE;
}

/******************************************************************************
 ** Name : smti_rsvr_link_info_report_hdl
 ** Desc : link info report hdl
 ** Input: 
 **     ctx: Global context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     > Done : SMTI_HDL_DONE
 **     > Fail : SMTI_FAILED
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.08.14 #
 ******************************************************************************/
static int smti_rsvr_link_info_report_hdl(smti_cntx_t *ctx, smti_sck_t *sck)
{
    smti_link_info_report_t *info;
    smti_read_snap_t *read = &sck->read;

    info = (smti_link_info_report_t *)(read->addr + sizeof(smti_header_t));

    sck->is_primary = info->is_primary;

    return SMTI_HDL_DONE;
}

/******************************************************************************
 ** Name : smti_rsvr_add_sck_hdl
 ** Desc : ADD SCK request hdl
 ** Input: 
 **     recv: Recv对象
 **     req: Parameter of ADD SCK
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smti_rsvr_add_sck_hdl(smti_rsvr_t *rsvr, smti_cmd_add_sck_t *req)
{
    smti_sck_t *add;

    /* 1. 为新结点分配空间 */
    add = slab_alloc(rsvr->pool, sizeof(smti_sck_t));
    if (NULL == add)
    {
        log_error(rsvr->log, "Alloc memory failed!");
        return SMTI_ERR;
    }

    add->fd = req->sckid;
    add->ctm = time(NULL);
    add->rtm = add->ctm;
    add->wtm = add->ctm;
    snprintf(add->ipaddr, sizeof(add->ipaddr), "%s", req->ipaddr);

    add->read.dataid = NULLID;
    add->read.addr = NULL;

    /* 2. 将结点加入到套接字链表 */
    if (NULL == rsvr->sck)
    {
        rsvr->sck = add;
        add->next = NULL;
    }
    else
    {
        add->next = rsvr->sck;
        rsvr->sck = add;
    }

    ++rsvr->connections; /* 统计TCP连接数 */

    log_trace(rsvr->log, "Tidx [%d] insert sckid [%d] success! ip:%s",
        rsvr->tidx, req->sckid, req->ipaddr);
    return SMTI_OK;
}

/******************************************************************************
 ** Name : smti_rsvr_del_sck_hdl
 ** Desc : 从套接字链表删除指定套接字.
 ** Input: 
 **     recv: Recv对象
 **     sck: 需要被被删除的套接字对象
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smti_rsvr_del_sck_hdl(smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    smti_sck_t *curr, *prev;

    curr = rsvr->sck;
    prev = curr;
    while (NULL != curr)
    {
        if (sck == curr)
        {
            if (prev == curr)
            {
                rsvr->sck = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }

            Close(curr->fd);
            smti_rsvr_clear_msg(rsvr, curr);
            curr->recv_total = 0;

            if (NULL != curr->null)
            {
                slab_dealloc(rsvr->pool, curr->null);
            }
            slab_dealloc(rsvr->pool, curr);

            --rsvr->connections; /* 统计TCP连接数 */

            return SMTI_OK;
        }

        prev = curr;
        curr= curr->next;
    }

    return SMTI_OK; /* Didn't found */
}

/******************************************************************************
 ** Name : smti_rsvr_del_all_sck_hdl
 ** Desc : 删除接收线程所有的套接字
 ** Input: 
 **     recv: Recv对象
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static void smti_rsvr_del_all_sck_hdl(smti_rsvr_t *rsvr)
{
    smti_sck_t *curr, *next;

    curr = rsvr->sck; 
    while (NULL != curr)
    {
        next = curr->next;

        Close(curr->fd);
        smti_rsvr_clear_msg(rsvr, curr);

        if (NULL != curr->null)
        {
            slab_dealloc(rsvr->pool, curr->null);
        }
        slab_dealloc(rsvr->pool, curr);

        curr = next;
    }

    rsvr->connections = 0; /* 统计TCP连接数 */
    rsvr->sck = NULL;
    return;
}

/******************************************************************************
 **函数名称: smti_rsvr_add_msg
 **功    能: 添加发送消息
 **输入参数: 
 **    recv: Recv对象
 **    buf: 将要发送的数据
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **    将要发送的数据放在链表的末尾
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smti_rsvr_add_msg(smti_rsvr_t *rsvr, smti_sck_t *sck, void *addr)
{
    list_t *add, *item, *tail = NULL;

    /* 1.创建新结点 */
    add = slab_alloc(rsvr->pool, sizeof(list_t));
    if (NULL == add)
    {
        log_debug(rsvr->log, "Alloc memory failed!");
        return SMTI_ERR;
    }

    add->data = addr;
    add->next = NULL;

    /* 2.插入链尾 */
    item = sck->message_list;
    if (NULL == item)
    {
        sck->message_list = add;
        return SMTI_OK;
    }

    /* 3.查找链尾 */
    do
    {
        tail = item;
        item = item->next;
    }while (NULL != item);

    tail->next = add;

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_rsvr_get_msg
 **功    能: 获取发送消息
 **输入参数: 
 **    recv: Recv对象
 **    buf: 将要发送的数据
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static void *smti_rsvr_get_msg(smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    void *addr;
    list_t *curr = sck->message_list;

    if (NULL == curr)
    {
        return NULL;
    }
    
    sck->message_list = curr->next;
    addr = curr->data;

    slab_dealloc(rsvr->pool, curr);

    return addr;
}

/******************************************************************************
 **函数名称: smti_rsvr_clear_msg
 **功    能: 清空发送消息
 **输入参数: 
 **    recv: Recv对象
 **    sck: 将要清空的套接字对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smti_rsvr_clear_msg(smti_rsvr_t *rsvr, smti_sck_t *sck)
{
    list_t *curr, *next;

    curr = sck->message_list; 
    while (NULL != curr)
    {
        next = curr->next;

        slab_dealloc(rsvr->pool, curr->data);
        slab_dealloc(rsvr->pool, curr);

        curr = next;
    }

    sck->message_list = NULL;
    return SMTI_OK;
}

#include <memory.h>
#include <assert.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "smtp.h"
#include "xml_tree.h"
#include "smtp_cmd.h"
#include "smtp_comm.h"
#include "smtp_recv.h"
#include "thread_pool.h"

/* 静态函数 */
static smtp_rsvr_t *smtp_rsvr_get_curr(smtp_cntx_t *ctx);
static int smtp_rsvr_init(smtp_cntx_t *ctx, smtp_rsvr_t *recv);
static int smtp_rsvr_core_handler(smtp_cntx_t *ctx, smtp_rsvr_t *recv);
static void smtp_rsvr_timeout_handler(smtp_cntx_t *ctx, smtp_rsvr_t *recv);

static int smtp_rsvr_trav_recv(smtp_cntx_t *ctx, smtp_rsvr_t *recv);
static int smtp_rsvr_trav_send(smtp_cntx_t *ctx, smtp_rsvr_t *recv);

static int smtp_rsvr_read_proc(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_read_init(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_read_header(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_read_body(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);

static int smtp_rsvr_check_header(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static void smtp_rsvr_read_release(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);

static int smtp_rsvr_proc_data(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_sys_data_proc(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_exp_data_proc(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);

static int smtp_rsvr_link_info_report_hdl(smtp_cntx_t *ctx, smtp_sck_t *sck);
static int smtp_rsvr_keepalive_req_hdl(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_send_work_cmd(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck);
static void smtp_rsvr_send_work_all_cmd(smtp_cntx_t *ctx, smtp_rsvr_t *recv);

static int smtp_rsvr_cmd_add_sck_hdl(smtp_rsvr_t *recv, smtp_cmd_add_sck_t *req);
static int smtp_rsvr_del_sck_hdl(smtp_rsvr_t *recv, smtp_sck_t *sck);
static void smtp_rsvr_del_all_sck_hdl(smtp_rsvr_t *recv);

static int smtp_rsvr_add_msg(smtp_rsvr_t *recv, smtp_sck_t *sck, void *addr);
static void *smtp_rsvr_get_msg(smtp_rsvr_t *recv, smtp_sck_t *sck);
static int smtp_rsvr_clear_msg(smtp_rsvr_t *recv, smtp_sck_t *sck);

#if defined(__SMTP_DEBUG_PRINT__)
static void smtp_print_conf(const smtp_conf_t *conf);
static void smtp_print_recvtp(const thread_pool_t *tpool);
static void smtp_print_worktp(const thread_pool_t *tpool);
static void smtp_print_reg(const smtp_reg_t *reg);
static void smtp_print_cntx(const smtp_cntx_t *ctx);
#endif /*__SMTP_DEBUG_PRINT__*/

/* Random select a recv thread */
#define smtp_rand_recv(ctx) ((ctx)->listen.total++ % (ctx->recvtp->num))
/* Random select a work thread */
#define smtp_rand_work(ctx) (rand() % (ctx->worktp->num))

/******************************************************************************
 ** Name : smtp_rsvr_set_rdset
 ** Desc : Set read fdset of recv
 ** Input: 
 **     recv: Recv context
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
#define smtp_rsvr_set_rdset(recv) \
{ \
    smtp_sck_t *curr, *next; \
    \
    FD_ZERO(&recv->rdset); \
    \
    FD_SET(recv->cmd_sck_id, &recv->rdset); \
    recv->max = recv->cmd_sck_id; \
    \
    curr = recv->sck; \
    while (NULL != curr) \
    { \
        if (recv->tm - curr->rtm >= 180) \
        { \
            log_trace("Didn't recv data for along time! fd:[%d] ip:[%s]", \
                    curr->fd, curr->ipaddr); \
            \
            next = curr->next; \
            smtp_rsvr_del_sck_hdl(recv, curr); \
            \
            curr = next; \
            continue; \
        } \
        \
        FD_SET(curr->fd, &recv->rdset); \
        if (curr->fd > recv->max) \
        { \
            recv->max = curr->fd; \
        } \
        \
        curr = curr->next; \
    } \
}

/******************************************************************************
 ** Name : smtp_rsvr_set_wrset
 ** Desc : Set write fdset of recv
 ** Input: 
 **     recv: Recv context
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.08.18 #
 ******************************************************************************/
#define smtp_rsvr_set_wrset(recv) \
{ \
    smtp_sck_t *curr; \
    \
    FD_ZERO(&recv->wrset); \
    \
    curr = recv->sck; \
    while (NULL != curr) \
    { \
        if (NULL == curr->message_list) \
        { \
            curr = curr->next; \
            continue; \
        } \
        \
        FD_SET(curr->fd, &recv->wrset); \
        \
        curr = curr->next; \
    } \
}

/******************************************************************************
 ** Name : smtp_rsvr_routine
 ** Desc : Start recv data
 ** Input: 
 **     args: Global context
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Init recv context
 **     2. Wait event
 **     3. Call recv handler
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.24 #
 ******************************************************************************/
void *smtp_rsvr_routine(void *args)
{
    int ret = 0;
    struct timeval timeout;
    smtp_rsvr_t *recv = NULL;
    smtp_cntx_t *ctx = (smtp_cntx_t *)args;

    /* 1. Get curr recv context */
    recv = smtp_rsvr_get_curr(ctx);
    if (NULL == recv)
    {
        log_error(recv->log, "Get recv failed!");
        pthread_exit(NULL);
        return (void *)-1;
    }

    for (;;)
    {
        /* 2. Wait event */
        smtp_rsvr_set_rdset(recv);
        smtp_rsvr_set_wrset(recv);

        timeout.tv_sec = SMTP_TMOUT_SEC;
        timeout.tv_usec = SMTP_TMOUT_USEC;
        ret = select(recv->max+1, &recv->rdset, &recv->wrset, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(recv->log, "errmsg:[%d] %s", errno, strerror(errno));
            return (void *)SMTP_ERR;
        }
        else if (0 == ret)
        {
            smtp_rsvr_timeout_handler(ctx, recv);
        #if defined(__SMTP_DEBUG_PRINT__)
            smtp_print_cntx(ctx);
        #endif /*__SMTP_DEBUG_PRINT__*/
            continue;
        }

        /* 3. Call core handler */
        smtp_rsvr_core_handler(ctx, recv);

    #if defined(__SMTP_DEBUG_PRINT__)
        smtp_scks_print(&recv->scks);
    #endif /*__SMTP_DEBUG_PRINT__*/
    }

    pthread_exit(NULL);
    return (void *)-1;
}

/******************************************************************************
 ** Name : smtp_creat_recvtp
 ** Desc : Create recv-thread-pool
 ** Input: 
 **     conf: Configuration
 ** Output: 
 **     recvtp: Recv thread-pool
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Create recv-thread-pool
 **     2. Alloc memory for recv-context
 **     3. Init recv-context
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.11 #
 ******************************************************************************/
int smtp_creat_recvtp(smtp_cntx_t *ctx)
{
    int ret = 0, idx = 0;
    smtp_rsvr_t *recv = NULL;
    smtp_conf_t *conf = &ctx->conf;

    /* 1. Create thread-pool */
    ret = thread_pool_init(&ctx->recvtp, conf->recv_thd_num);
    if (SMTP_OK != ret)
    {
        log_error(recv->log, "Initialize thread pool failed!");
        return SMTP_ERR;
    }

    /* 2. Alloc memory for recv context */
    ctx->recvtp->data = (void *)calloc(conf->recv_thd_num, sizeof(smtp_rsvr_t));
    if (NULL == ctx->recvtp->data)
    {
        log_error(recv->log, "errmsg:[%d] %s!", errno, strerror(errno));
        thread_pool_destroy(ctx->recvtp);
        ctx->recvtp = NULL;
        return SMTP_ERR;
    }

    /* 3. Init recv context */
    recv = (smtp_rsvr_t *)ctx->recvtp->data;
    for (idx=0; idx<conf->recv_thd_num; ++idx, ++recv)
    {
        recv->tidx = idx;

        ret = smtp_rsvr_init(ctx, recv);
        if (0 != ret)
        {
            thread_pool_destroy(ctx->recvtp);
            ctx->recvtp = NULL;

            log_error(recv->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return SMTP_ERR;
        }
    }

    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_recvtp_destroy
 ** Desc : Destroy recv-thread-pool
 ** Input: 
 ** Output: 
 **     _ctx: Global context
 **     args: recvtp->data地址
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.15 #
 ******************************************************************************/
void smtp_recvtp_destroy(void *_ctx, void *args)
{
    int idx = 0, num = 0;
    smtp_cntx_t *ctx = (smtp_cntx_t *)_ctx;
    smtp_rsvr_t *recv = (smtp_rsvr_t *)args;

    num = ctx->conf.recv_thd_num;

    for (idx=0; idx<num; ++idx, ++recv)
    {
        /* 1. 关闭命令套接字 */
        Close(recv->cmd_sck_id);

        /* 2. 关闭通信套接字 */
        smtp_rsvr_del_all_sck_hdl(recv);

        eslab_destroy(&recv->pool);
    }

    Free(args);

    return;
}

/******************************************************************************
 ** Name : smtp_rsvr_get_curr
 ** Desc : Curr recv svr context
 ** Input: 
 **     ctx: Global context
 ** Output: NONE
 ** Return: Address of recv context
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
static smtp_rsvr_t *smtp_rsvr_get_curr(smtp_cntx_t *ctx)
{
    int tidx = 0;

    /* 1. Get thread idx */
    tidx = thread_pool_get_tidx(ctx->recvtp);
    if (tidx < 0)
    {
        log_error(recv->log, "Get index of current thread failed!");
        return NULL;
    }

    return (smtp_rsvr_t *)(ctx->recvtp->data + tidx * sizeof(smtp_rsvr_t));
}

/******************************************************************************
 ** Name : smtp_rsvr_init
 ** Desc : Init Recv context
 ** Input: 
 **     ctx: Global context
 ** Output: NONE
 ** Return: Address of recv context
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
static int smtp_rsvr_init(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int ret = 0;
    char path[FILE_PATH_MAX_LEN] = {0};
    smtp_conf_t *conf = &ctx->conf;

    recv->tm = time(NULL);

    /* 1. 创建各队列滞留条数数组 */
    recv->delay_total = calloc(ctx->conf.recvq_num, sizeof(unsigned long long));
    if (NULL == recv->delay_total)
    {
        log_error(recv->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTP_ERR;
    }

    /* 2. 创建CMD套接字 */
    smtp_rsvr_usck_path(conf, path, recv->tidx);
    
    recv->cmd_sck_id = usck_udp_creat(path);
    if (recv->cmd_sck_id < 0)
    {
        log_error(recv->log, "Create unix-udp socket failed!");
        return SMTP_ERR;
    }

    /* 3. 创建SLAB内存池 */
    ret = eslab_init(&recv->pool, SMTP_MEM_POOL_SIZE);
    if (0 != ret)
    {
        log_error(recv->log, "Initialize slab mem-pool failed!");
        return SMTP_ERR;
    }

    recv->sck = NULL;
    return 0;
}

/******************************************************************************
 ** Name : smtp_rsvr_recv_cmd
 ** Desc : Recv command
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
static int smtp_rsvr_recv_cmd(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int ret = 0;
    smtp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    if (!FD_ISSET(recv->cmd_sck_id, &recv->rdset))
    {
        return SMTP_OK;
    }

    ret = usck_udp_recv(recv->cmd_sck_id, (void *)&cmd, sizeof(cmd));
    if (ret < 0)
    {
        log_error(recv->log, "Read data failed!");
        return SMTP_ERR_RECV_CMD;
    }

    switch (cmd.type)
    {
        case SMTP_CMD_ADD_SCK:
        {
            return smtp_rsvr_cmd_add_sck_hdl(recv, (smtp_cmd_add_sck_t *)&cmd.args);
        }
        default:
        {
            log_error(recv->log, "Received unknown command! type:[%d]", cmd.type);
            return SMTP_ERR_UNKNOWN_CMD;
        }
    }

    return SMTP_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 ** Name : smtp_rsvr_trav_recv
 ** Desc : Recv data from client.
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
static int smtp_rsvr_trav_recv(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int ret = 0;
    smtp_sck_t *curr, *next;

    recv->tm = time(NULL);
    curr = recv->sck;
    while (NULL != curr)
    {
        if (FD_ISSET(curr->fd, &recv->rdset))
        {
            curr->rtm = recv->tm;

            /* Recv data */
            ret = smtp_rsvr_read_proc(ctx, recv, curr);
            if (SMTP_OK != ret)
            {
                log_error(recv->log, "Read proc failed! fd:[%d] ip:[%s]", curr->fd, curr->ipaddr);
                next = curr->next;
                smtp_rsvr_del_sck_hdl(recv, curr);
                curr = next;
                continue;
            }
        }

        curr = curr->next;
    }

    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_trav_send
 ** Desc : Send data to client.
 ** Input: 
 **     ctx: 上下文
 **     recv: Recv对象
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
static int smtp_rsvr_trav_send(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int n;
    smtp_header_t *head;
    smtp_sck_t *curr;
    smtp_send_snap_t *send;

    recv->tm = time(NULL);
    curr = recv->sck;

    while (NULL != curr)
    {
        if (FD_ISSET(curr->fd, &recv->wrset))
        {
            curr->wtm = recv->tm;
            send = &curr->send;

            for (;;)
            {
                /* 1. 获取需要发送的数据 */
                if (NULL == send->addr)
                {
                    send->addr = smtp_rsvr_get_msg(recv, curr);
                    if (NULL == send->addr)
                    {
                        break;
                    }

                    head = (smtp_header_t *)send->addr;

                    send->loc = SMTP_DATA_LOC_SLAB;
                    send->off = 0;
                    send->total = head->body_len + sizeof(smtp_header_t);
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

                    log_error(recv->log, "errmsg:[%d] %s!", errno, strerror(errno));

                    if (SMTP_DATA_LOC_SLAB == send->loc)
                    {
                        eslab_free(&recv->pool, send->addr);
                    }

                    smtp_reset_send_snap(send);

                    /* 关闭套接字　并清空发送队列 */
                    smtp_rsvr_del_sck_hdl(recv, curr);
                    return SMTP_ERR;
                }

                /* 3. 释放空间 */
                if (SMTP_DATA_LOC_SLAB == send->loc)
                {
                    eslab_free(&recv->pool, send->addr);
                }

                smtp_reset_send_snap(send);
            }
        }

        curr = curr->next;
    }

    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_read_proc
 ** Desc : Recv data
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Read init 
 **     2. Read head
 **     3. Read body
 **     4. Proc data
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.12 #
 ******************************************************************************/
static int smtp_rsvr_read_proc(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    int ret = 0;
    smtp_header_t *head;
    smtp_read_snap_t *read = &sck->read;

    switch (read->phase)
    {
        case SMTP_PHASE_READ_INIT:
        {
            ret = smtp_rsvr_read_init(ctx, recv, sck);
            if (SMTP_OK != ret)
            {
                log_error(recv->log, "Init read failed!");
                return SMTP_ERR;
            }

            /* NOTE: Don't break - Continue handle */
        }
        case SMTP_PHASE_READ_HEAD:
        {
            ret = smtp_rsvr_read_header(ctx, recv, sck);
            if (SMTP_DONE == ret)
            {
                head = (smtp_header_t *)read->addr;
                if (head->body_len > 0)
                {
                    return SMTP_OK;
                }

                goto PHASE_READ_POST;
            }
            else if (SMTP_AGAIN == ret)  /* incomplete */
            {
                /* Note: Continue recv head at next loop */
                return SMTP_OK;
            }
            else if (SMTP_SCK_CLOSED == ret)
            {
                log_debug(recv->log, "Client disconnect!");
                break;
            }
            else
            {
                log_error(recv->log, "Recv head failed!");
                break; /* error */
            }
            /* NOTE: Don't break - Continue handle */
        }
        case SMTP_PHASE_READ_BODY:
        {
            ret = smtp_rsvr_read_body(ctx, recv, sck);
            if (SMTP_DONE == ret)
            {
                /* NULL;  Note: Continue handle */
            }
            else if (SMTP_AGAIN == ret)
            {
                /* Note: Continue recv body at next loop */
                return SMTP_OK;
            }
            else if (SMTP_HDL_DISCARD == ret)
            {
                smtp_rsvr_read_release(ctx, recv, sck);
                return SMTP_OK;
            }
            else if (SMTP_SCK_CLOSED == ret)
            {
                log_debug(recv->log, "Client disconnect!");
                break;
            }
            else
            {
                log_error(recv->log, "Recv body failed!");
                break; /* error */
            }
            /* NOTE: Don't break - Continue handle */
        }
        case SMTP_PHASE_READ_POST:
        {
        PHASE_READ_POST:
            ret = smtp_rsvr_proc_data(ctx, recv, sck);
            if (SMTP_OK == ret)
            {
                smtp_reset_read_snap(read);
                return SMTP_OK;
            }
            else if ((SMTP_HDL_DONE == ret)
                || (SMTP_HDL_DISCARD == ret))
            {
                smtp_rsvr_read_release(ctx, recv, sck);
                return SMTP_OK;
            }

            break;
        }
        default:
        {
            log_error(recv->log, "Unknown phase!");
            break;
        }
    }

    smtp_rsvr_read_release(ctx, recv, sck);

    return SMTP_ERR;
}

/******************************************************************************
 ** Name : smtp_rsvr_read_init
 ** Desc : Prepare recv read data
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 **     只有2种返回结果：Succ 或 队列空间不足
 ** Author: # Qifeng.zou # 2014.05.13 #
 ******************************************************************************/
static int smtp_rsvr_read_init(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    int times = 0;
    smtp_read_snap_t *read = &sck->read;

AGAIN:
    /* 1. 随机选择Recv队列 */
    read->rqidx = rand() % ctx->conf.recvq_num;

    /* 2. 从队列申请空间 */
    read->dataid = orm_queue_data_malloc(ctx->recvq[read->rqidx], &read->addr);
    if (NULLID == read->dataid)
    {
        smtp_rsvr_send_work_all_cmd(ctx, recv);

        if (times++ < 3)
        {
            goto AGAIN;
        }

        log_error(recv->log, "Recv queue was full! Perhaps lock conflicts too much!"
                "recv:[%llu] delay:[%llu] drop:%llu error:%llu",
                recv->recv_total, recv->delay_total[read->rqidx],
                recv->drop_total, recv->err_total);

        /* 创建NULL空间 */
        if (NULL == sck->null)
        {
            sck->null = eslab_alloc(&recv->pool, ctx->conf.recvq.size);
            if (NULL == sck->null)
            {
                log_error(recv->log, "errmsg:[%d] %s!", errno, strerror(errno));
                return SMTP_ERR;
            }
        }

        /* 指向NULL空间 */
        read->rqidx = 0;
        read->dataid = 0;
        read->addr = sck->null;
    }

    /* 3. 设置标识量 */
    read->offset = 0;
    smtp_set_read_phase(read, SMTP_PHASE_READ_HEAD);
    
    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_read_release
 ** Desc : It's wrong when recv data.
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.05.14 #
 ******************************************************************************/
static void smtp_rsvr_read_release(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;

    /* 1. 释放内存 */
    if ((read->dataid >= 0)
        && (read->addr != sck->null))
    {
        orm_queue_data_free(ctx->recvq[read->rqidx], read->dataid);
    }

    /* 2. 重置标识量 */
    smtp_reset_read_snap(read);
}

/******************************************************************************
 ** Name : smtp_rsvr_read_header
 ** Desc : Recv head
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     1. incomplete - SMTP_AGAIN
 **     2. done  - SMTP_DONE
 **     3. close - SMTP_SCK_CLOSED
 **     4. failed - SMTP_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but recv again at next time.
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smtp_rsvr_read_header(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    int n = 0, ret = 0, left = 0;
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = (smtp_header_t *)read->addr;


    /* 1. Compute left characters */
    left = sizeof(smtp_header_t) - read->offset;

    /* 2. Recv head */
    n = Readn(sck->fd, read->addr + read->offset,  left);
    if (n != left)
    {
        if (n > 0)
        {
            read->offset += n;
            return SMTP_AGAIN;
        }
        else if (0 == n)
        {
            if (0 != read->offset)
            {
                ++recv->err_total; /* 错误计数 */
            }

            log_error(recv->log, "Client disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                    errno, strerror(errno), sck->fd, n, left);
            return SMTP_SCK_CLOSED;
        }

        ++recv->err_total; /* 错误计数 */

        log_error(recv->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SMTP_ERR;
    }
    
    read->offset += n;

    /* 3. Check head */
    ret = smtp_rsvr_check_header(ctx, recv, sck);
    if (SMTP_OK != ret)
    {
        ++recv->err_total; /* 错误计数 */

        log_error(recv->log, "Check header failed! type:%d len:%d flag:%d mark:[%u/%u]",
                head->type, head->body_len, head->flag, head->mark, SMTP_MSG_MARK_KEY);
        return SMTP_ERR;
    }

    read->total = sizeof(smtp_header_t) + head->body_len;
    smtp_set_read_phase(read, SMTP_PHASE_READ_BODY);

    log_debug(recv->log, "Recv header success! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->body_len, head->flag, head->mark, SMTP_MSG_MARK_KEY);

    return SMTP_DONE;
}

/******************************************************************************
 ** Name : smtp_rsvr_check_header
 ** Desc : Check head
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.05.13 #
 ******************************************************************************/
static int smtp_rsvr_check_header(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = (smtp_header_t *)read->addr;


    /* 1. 检查校验值 */
    if (SMTP_MSG_MARK_KEY != head->mark)
    {
        log_error(recv->log, "Mark [%u/%u] isn't right! type:%d len:%d flag:%d",
                head->mark, SMTP_MSG_MARK_KEY, head->type, head->body_len, head->flag);
        return SMTP_ERR;
    }

    /* 2. 检查类型 */
    if (!smtp_is_type_valid(head->type))
    {
        log_error(recv->log, "Data type is invalid! type:%d len:%d", head->type, head->body_len);
        return SMTP_ERR;
    }
 
    /* 3. 检查长度: 因所有队列长度一致 因此使用[0]判断 */
    if (!smtp_is_len_valid(ctx->recvq[0], head->body_len))
    {
        log_error(recv->log, "Length is too long! type:%d len:%d", head->type, head->body_len);
        return SMTP_ERR;
    }

    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_read_body
 ** Desc : Recv body
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return:
 **     1. incomplete - SMTP_AGAIN
 **     2. done  - SMTP_DONE
 **     3. close - SMTP_SCK_CLOSED
 **     4. failed - SMTP_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but recv again at next time.
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smtp_rsvr_read_body(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    int n = 0, left = 0;
    smtp_reg_t *reg = NULL;
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = (smtp_header_t *)read->addr;


    /* 1. Recv body */
    left = head->body_len + sizeof(smtp_header_t) - read->offset;

    n = Readn(sck->fd, read->addr + read->offset, left);
    if (n != left)
    {
        if (n > 0)
        {
            read->offset += n;
            return SMTP_AGAIN;
        }
        else if (0 == n)
        {
            ++recv->err_total; /* 错误计数 */

            log_error(recv->log, "Client disconnected. errmsg:[%d] %s! "
                    "fd:[%d] type:[%d] flag:[%d] bodylen:[%d] total:[%d] left:[%d] offset:[%d]",
                    errno, strerror(errno),
                    sck->fd, head->type, head->flag, head->body_len, read->total, left, read->offset);
            return SMTP_SCK_CLOSED;
        }

        ++recv->err_total; /* 错误计数 */

        log_error(recv->log, "errmsg:[%d] %s! type:%d len:%d n:%d fd:%d total:%d offset:%d addr:%p",
                errno, strerror(errno), head->type,
                head->body_len, n, sck->fd, read->total, read->offset, read->addr);

        return SMTP_ERR;
    }

    /* 2. Set flag variables */
    read->offset += n;
    smtp_set_read_phase(read, SMTP_PHASE_READ_POST);

    log_trace("Recv success! type:%d len:%d", head->type, head->body_len);
    
    return SMTP_DONE;
}

/******************************************************************************
 ** Name : smtp_rsvr_sys_data_proc
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
static int smtp_rsvr_sys_data_proc(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = (smtp_header_t *)read->addr;

    switch (head->type)
    {
        case SMTP_KPALIVE_REQ:
        {
            return smtp_rsvr_keepalive_req_hdl(ctx, recv, sck);
        }
        case SMTP_LINK_INFO_REPORT:
        {
            return smtp_rsvr_link_info_report_hdl(ctx, sck);
        }
        default:
        {
            log_error(recv->log, "Give up handle this type [%d]!", head->type);
            return SMTP_HDL_DISCARD;
        }
    }
    
    return SMTP_HDL_DISCARD;
}

/******************************************************************************
 ** Name : smtp_rsvr_send_work_cmd
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
static int smtp_rsvr_send_work_cmd(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    int ret = 0, times = 0, work_tidx = 0;
    smtp_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    smtp_cmd_work_t *work_cmd = (smtp_cmd_work_t *)&cmd.args;
    smtp_conf_t *conf = &ctx->conf;

    cmd.type = SMTP_CMD_WORK;
    work_cmd->ori_recv_tidx = recv->tidx;
    work_cmd->num = ++recv->delay_total[sck->read.rqidx]; /* +1 */
    work_cmd->rqidx = sck->read.rqidx;

    /* 1. 随机选择Work线程 */
    /* work_tidx = smtp_rand_work(ctx); */
    work_tidx = sck->read.rqidx / SMTP_WORKER_HDL_QNUM;

    smtp_wsvr_usck_path(conf, path, work_tidx);

    /* 2. 发送处理命令 */
    ret = usck_udp_send(recv->cmd_sck_id, path, &cmd, sizeof(smtp_cmd_t));
    if (ret < 0)
    {
        log_debug(recv->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                errno, strerror(errno), path);
        return SMTP_ERR;
    }

    recv->delay_total[sck->read.rqidx] = 0;

    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_resend_work_cmd
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
static int smtp_rsvr_resend_work_cmd(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int ret = 0, work_tidx = 0, times = 0, idx = 0;
    smtp_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    smtp_cmd_work_t *work_cmd = (smtp_cmd_work_t *)&cmd.args;
    smtp_conf_t *conf = &ctx->conf;


    /* 依次检测各Recv队列的滞留数据 */
    for (idx=0; idx<ctx->conf.recvq_num; ++idx)
    {
        if (recv->delay_total[idx] > 0)
        {
            cmd.type = SMTP_CMD_WORK;
            work_cmd->ori_recv_tidx = recv->tidx;
            work_cmd->rqidx = idx;
            work_cmd->num = recv->delay_total[idx];

            /* 1. 随机选择Work线程 */
            /* work_tidx = smtp_rand_work(ctx); */
            work_tidx = idx / SMTP_WORKER_HDL_QNUM;

            smtp_wsvr_usck_path(conf, path, work_tidx);

            /* 2. 发送处理命令 */
            ret = usck_udp_send(recv->cmd_sck_id, path, &cmd, sizeof(smtp_cmd_t));
            if (ret < 0)
            {
                log_debug(recv->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                        errno, strerror(errno), path);
                continue;
            }

            recv->delay_total[idx] = 0;
        }
    }

    return 0;
}

/******************************************************************************
 ** Name : smtp_rsvr_send_work_all_cmd
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
static void smtp_rsvr_send_work_all_cmd(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int work_idx = 0, ret = 0, times = 0, idx = 0;
    smtp_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    smtp_conf_t *conf = &ctx->conf;
    smtp_cmd_work_t *work_cmd = (smtp_cmd_work_t *)&cmd.args;

     /* 1. 设置命令参数 */
    cmd.type = SMTP_CMD_WORK;
    work_cmd->ori_recv_tidx = recv->tidx;
    work_cmd->num = -1; /* 取出所有数据 */

     /* 2. 依次遍历所有Recv队列 让Work线程处理其中的数据 */
    for (idx=0; idx<conf->recvq_num; ++idx)
    {
        work_cmd->rqidx = idx;

    AGAIN:
        /* 2.1 随机选择Work线程 */
        work_idx = rand() % conf->wrk_thd_num;

        smtp_wsvr_usck_path(conf, path, work_idx);

        /* 2.2 发送处理命令 */
        ret = usck_udp_send(recv->cmd_sck_id, path, &cmd, sizeof(smtp_cmd_t));
        if (0 != ret)
        {
            if (++times < 3)
            {
                goto AGAIN;
            }
            log_debug(recv->log, "Send command failed! errmsg:[%d] %s! path:[%s]",
                    errno, strerror(errno), path);
            continue;
        }

        recv->delay_total[idx] = 0;
    }

    return;
}

/******************************************************************************
 ** Name : smtp_rsvr_exp_data_proc
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
static int smtp_rsvr_exp_data_proc(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    int ret = 0;
    smtp_read_snap_t *read = &sck->read;

    ++recv->recv_total; /* 总数 */

    /* 1. 是否在NULL空间: 直接丢弃 */
    if (read->addr == sck->null)
    {
        ++recv->drop_total;  /* 丢弃计数 */

        log_error(recv->log, "Lost data! tidx:[%d] fd:[%d] recv:%llu drop:%llu error:%llu",
                recv->tidx, sck->fd, recv->recv_total,
                recv->drop_total, recv->err_total);
        return SMTP_OK;
    }

    /* 2. 放入队列中 */
    ret = orm_queue_push(ctx->recvq[read->rqidx], read->dataid);
    if (ret < 0)
    {
        orm_queue_data_free(ctx->recvq[read->rqidx], read->dataid);

        ++recv->drop_total;  /* 丢弃计数 */

        log_error(recv->log, "Push failed! tidx:[%d] dataid:[%d] recv:%llu drop:%llu error:%llu",
                recv->tidx, read->dataid, recv->recv_total,
                recv->drop_total, recv->err_total);
        return SMTP_OK;  /* Note: Don't return error */
    }

    /* 1. Notify work-thread */
    ret = smtp_rsvr_send_work_cmd(ctx, recv, sck);
    if (SMTP_OK != ret)
    {
        /* log_error(recv->log, "errmsg:[%d] %s!", errno, strerror(errno)); */
        return SMTP_OK;  /* Note: Don't return error, resend at next time */
    }

    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_proc_data
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
static int smtp_rsvr_proc_data(smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = read->addr;

    /* 1. Handle system data */
    if (SMTP_SYS_DATA == head->flag)
    {
        return smtp_rsvr_sys_data_proc(ctx, recv, sck);
    }

    /* 2. Forward expand data */
    return smtp_rsvr_exp_data_proc(ctx, recv, sck);
}

/******************************************************************************
 ** Name : smtp_rsvr_core_handler
 ** Desc : Recv handler
 ** Input: 
 **     ctx: Global context
 **     recv: Recv context
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Recv command
 **     2. Recv data from client
 ** Note : 
 ** Author: # Qifeng.zou # 2014.03.28 #
 ******************************************************************************/
static int smtp_rsvr_core_handler(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    int ret = 0;

    /* 1. Recv command */
    ret = smtp_rsvr_recv_cmd(ctx, recv);
    if (SMTP_OK != ret)
    {
        log_error(recv->log, "Recv command failed!");
    }
    
    /* 2. Traverse recv */
    ret = smtp_rsvr_trav_recv(ctx, recv);
    if (SMTP_OK != ret)
    {
        log_error(recv->log, "Recv data from client failed!");
    }

    /* 3. Traverse send */
    ret = smtp_rsvr_trav_send(ctx, recv);
    if (SMTP_OK != ret)
    {
        log_error(recv->log, "Recv data from client failed!");
    }


    return SMTP_ERR;
}

/******************************************************************************
 ** Name : smtp_rsvr_timeout_handler
 ** Desc : Recv timeout handler
 ** Input: 
 **     ctx: Context
 **     recv: Recv context
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.02 #
 ******************************************************************************/
static void smtp_rsvr_timeout_handler(smtp_cntx_t *ctx, smtp_rsvr_t *recv)
{
    smtp_sck_t *curr = NULL, *next = NULL;

    /* 1. Check timeout connection */
    curr = recv->sck;
    recv->tm = time(NULL);
    while (NULL != curr)
    {
        if (recv->tm - curr->rtm >= 2*SMTP_SCK_KPALIVE_SEC)
        {
            log_trace("Didn't recv data for along time! fd:[%d] ip:[%s]",
                curr->fd, curr->ipaddr);

            /* Release memory which was alloced at recv data */
            smtp_rsvr_read_release(ctx, recv, curr);

            /* Remove sckid from list */
            next = curr->next;
            smtp_rsvr_del_sck_hdl(recv, curr);

            curr = next;
            continue;
        }

        curr = curr->next;
    }

    /* 2. Resend failed command */
    smtp_rsvr_resend_work_cmd(ctx, recv);
}

/******************************************************************************
 ** Name : smtp_rsvr_keepalive_req_hdl
 ** Desc : Keepalive request handler
 ** Input: 
 **     ctx: Global context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     > Done : SMTP_HDL_DONE
 **     > Fail : SMTP_FAILED
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.01 #
 ******************************************************************************/
static int smtp_rsvr_keepalive_req_hdl(
        smtp_cntx_t *ctx, smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    void *addr;
    smtp_header_t *head;

    addr = eslab_alloc(&recv->pool, sizeof(smtp_header_t));
    if (NULL == addr)
    {
        log_error(recv->log, "Alloc memory from slab failed!");
        return SMTP_ERR;
    }

    head = (smtp_header_t *)addr;

    head->type = SMTP_KPALIVE_REP;
    head->body_len = 0;
    head->flag = SMTP_SYS_DATA;
    head->mark = SMTP_MSG_MARK_KEY;
    
    smtp_rsvr_add_msg(recv, sck, addr);

    log_debug(recv->log, "Add respond of keepalive request!");

    return SMTP_HDL_DONE;
}

/******************************************************************************
 ** Name : smtp_rsvr_link_info_report_hdl
 ** Desc : link info report handler
 ** Input: 
 **     ctx: Global context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     > Done : SMTP_HDL_DONE
 **     > Fail : SMTP_FAILED
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.08.14 #
 ******************************************************************************/
static int smtp_rsvr_link_info_report_hdl(smtp_cntx_t *ctx, smtp_sck_t *sck)
{
    smtp_link_info_report_t *info;
    smtp_read_snap_t *read = &sck->read;

    info = (smtp_link_info_report_t *)(read->addr + sizeof(smtp_header_t));

    sck->is_primary = info->is_primary;

    return SMTP_HDL_DONE;
}

/******************************************************************************
 ** Name : smtp_rsvr_cmd_add_sck_hdl
 ** Desc : ADD SCK request handler
 ** Input: 
 **     recv: Recv对象
 **     req: Parameter of ADD SCK
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smtp_rsvr_cmd_add_sck_hdl(smtp_rsvr_t *recv, smtp_cmd_add_sck_t *req)
{
    smtp_sck_t *add;

    /* 1. 为新结点分配空间 */
    add = eslab_alloc(&recv->pool, sizeof(smtp_sck_t));
    if (NULL == add)
    {
        log_error(recv->log, "Alloc memory failed!");
        return SMTP_ERR;
    }

    add->fd = req->sckid;
    add->ctm = time(NULL);
    add->rtm = add->ctm;
    add->wtm = add->ctm;
    snprintf(add->ipaddr, sizeof(add->ipaddr), "%s", req->ipaddr);

    add->read.dataid = NULLID;
    add->read.addr = NULL;

    /* 2. 将结点加入到套接字链表 */
    if (NULL == recv->sck)
    {
        recv->sck = add;
        add->next = NULL;
    }
    else
    {
        add->next = recv->sck;
        recv->sck = add;
    }

#if defined(__SMTP_DEBUG_PRINT__)
    smtp_rsvr_print_all_sck(recv);
#endif /*__SMTP_DEBUG_PRINT__*/

    ++recv->connections; /* 统计TCP连接数 */

    log_trace("Tidx [%d] insert sckid [%d] success! ip:[%s]",
        recv->tidx, req->sckid, req->ipaddr);
    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_rsvr_del_sck_hdl
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
static int smtp_rsvr_del_sck_hdl(smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    smtp_sck_t *curr, *prev;

    curr = recv->sck;
    prev = curr;
    while (NULL != curr)
    {
        if (sck == curr)
        {
            if (prev == curr)
            {
                recv->sck = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }

            Close(curr->fd);
            smtp_rsvr_clear_msg(recv, curr);
            curr->recv_total = 0;

            if (NULL != curr->null)
            {
                eslab_free(&recv->pool, curr->null);
            }
            eslab_free(&recv->pool, curr);

            --recv->connections; /* 统计TCP连接数 */

            return SMTP_OK;
        }

        prev = curr;
        curr= curr->next;
    }

    return SMTP_OK; /* Didn't found */
}

/******************************************************************************
 ** Name : smtp_rsvr_del_all_sck_hdl
 ** Desc : 删除接收线程所有的套接字
 ** Input: 
 **     recv: Recv对象
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static void smtp_rsvr_del_all_sck_hdl(smtp_rsvr_t *recv)
{
    smtp_sck_t *curr, *next;

    curr = recv->sck; 
    while (NULL != curr)
    {
        next = curr->next;

        Close(curr->fd);
        smtp_rsvr_clear_msg(recv, curr);

        if (NULL != curr->null)
        {
            eslab_free(&recv->pool, curr->null);
        }
        eslab_free(&recv->pool, curr);

        curr = next;
    }

    recv->connections = 0; /* 统计TCP连接数 */
    recv->sck = NULL;
    return;
}

/******************************************************************************
 **函数名称: smtp_rsvr_add_msg
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
static int smtp_rsvr_add_msg(smtp_rsvr_t *recv, smtp_sck_t *sck, void *addr)
{
    list_t *add, *item, *tail = NULL;

    /* 1.创建新结点 */
    add = eslab_alloc(&recv->pool, sizeof(list_t));
    if (NULL == add)
    {
        log_debug(recv->log, "Alloc memory failed!");
        return SMTP_ERR;
    }

    add->data = addr;
    add->next = NULL;

    /* 2.插入链尾 */
    item = sck->message_list;
    if (NULL == item)
    {
        sck->message_list = add;
        return SMTP_OK;
    }

    /* 3.查找链尾 */
    do
    {
        tail = item;
        item = item->next;
    }while (NULL != item);

    tail->next = add;

    return SMTP_OK;
}

/******************************************************************************
 **函数名称: smtp_rsvr_get_msg
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
static void *smtp_rsvr_get_msg(smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    void *addr;
    list_t *curr = sck->message_list;

    if (NULL == curr)
    {
        return NULL;
    }
    
    sck->message_list = curr->next;
    addr = curr->data;

    eslab_free(&recv->pool, curr);

    return addr;
}

/******************************************************************************
 **函数名称: smtp_rsvr_clear_msg
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
static int smtp_rsvr_clear_msg(smtp_rsvr_t *recv, smtp_sck_t *sck)
{
    list_t *curr, *next;

    curr = sck->message_list; 
    while (NULL != curr)
    {
        next = curr->next;

        eslab_free(&recv->pool, curr->data);
        eslab_free(&recv->pool, curr);

        curr = next;
    }

    sck->message_list = NULL;
    return SMTP_OK;
}

#if defined(__SMTP_DEBUG_PRINT__)
/******************************************************************************
 ** Name : smtp_print_conf
 ** Desc : Print configuration
 ** Input: 
 **     conf: Configuration
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
static void smtp_print_conf(const smtp_conf_t *conf)
{
    log_debug(recv->log, "CONF - Name:%s Port:%d RecvThdNum:%d WorkThdNum:%d",
            conf->name, conf->port, conf->recv_thd_num, conf->recv_thd_num);
}

/******************************************************************************
 ** Name : smtp_print_listen
 ** Desc : Print listen-thread
 ** Input: 
 **     lsvr: Accept thread
 ** Output: NONE
 ** Return: void 
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
static void smtp_print_listen(const smtp_lsn_svr_t *lsvr)
{
    log_debug(recv->log, "tid: %lu cmd_sck_id:%d lsn_sck_id:%d",
            lsvr->tid, lsvr->cmd_sck_id, lsvr->lsn_sck_id);
}

/******************************************************************************
 ** Name : smtp_print_recvtp
 ** Desc : Print recv-thread-pool
 ** Input: 
 **     recvtp: Recv thread-pool
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
static void smtp_print_recvtp(const thread_pool_t *recvtp)
{
    int idx;
    const smtp_rsvr_t *recv = (const smtp_rsvr_t *)recvtp->data;

    for (idx=0; idx<recvtp->num; idx++, recv++)
    {
        log_debug(recv->log, "RecvThreadPool - cmfd:%d tidx:%d max:%d rdset:%d",
                recv->cmd_sck_id, recv->tidx, recv->max, recv->rdset);

        smtp_scks_print(&recv->scks);
    }
}

/******************************************************************************
 ** Name : smtp_print_worktp
 ** Desc : Print work-thread-pool
 ** Input: 
 **     tpool: Thread pool
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
static void smtp_print_worktp(const thread_pool_t *worktp)
{
    int idx;
    const smtp_work_t *wsvr = (const smtp_work_t *)worktp->data;

    for (idx=0; idx<worktp->num; idx++, work++)
    {
        log_debug(recv->log, "WorkThreadPool - cmfd:%d tidx:%d max:%d rdset:%d",
                wsvr->cmd_sck_id, wsvr->tidx, wsvr->max, wsvr->rdset);
    }
}

/******************************************************************************
 ** Name : smtp_print_reg
 ** Desc : Print register
 ** Input: 
 **     reg: Register
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
static void smtp_print_reg(const smtp_reg_t *reg)
{
    int idx;
    
    for (idx=0; idx<SMTP_TYPE_MAX; ++idx)
    {
        if (1 != reg[idx].flag)
        {
            continue;
        }

        log_debug(recv->log, "Register - type:%d cb:%p args:%p", reg[idx].type, reg[idx].cb, reg[idx].args);
    }
}

/******************************************************************************
 ** Name : smtp_print_cntx
 ** Desc : Print main context
 ** Input: 
 **     ctx: Global context
 ** Output: NONE
 ** Return: void
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
void smtp_print_cntx(const smtp_cntx_t *ctx)
{
    smtp_print_conf(&ctx->conf);
    smtp_print_listen(&ctx->listen);
    smtp_print_recvtp(ctx->recvtp);
    smtp_print_worktp(ctx->worktp);
    smtp_print_reg(&ctx->reg);
}
#endif /*__SMTP_DEBUG_PRINT__*/

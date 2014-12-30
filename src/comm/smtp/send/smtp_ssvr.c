#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "smtp_cmd.h"
#include "smtp_cli.h"
#include "smtp_ssvr.h"

#define SMTP_NULL_MAX_LEN        (10*1024)   /* NULL空间最大长度 */

/* 释放发送的数据 */
#define smtp_ssvr_release_send_data(ssvr, send) \
{ \
    switch (send->loc) \
    { \
        case SMTP_DATA_LOC_ORM_QUEUE: \
        { \
            orm_queue_data_free(ssvr->sq, send->data_loc_sendq.dataid); \
            break; \
        } \
        case SMTP_DATA_LOC_SLAB: \
        { \
            slab_dealloc(&ssvr->pool, send->addr); \
            send->addr = NULL; \
            break; \
        } \
    } \
}

/* 静态函数 */
static void *smtp_ssvr_routine(void *args);

static int smtp_ssvr_init(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr);
static smtp_ssvr_t *smtp_get_curr_ssvr(smtp_ssvr_ctx_t *ctx);

static int smtp_ssvr_creat_sendq(smtp_ssvr_t *ssvr, const smtp_ssvr_conf_t *conf);
static int smtp_ssvr_creat_usck(smtp_ssvr_t *ssvr, const smtp_ssvr_conf_t *conf);
static int smtp_ssvr_connect(smtp_ssvr_t *ssvr, const smtp_ssvr_conf_t *conf);

static int smtp_ssvr_kpalive_req(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr);
static int smtp_ssvr_link_info_report(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr);

static int smtp_ssvr_recv_cmd_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr);
static int smtp_ssvr_recv_data_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr);

static void smtp_ssvr_read_release(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);
static int smtp_ssvr_read_init(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);
static int smtp_ssvr_read_header(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);
static int smtp_ssvr_read_body(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);

static int smtp_ssvr_proc_data(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);
static int smtp_ssvr_sys_data_proc(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);
static int smtp_ssvr_exp_data_proc(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);

static int smtp_ssvr_timeout_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr);
static int smtp_ssvr_cmd_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, const smtp_cmd_t *cmd);
static int smtp_ssvr_send_all_cmd_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, const smtp_cmd_send_t *args);

static int smtp_ssvr_add_msg(smtp_ssvr_t *ssvr, void *addr);
static void *smtp_ssvr_get_msg(smtp_ssvr_t *ssvr);
static int smtp_ssvr_clear_msg(smtp_ssvr_t *ssvr);

static void smtp_ssvr_reset_sck(smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck);

/* 链路状态检测 */
#define smtp_ssvr_conn_check(ssvr) (ssvr->sck.fd >= 0)? 1 : 0 /* 0:ERR 1:OK */

/******************************************************************************
 **函数名称: smtp_ssvr_creat_sendtp
 **功    能: 创建SND线程池
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
int smtp_ssvr_creat_sendtp(smtp_ssvr_ctx_t *ctx)
{
    int ret = 0, idx = 0;
    smtp_ssvr_conf_t *conf = &ctx->conf;
    smtp_ssvr_t *ssvr = NULL;

    /* 1. 创建SND线程池 */
    ret = thread_pool_init(&ctx->sendtp, conf->snd_thd_num);
    if (0 != ret)
    {
        thread_pool_destroy(ctx->sendtp);
        ctx->sendtp = NULL;
        return SMTP_ERR;
    }

    /* 2. 创建SND线程上下文 */
    ctx->sendtp->data = calloc(conf->snd_thd_num, sizeof(smtp_ssvr_t));
    if (NULL == ctx->sendtp->data)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTP_ERR;
    }

    /* 3. 设置SND线程上下文 */
    ssvr = (smtp_ssvr_t *)ctx->sendtp->data;
    for (idx=0; idx<conf->snd_thd_num; ++idx, ++ssvr)
    {
        ssvr->tidx = idx;

        ret = smtp_ssvr_init(ctx, ssvr);
        if (0 != ret)
        {
            log_error(ssvr->log, "Initialize send thread failed!");
            return SMTP_ERR;
        }
    }

    /* 4. 注册SND线程回调 */
    for (idx=0; idx<conf->snd_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, smtp_ssvr_routine, ctx);
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_sendtp_destroy
 **功    能: 销毁Send线程
 **输入参数: 
 **     ctx: 发送端上下文
 **     args: 线程池参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.04 #
 ******************************************************************************/
void smtp_ssvr_sendtp_destroy(void *_ctx, void *args)
{
    int idx = 0;
    smtp_ssvr_ctx_t *ctx = (smtp_ssvr_ctx_t *)ctx;
    smtp_ssvr_conf_t *conf = &ctx->conf;
    smtp_ssvr_t *ssvr = (smtp_ssvr_t *)args;

    /* 3. 设置SND线程上下文 */
    for (idx=0; idx<conf->snd_thd_num; ++idx, ++ssvr)
    {
        Close(ssvr->cmd_sck_id);
        Close(ssvr->sck.fd);
        orm_queue_free(ssvr->sq);
    }

    free(args);

    return;
}

/******************************************************************************
 **函数名称: smtp_ssvr_init
 **功    能: 初始化发送线程
 **输入参数: 
 **     ctx: 上下文信息
 **     ssvr: Send线程上下文
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
static int smtp_ssvr_init(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    int ret = 0;
    smtp_ssvr_conf_t *conf = &ctx->conf;
    smtp_send_snap_t *send = &ssvr->sck.send;

    /* 1. 创建发送队列 */
    ret = smtp_ssvr_creat_sendq(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SMTP_ERR;
    }

    /* 2. 创建unix套接字 */
    ret = smtp_ssvr_creat_usck(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SMTP_ERR;
    }

    /* 3. 创建SLAB内存池 */
    ret = slab_init(&ssvr->pool, SMTP_MEM_POOL_SIZE);
    if (0 != ret)
    {
        log_error(ssvr->log, "Initialize slab mem-pool failed!");
        return SMTP_ERR;
    }

    ssvr->sck.null = slab_alloc(&ssvr->pool, SMTP_NULL_MAX_LEN);
    if (NULL == ssvr->sck.null)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return SMTP_ERR;
    }

    /* 4. 连接接收服务器 */
    ret = smtp_ssvr_connect(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connect recv server failed!");
        /* Note: Don't return error! */
    }

    smtp_reset_send_snap(send);
    
    smtp_ssvr_link_info_report(ctx, ssvr);

    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_creat_sendq
 **功    能: 创建SND线程的发送队列
 **输入参数: 
 **     ssvr: Send线程上下文
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static int smtp_ssvr_creat_sendq(smtp_ssvr_t *ssvr, const smtp_ssvr_conf_t *conf)
{
    const smtp_queue_conf_t *qcf;
    char qname[FILE_NAME_MAX_LEN];


    /* 1. 创建/连接发送队列 */
    qcf = &conf->send_qcf;
    snprintf(qname, sizeof(qname), "%s-%d", qcf->name, ssvr->tidx);

    ssvr->sq = orm_queue_new(qcf->size, qcf->count, qcf->flag, qname);
    if (NULL == ssvr->sq)
    {
        log_error(ssvr->log, "Create send-queue failed!");
        return SMTP_ERR;
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_creat_usck
 **功    能: 创建SND线程的命令接收套接字
 **输入参数: 
 **     ssvr: Send线程上下文
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static int smtp_ssvr_creat_usck(smtp_ssvr_t *ssvr, const smtp_ssvr_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN] = {0};

    smtp_ssvr_usck_path(conf, path, ssvr->tidx);
    
    ssvr->cmd_sck_id = usck_udp_creat(path);
    if (ssvr->cmd_sck_id < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return -1;
    }

    LogTrace("cmd_sck_id:[%d] path:%s", ssvr->cmd_sck_id, path);
    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_connect
 **功    能: 连接远程服务器
 **输入参数: 
 **     ctx: 上下文信息
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.25 #
 ******************************************************************************/
static int smtp_ssvr_connect(smtp_ssvr_t *ssvr, const smtp_ssvr_conf_t *conf)
{
    int ret, opt = 1;
    struct sockaddr_in svraddr;
    smtp_ssvr_sck_t *sck = &ssvr->sck;


    /* 1. 创建套接字 */
    sck->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sck->fd < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTP_ERR;
    }

    /* 解决TIME_WAIT状态过多的问题 */
    opt = 1;
    setsockopt(sck->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 2. 连接远程Recv服务 */
    bzero(&svraddr, sizeof(svraddr));

    svraddr.sin_family = AF_INET;
    inet_pton(AF_INET, conf->ipaddr, &svraddr.sin_addr);
    svraddr.sin_port = htons(conf->port);

    ret = connect(sck->fd, (struct sockaddr *)&svraddr, sizeof(svraddr));
    if (0 != ret)
    {
        Close(sck->fd);
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTP_ERR;
    }

    fd_nonblock(sck->fd);
    
    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_bind_cpu
 **功    能: 绑定CPU
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.06.11 #
 ******************************************************************************/
static void smtp_ssvr_bind_cpu(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    int idx = 0, mod = 0;
    cpu_set_t cpuset;
    smtp_cpu_conf_t *cpu = &ctx->conf.cpu;

    mod = sysconf(_SC_NPROCESSORS_CONF) - cpu->start;
    if (mod <= 0)
    {
        idx = ssvr->tidx % sysconf(_SC_NPROCESSORS_CONF);
    }
    else
    {
        idx = cpu->start + (ssvr->tidx % mod);
    }

    CPU_ZERO(&cpuset);
    CPU_SET(idx, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

/******************************************************************************
 **函数名称: smtp_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数: 
 **     ssvr: Send线程上下文
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.22 #
 ******************************************************************************/
void smtp_ssvr_set_rwset(smtp_ssvr_t *ssvr) 
{ 
    smtp_header_t *head;
    smtp_send_snap_t *send = &ssvr->sck.send;

    FD_ZERO(&ssvr->rset);
    FD_ZERO(&ssvr->wset);

    FD_SET(ssvr->cmd_sck_id, &ssvr->rset);

    if (ssvr->sck.fd < 0)
    {
        return;
    }

    ssvr->max = (ssvr->cmd_sck_id > ssvr->sck.fd)? ssvr->cmd_sck_id : ssvr->sck.fd;

    /* 1 设置读集合 */
    FD_SET(ssvr->sck.fd, &ssvr->rset);

    /* 2 设置写集合: 发送至接收端 */
    if (NULL != send->addr
        || NULL != ssvr->sck.message_list
        || 0 != orm_queue_data_count(ssvr->sq))
    {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
    }

    return;
}

/******************************************************************************
 **函数名称: smtp_ssvr_event_handler
 **功    能: Send主程序
 **输入参数: 
 **     ctx: 上下文信息
 **     ssvr: Send上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 从队列中读取信息
 **     2. 将数据发送至远程服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.25 #
 ******************************************************************************/
static int smtp_ssvr_event_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    fd_set *wset = NULL;
    int ret = 0, intv = SMTP_RECONN_MIN_INTV;
    struct timeval tmout;
    smtp_ssvr_sck_t *sck = sck = &ssvr->sck;
    smtp_send_snap_t *send = &ssvr->sck.send;


    for (;;)
    {
        /* 2. 连接合法性判断 */
        if (!smtp_ssvr_conn_check(ssvr))
        {
            smtp_ssvr_clear_msg(ssvr);

            /* 2.1 重连Recv端 */
            ret = smtp_ssvr_connect(ssvr, &ctx->conf);
            if (ret < 0)
            {
                log_error(ssvr->log, "Conncet recv server failed!");

                if (intv >  SMTP_RECONN_MAX_INTV)
                {
                    intv = SMTP_RECONN_MAX_INTV;
                }
                Sleep(intv);
                intv <<= 1;
                continue;
            }

            smtp_ssvr_link_info_report(ctx, ssvr);

            intv = SMTP_RECONN_MIN_INTV;
        }

        /* 3. 接收数据&命令 */
        smtp_ssvr_set_rwset(ssvr);

        tmout.tv_sec = SMTP_SND_TMOUT_SEC;
        tmout.tv_usec = SMTP_SND_TMOUT_USEC;
        ret = select(ssvr->max+1, &ssvr->rset, &ssvr->wset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            LogTrace("errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
        else if (0 == ret)
        {
            smtp_ssvr_timeout_handler(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(ssvr->sck.fd, &ssvr->wset))
        {
            smtp_ssvr_send_all_cmd_handler(ctx, ssvr, NULL);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            smtp_ssvr_recv_cmd_handler(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(ssvr->sck.fd, &ssvr->rset))
        {
            smtp_ssvr_recv_data_handler(ctx, ssvr);
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: smtp_ssvr_routine
 **功    能: Snd线程调用的主程序
 **输入参数: 
 **     args: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 获取发送线程
 **     2. 绑定CPU
 **     3. 调用发送主程
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.25 #
 ******************************************************************************/
static void *smtp_ssvr_routine(void *args)
{
    smtp_ssvr_t *ssvr = NULL;
    smtp_ssvr_ctx_t *ctx = (smtp_ssvr_ctx_t *)args;

    LogDebug("Send-thread is running!");

    /* 1. 获取发送线程 */
    ssvr = smtp_get_curr_ssvr(ctx);
    if (NULL == ssvr)
    {
        log_error(ssvr->log, "Init send thread failed!");
        pthread_exit(NULL);
        return (void *)-1;
    }

    /* 2. 绑定指定CPU */
    smtp_ssvr_bind_cpu(ctx, ssvr);

    smtp_ssvr_event_handler(ctx, ssvr);

    pthread_exit(NULL);
    return (void *)-1;
}

/******************************************************************************
 **函数名称: smtp_ssvr_kpalive_req
 **功    能: 发送保活命令
 **输入参数: 
 **     ctx: 上下文信息
 **     ssvr: Snd线程上下文
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     因发送KeepAlive请求时，说明链路空闲时间较长，
 **     因此发送数据时，不用判断EAGAIN的情况是否存在。
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
static int smtp_ssvr_kpalive_req(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    void *addr;
    int ret = 0;
    smtp_ssvr_sck_t *sck = &ssvr->sck;
    smtp_send_snap_t *send = &ssvr->sck.send;
    smtp_header_t *head;
    int size = sizeof(smtp_header_t);

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0) 
        || (SMTP_KPALIVE_SENT == sck->kpalive)) 
    {
        Close(sck->fd);

        smtp_ssvr_release_send_data(ssvr, send);
        smtp_reset_send_snap(send);

        log_error(ssvr->log, "Didn't get keep-alive respond for a long time!");
        return 0;
    }

    addr = slab_alloc(&ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return 0;
    }

    /* 2. 设置心跳数据 */
    head = (smtp_header_t *)addr;

    head->type = SMTP_KPALIVE_REQ;
    head->body_len = 0;
    head->flag = SMTP_SYS_DATA;
    head->mark = SMTP_MSG_MARK_KEY;

    /* 3. 加入发送列表 */
    smtp_ssvr_add_msg(ssvr, addr);

    LogDebug("Add keepalive request success! fd:[%d]", sck->fd);

    smtp_set_kpalive_stat(sck, SMTP_KPALIVE_SENT);
    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_link_info_report
 **功    能: 发送链路信息报告
 **输入参数: 
 **     ssvr: Snd线程上下文
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.08.14 #
 ******************************************************************************/
static int smtp_ssvr_link_info_report(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    void *addr;
    int ret = 0;
    smtp_ssvr_sck_t *sck = &ssvr->sck;
    smtp_send_snap_t *send = &ssvr->sck.send;
    smtp_header_t *head;
    smtp_link_info_report_t *info;
    int size = sizeof(smtp_header_t) + sizeof(smtp_link_info_report_t);

    /* 1. 申请空间 */
    addr = slab_alloc(&ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return 0;
    }

    /* 2. 设置心跳数据 */
    head = (smtp_header_t *)addr;

    head->type = SMTP_LINK_INFO_REPORT;
    head->body_len = sizeof(smtp_link_info_report_t);
    head->flag = SMTP_SYS_DATA;
    head->mark = SMTP_MSG_MARK_KEY;

    info = addr + sizeof(smtp_header_t);

    info->is_primary = (0 == ssvr->tidx)? 1 : 0;

    /* 3. 加入发送列表 */
    smtp_ssvr_add_msg(ssvr, addr);

    LogDebug("Add link info report! fd:[%d]", sck->fd);
    return 0;
}

/******************************************************************************
 **函数名称: smtp_get_curr_ssvr
 **功    能: 获取当前SND线程的上下文
 **输入参数: 
 **     ssvr: Send线程上下文
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: Address of sndsvr
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static smtp_ssvr_t *smtp_get_curr_ssvr(smtp_ssvr_ctx_t *ctx)
{
    int tidx = -1;
    smtp_ssvr_t *ssvr = NULL;

    /* 1. 获取线程索引 */
    tidx = thread_pool_get_tidx(ctx->sendtp);
    if (tidx < 0)
    {
        log_error(ssvr->log, "Get index of current thread failed!");
        return NULL;
    }

    /* 2. 获取SND线程上下文 */
    ssvr = (smtp_ssvr_t *)(ctx->sendtp->data + tidx * sizeof(smtp_ssvr_t));
    ssvr->tidx = tidx;

    return ssvr;
}

/******************************************************************************
 **函数名称: smtp_ssvr_timeout_handler
 **功    能: 超时处理
 **输入参数: 
 **     ctx: 上下文信息
 **     ssvr: Send线程上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断是否长时间无数据通信
 **     2. 发送保活数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static int smtp_ssvr_timeout_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    int ret = 0;
    time_t curr_tm = time(NULL);
    smtp_ssvr_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((NULL != ssvr->sck.send.addr)
        || (curr_tm - sck->wr_tm) < SMTP_SCK_KPALIVE_SEC)
    {
        return 0;
    }

    /* 2. 发送保活请求 */
    ret = smtp_ssvr_kpalive_req(ctx, ssvr);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return -1;
    }

    sck->wr_tm = curr_tm;

    return 0;
}

/******************************************************************************
 ** Name : smtp_ssvr_check_header
 ** Desc : Check head
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.05.13 #
 ******************************************************************************/
static int smtp_ssvr_check_header(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = (smtp_header_t *)read->addr;

    /* 1. 检查校验值 */
    if (SMTP_MSG_MARK_KEY != head->mark)
    {
        log_error(ssvr->log, "Mark [%u/%u] isn't right! type:%d len:%d flag:%d",
            head->mark, SMTP_MSG_MARK_KEY, head->type, head->body_len, head->flag);
        return SMTP_ERR;
    }

    /* 2. 检查类型 */
    if (!smtp_is_type_valid(head->type))
    {
        log_error(ssvr->log, "Data type is invalid! type:%d len:%d", head->type, head->body_len);
        return SMTP_ERR;
    }
 
    /* 3. 检查长度: 因所有队列长度一致 因此使用[0]判断 */
    if (head->body_len + sizeof(smtp_header_t) >= SMTP_NULL_MAX_LEN)
    {
        log_error(ssvr->log, "Length is too long! type:%d len:%d", head->type, head->body_len);
        return SMTP_ERR;
    }

    return SMTP_OK;
}



/******************************************************************************
 ** Name : smtp_ssvr_read_init
 ** Desc : Prepare ssvr read data
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 **     只有2种返回结果：Succ 或 队列空间不足
 ** Author: # Qifeng.zou # 2014.05.13 #
 ******************************************************************************/
static int smtp_ssvr_read_init(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    int times = 0;
    smtp_read_snap_t *read = &sck->read;

    /* 指向NULL空间 */
    read->rqidx = 0;
    read->dataid = 0;
    read->addr = sck->null;

    /* 3. 设置标识量 */
    read->offset = 0;
    smtp_set_read_phase(read, SMTP_PHASE_READ_HEAD);
    
    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_ssvr_read_header
 ** Desc : Recv head
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
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
 ** Author: # Qifeng.zou # 2014.08.10 #
 ******************************************************************************/
static int smtp_ssvr_read_header(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
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
            log_error(ssvr->log, "Server disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                errno, strerror(errno), sck->fd, n, left);
            return SMTP_SCK_CLOSED;
        }

        log_error(ssvr->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SMTP_ERR;
    }
    
    read->offset += n;

    /* 3. Check head */
    ret = smtp_ssvr_check_header(ctx, ssvr, sck);
    if (SMTP_OK != ret)
    {
        log_error(ssvr->log, "Check header failed! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->body_len, head->flag, head->mark, SMTP_MSG_MARK_KEY);
        return SMTP_ERR;
    }

    read->total = sizeof(smtp_header_t) + head->body_len;
    smtp_set_read_phase(read, SMTP_PHASE_READ_BODY);

    return SMTP_DONE;
}

/******************************************************************************
 ** Name : smtp_ssvr_read_body
 ** Desc : Recv body
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return:
 **     1. incomplete - SMTP_AGAIN
 **     2. done  - SMTP_DONE
 **     3. close - SMTP_SCK_CLOSED
 **     4. failed - SMTP_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but ssvr again at next time.
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smtp_ssvr_read_body(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    int n = 0, left = 0;
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
            log_error(ssvr->log, "Client disconnected. errmsg:[%d] %s! "
                "fd:[%d] type:[%d] flag:[%d] bodylen:[%d] total:[%d] left:[%d] offset:[%d]",
                errno, strerror(errno),
                sck->fd, head->type, head->flag, head->body_len, read->total, left, read->offset);
            return SMTP_SCK_CLOSED;
        }

        log_error(ssvr->log, "errmsg:[%d] %s! type:%d len:%d n:%d fd:%d total:%d offset:%d addr:%p",
            errno, strerror(errno), head->type,
            head->body_len, n, sck->fd, read->total, read->offset, read->addr);

        return SMTP_ERR;
    }

    /* 2. Set flag variables */
    read->offset += n;
    smtp_set_read_phase(read, SMTP_PHASE_READ_POST);

    LogTrace("Recv success! type:%d len:%d", head->type, head->body_len);
    
    return SMTP_DONE;
}

/******************************************************************************
 ** Name : smtp_ssvr_proc_data
 ** Desc : Process data from server.
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Send server 
 **     sck: Socket info
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Handle system data
 **     2. Forward expand data
 ** Note : 
 ** Author: # Qifeng.zou # 2014.05.13 #
 ******************************************************************************/
static int smtp_ssvr_proc_data(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = read->addr;

    /* 1. Handle system data */
    if (SMTP_SYS_DATA == head->flag)
    {
        return smtp_ssvr_sys_data_proc(ctx, ssvr, sck);
    }

    /* 2. Forward expand data */
    return smtp_ssvr_exp_data_proc(ctx, ssvr, sck);
}

/******************************************************************************
 **函数名称: smtp_ssvr_recv_data_handler
 **功    能: 接收来自服务器的信息
 **输入参数: 
 **     ctx: 上下文信息
 **     ssvr: Send线程上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 读取消息HEAD
 **     2. 读取消息BODY
 **     3. 处理消息内容
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
static int smtp_ssvr_recv_data_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    int ret = 0;
    smtp_ssvr_sck_t *sck = &ssvr->sck;
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head;

    sck->rd_tm = time(NULL);

    switch (read->phase)
    {
        case SMTP_PHASE_READ_INIT: /* 1. 初始化读取消息 */
        {
            smtp_ssvr_read_init(ctx, ssvr, sck);
            /* NOTE: Don't break - Continue handle */
        }
        case SMTP_PHASE_READ_HEAD: /* 2. 读取消息HEAD */
        {
            ret = smtp_ssvr_read_header(ctx, ssvr, sck);
            if (SMTP_DONE == ret)
            {
                head = (smtp_header_t *)read->addr;
                if (head->body_len > 0)
                {
                    return SMTP_OK;
                }
                goto READ_POST_STEP;
            }
            else if (SMTP_AGAIN == ret)  /* incomplete */
            {
                /* Note: Continue ssvr head at next loop */
                return SMTP_OK;
            }
            else if (SMTP_SCK_CLOSED == ret)
            {
                LogDebug("Server disconnect!");
                break;
            }
            else
            {
                log_error(ssvr->log, "Recv head failed!");
                break; /* error */
            }
            /* NOTE: Don't break - Continue handle */
        }
        case SMTP_PHASE_READ_BODY: /* 3. 读取消息BODY */
        {
            ret = smtp_ssvr_read_body(ctx, ssvr, sck);
            if (SMTP_DONE == ret)
            {
                goto READ_POST_STEP;
            }
            else if (SMTP_AGAIN == ret)
            {
                /* Note: Continue ssvr body at next loop */
                return SMTP_OK;
            }
            else if (SMTP_HDL_DISCARD == ret)
            {
                smtp_ssvr_read_release(ctx, ssvr, sck);
                return SMTP_OK;
            }
            else if (SMTP_SCK_CLOSED == ret)
            {
                LogDebug("Client disconnect!");
                break;
            }
            else
            {
                log_error(ssvr->log, "Recv body failed!");
                break; /* error */
            }
            /* NOTE: Don't break - Continue handle */
        }
        case SMTP_PHASE_READ_POST:
        {
        READ_POST_STEP:
            ret = smtp_ssvr_proc_data(ctx, ssvr, sck);
            if (SMTP_OK == ret)
            {
                smtp_reset_read_snap(read);
                return SMTP_OK;
            }
            else if ((SMTP_HDL_DONE == ret)
                || (SMTP_HDL_DISCARD == ret))
            {
                smtp_ssvr_read_release(ctx, ssvr, sck);
                return SMTP_OK;
            }

            break;
        }
        default:
        {
            log_error(ssvr->log, "Unknown step!");
            break;
        }
    }

    smtp_ssvr_read_release(ctx, ssvr, sck);
    smtp_ssvr_reset_sck(ssvr, sck);
    return SMTP_ERR;
}

/******************************************************************************
 ** Name : smtp_ssvr_read_release
 ** Desc : Release and reset read buffer.
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.05.14 #
 ******************************************************************************/
static void smtp_ssvr_read_release(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
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
 **函数名称: smtp_ssvr_recv_cmd_handler
 **功    能: 接收命令
 **输入参数: 
 **     ctx: 上下文信息
 **     ssvr: Send线程上下文
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收命令
 **     2. 处理命令
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static int smtp_ssvr_recv_cmd_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr)
{
    int ret = 0;
    smtp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    ret = usck_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd));
    if (ret < 0)
    {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* 2. 处理命令 */
    ret = smtp_ssvr_cmd_handler(ctx, ssvr, &cmd);
    if (0 != ret)
    {
        log_error(ssvr->log, "Handle command failed");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_cmd_handler
 **功    能: 命令处理
 **输入参数: 
 **     ssvr: Send线程上下文
 **     cmd: 接收到的命令信息
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static int smtp_ssvr_cmd_handler(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, const smtp_cmd_t *cmd)
{
    smtp_ssvr_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case SMTP_CMD_SEND:
        case SMTP_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd))
            {
                return smtp_ssvr_send_all_cmd_handler(ctx,
                        ssvr, (const smtp_cmd_send_t *)&cmd->args);
            }
            return 0;
        }
        default:
        {
            log_error(ssvr->log, "Received unknown command! type:[%d]", cmd->type);
            return 0;
        }
    }
    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_send_all_cmd_handler
 **功    能: 发送数据的请求处理
 **输入参数: 
 **     ssvr: Send线程上下文
 **     args: 命令参数
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.04.26 #
 ******************************************************************************/
static int smtp_ssvr_send_all_cmd_handler(
        smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, const smtp_cmd_send_t *args)
{
    int ret = 0;
    size_t n = 0;
    struct timeval tmout;
    time_t ctm = time(NULL);
    smtp_header_t *head = NULL;
    smtp_ssvr_sck_t *sck = &ssvr->sck;
    smtp_send_snap_t *send = &ssvr->sck.send;


    for (;;)
    {
        if (NULL == send->addr)
        {
            /* 1. 取发送的数据 */
            send->addr = smtp_ssvr_get_msg(ssvr);
            if (NULL != send->addr)
            {
                head = (smtp_header_t *)send->addr;

                send->loc = SMTP_DATA_LOC_SLAB;
                send->total = sizeof(smtp_header_t) + head->body_len;
                send->left = send->total;
            }
            else
            {
                send->data_loc_sendq.dataid = orm_queue_pop(ssvr->sq, (void **)&send->addr);
                if (NULLID == send->data_loc_sendq.dataid)
                {
                    return 0;
                }

                head = (smtp_header_t *)send->addr;

                send->loc = SMTP_DATA_LOC_ORM_QUEUE;
                send->total = sizeof(smtp_header_t) + head->body_len;
                send->left = send->total;
            }
        }

        /* 2. 发送数据 */
        n = Writen(sck->fd, send->addr + send->off, send->left);
        if (n < 0)
        {
            head = (smtp_header_t *)send->addr;

        #if defined(__SMTP_DEBUG__)
            send->fail++;
            fprintf(stderr, "succ:%llu fail:%llu again:%llu\n",
                send->succ, send->fail, send->again);
        #endif /*__SMTP_DEBUG__*/

            log_error(ssvr->log, "\terrmsg:[%d] %s! fd:%d type:%d body:%d left:[%d/%d]",
                errno, strerror(errno), sck->fd,
                head->type, head->body_len, send->left, send->total);

            Close(sck->fd);

            smtp_ssvr_release_send_data(ssvr, send);
            smtp_reset_send_snap(send);
            return -1;
        }
        /* 只发送了部分数据 */
        else if (n != send->left)
        {
            sck->wr_tm = ctm;

            send->left -= n;
            send->off += n;
            
        #if defined(__SMTP_DEBUG__)
            send->again++;
        #endif /*__SMTP_DEBUG__*/
            return 0;
        }

        send->left -= n;
        send->off += n;

        LogDebug("\tfd:%d n:%d left:[%d/%d]", sck->fd, n, send->left, send->total);

        sck->wr_tm = ctm;

        /* 4. 释放空间 */
        smtp_ssvr_release_send_data(ssvr, send);

    #if defined(__SMTP_DEBUG__)
        send->succ++;
    #endif /*__SMTP_DEBUG__*/

        smtp_reset_send_snap(send);
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtp_ssvr_add_msg
 **功    能: 添加发送数据
 **输入参数: 
 **    ssvr: Recv对象
 **    addr: 将要发送的数据
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **    将要发送的数据放在链表的末尾
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smtp_ssvr_add_msg(smtp_ssvr_t *ssvr, void *addr)
{
    list_t *add, *item, *tail = NULL;

    /* 1.创建新结点 */
    add = slab_alloc(&ssvr->pool, sizeof(list_t));
    if (NULL == add)
    {
        LogDebug("Alloc memory failed!");
        return SMTP_ERR;
    }

    add->data = addr;
    add->next = NULL;

    /* 2.插入链尾 */
    item = ssvr->sck.message_list;
    if (NULL == item)
    {
        ssvr->sck.message_list = add;

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
 **函数名称: smtp_ssvr_get_msg
 **功    能: 获取发送数据
 **输入参数: 
 **    ssvr: Recv对象
 **    sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static void *smtp_ssvr_get_msg(smtp_ssvr_t *ssvr)
{
    void *addr;
    list_t *curr = ssvr->sck.message_list;

    if (NULL == curr)
    {
        return NULL;
    }
    
    ssvr->sck.message_list = curr->next;
    addr = curr->data;

    slab_dealloc(&ssvr->pool, curr);

    return addr;
}

/******************************************************************************
 **函数名称: smtp_ssvr_clear_msg
 **功    能: 清空发送数据
 **输入参数: 
 **    ssvr: Send对象
 **    sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.04 #
 ******************************************************************************/
static int smtp_ssvr_clear_msg(smtp_ssvr_t *ssvr)
{
    list_t *curr, *next;

    curr = ssvr->sck.message_list; 
    while (NULL != curr)
    {
        next = curr->next;

        slab_dealloc(&ssvr->pool, curr->data);
        slab_dealloc(&ssvr->pool, curr);

        curr = next;
    }

    ssvr->sck.message_list = NULL;
    return SMTP_OK;
}

/******************************************************************************
 ** Name : smtp_ssvr_sys_data_proc
 ** Desc : Handle system data
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.14 #
 ******************************************************************************/
static int smtp_ssvr_sys_data_proc(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    smtp_read_snap_t *read = &sck->read;
    smtp_header_t *head = (smtp_header_t *)read->addr;

    switch (head->type)
    {
        case SMTP_KPALIVE_REP:  /* 保活应答数据 */
        {
            LogDebug("Received keepalive respond!");

            smtp_set_kpalive_stat(sck, SMTP_KPALIVE_SUCC);
            return 0;
        }
        default:
        {
            log_error(ssvr->log, "Give up handle this type [%d]!", head->type);
            return SMTP_HDL_DISCARD;
        }
    }
    
    return SMTP_HDL_DISCARD;
}

/******************************************************************************
 ** Name : smtp_ssvr_exp_data_proc
 ** Desc : Forward expand data
 ** Input: 
 **     ctx: 上下文
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 **     1. Send "Work REQ" command to worker thread
 **     2. Send fail commands again
 ** Note : 
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smtp_ssvr_exp_data_proc(smtp_ssvr_ctx_t *ctx, smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    int ret;
    smtp_read_snap_t *read = &sck->read;

    /* 1. 是否在NULL空间: 直接丢弃 */
    if (read->addr == sck->null)
    {
        log_error(ssvr->log, "Lost data! tidx:[%d] fd:[%d]", ssvr->tidx, sck->fd);
        return SMTP_OK;
    }

    return SMTP_OK;
}

/******************************************************************************
 **函数名称: smtp_ssvr_reset_sck
 **功    能: 重置套接字信息
 **输入参数: 
 **     ssvr: Send对象
 **     sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.08 #
 ******************************************************************************/
static void smtp_ssvr_reset_sck(smtp_ssvr_t *ssvr, smtp_ssvr_sck_t *sck)
{
    smtp_send_snap_t *send = &sck->send;

    /* 1. 重置标识量 */
    Close(sck->fd);
    sck->wr_tm = 0;
    sck->rd_tm = 0;

    smtp_set_kpalive_stat(sck, SMTP_KPALIVE_STAT_UNKNOWN);

    /* 2. 释放空间 */
    smtp_ssvr_clear_msg(ssvr);
}

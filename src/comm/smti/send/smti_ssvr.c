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

#include "smti_cmd.h"
#include "smti_cli.h"
#include "smti_ssvr.h"

#define SMTI_NULL_MAX_LEN        (10*1024)   /* NULL空间最大长度 */

/* 释放发送的数据 */
#define smti_ssvr_release_send_data(ssvr, send) \
{ \
    switch (send->loc) \
    { \
        case SMTI_DATA_LOC_ORM_QUEUE: \
        { \
            orm_queue_data_free(ssvr->sq, send->data_loc_sendq.dataid); \
            break; \
        } \
        case SMTI_DATA_LOC_SLAB: \
        { \
            slab_dealloc(&ssvr->pool, send->addr); \
            send->addr = NULL; \
            break; \
        } \
    } \
}

/* 静态函数 */
static void *smti_ssvr_routine(void *args);

static int smti_ssvr_init(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr);
static smti_ssvr_t *smti_get_curr_ssvr(smti_ssvr_ctx_t *ctx);

static int smti_ssvr_creat_sendq(smti_ssvr_t *ssvr, const smti_ssvr_conf_t *conf);
static int smti_ssvr_creat_usck(smti_ssvr_t *ssvr, const smti_ssvr_conf_t *conf);
static int smti_ssvr_connect(smti_ssvr_t *ssvr, const smti_ssvr_conf_t *conf);

static int smti_ssvr_kpalive_req(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr);
static int smti_ssvr_link_info_report(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr);

static int smti_ssvr_recv_cmd_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr);
static int smti_ssvr_recv_data_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr);

static void smti_ssvr_read_release(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);
static int smti_ssvr_read_init(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);
static int smti_ssvr_read_header(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);
static int smti_ssvr_read_body(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);

static int smti_ssvr_proc_data(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);
static int smti_ssvr_sys_data_proc(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);
static int smti_ssvr_exp_data_proc(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);

static int smti_ssvr_timeout_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr);
static int smti_ssvr_cmd_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, const smti_cmd_t *cmd);
static int smti_ssvr_send_all_cmd_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, const smti_cmd_send_t *args);

static int smti_ssvr_add_msg(smti_ssvr_t *ssvr, void *addr);
static void *smti_ssvr_get_msg(smti_ssvr_t *ssvr);
static int smti_ssvr_clear_msg(smti_ssvr_t *ssvr);

static void smti_ssvr_reset_sck(smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck);

/* 链路状态检测 */
#define smti_ssvr_conn_check(ssvr) (ssvr->sck.fd >= 0)? 1 : 0 /* 0:ERR 1:OK */

/******************************************************************************
 **函数名称: smti_ssvr_creat_sendtp
 **功    能: 创建SND线程池
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
int smti_ssvr_creat_sendtp(smti_ssvr_ctx_t *ctx)
{
    int ret = 0, idx = 0;
    smti_ssvr_conf_t *conf = &ctx->conf;
    smti_ssvr_t *ssvr = NULL;

    /* 1. 创建SND线程池 */
    ret = thread_pool_init(&ctx->sendtp, conf->snd_thd_num);
    if (0 != ret)
    {
        thread_pool_destroy(ctx->sendtp);
        ctx->sendtp = NULL;
        return SMTI_ERR;
    }

    /* 2. 创建SND线程上下文 */
    ctx->sendtp->data = calloc(conf->snd_thd_num, sizeof(smti_ssvr_t));
    if (NULL == ctx->sendtp->data)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTI_ERR;
    }

    /* 3. 设置SND线程上下文 */
    ssvr = (smti_ssvr_t *)ctx->sendtp->data;
    for (idx=0; idx<conf->snd_thd_num; ++idx, ++ssvr)
    {
        ssvr->tidx = idx;

        ret = smti_ssvr_init(ctx, ssvr);
        if (0 != ret)
        {
            log_error(ssvr->log, "Initialize send thread failed!");
            return SMTI_ERR;
        }
    }

    /* 4. 注册SND线程回调 */
    for (idx=0; idx<conf->snd_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, smti_ssvr_routine, ctx);
    }

    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_sendtp_destroy
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
void smti_ssvr_sendtp_destroy(void *_ctx, void *args)
{
    int idx = 0;
    smti_ssvr_ctx_t *ctx = (smti_ssvr_ctx_t *)ctx;
    smti_ssvr_conf_t *conf = &ctx->conf;
    smti_ssvr_t *ssvr = (smti_ssvr_t *)args;

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
 **函数名称: smti_ssvr_init
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
static int smti_ssvr_init(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    int ret = 0;
    smti_ssvr_conf_t *conf = &ctx->conf;
    smti_send_snap_t *send = &ssvr->sck.send;

    /* 1. 创建发送队列 */
    ret = smti_ssvr_creat_sendq(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SMTI_ERR;
    }

    /* 2. 创建unix套接字 */
    ret = smti_ssvr_creat_usck(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SMTI_ERR;
    }

    /* 3. 创建SLAB内存池 */
    ret = slab_init(&ssvr->pool, SMTI_MEM_POOL_SIZE);
    if (0 != ret)
    {
        log_error(ssvr->log, "Initialize slab mem-pool failed!");
        return SMTI_ERR;
    }

    ssvr->sck.null = slab_alloc(&ssvr->pool, SMTI_NULL_MAX_LEN);
    if (NULL == ssvr->sck.null)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return SMTI_ERR;
    }

    /* 4. 连接接收服务器 */
    ret = smti_ssvr_connect(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connect recv server failed!");
        /* Note: Don't return error! */
    }

    smti_reset_send_snap(send);
    
    smti_ssvr_link_info_report(ctx, ssvr);

    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_creat_sendq
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
static int smti_ssvr_creat_sendq(smti_ssvr_t *ssvr, const smti_ssvr_conf_t *conf)
{
    const smti_queue_conf_t *qcf;
    char qname[FILE_NAME_MAX_LEN];


    /* 1. 创建/连接发送队列 */
    qcf = &conf->send_qcf;
    snprintf(qname, sizeof(qname), "%s-%d", qcf->name, ssvr->tidx);

    ssvr->sq = orm_queue_new(qcf->size, qcf->count, qcf->flag, qname);
    if (NULL == ssvr->sq)
    {
        log_error(ssvr->log, "Create send-queue failed!");
        return SMTI_ERR;
    }

    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_creat_usck
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
static int smti_ssvr_creat_usck(smti_ssvr_t *ssvr, const smti_ssvr_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN] = {0};

    smti_ssvr_usck_path(conf, path, ssvr->tidx);
    
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
 **函数名称: smti_ssvr_connect
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
static int smti_ssvr_connect(smti_ssvr_t *ssvr, const smti_ssvr_conf_t *conf)
{
    int ret, opt = 1;
    struct sockaddr_in svraddr;
    smti_ssvr_sck_t *sck = &ssvr->sck;


    /* 1. 创建套接字 */
    sck->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sck->fd < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTI_ERR;
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
        return SMTI_ERR;
    }

    fd_nonblock(sck->fd);
    
    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_bind_cpu
 **功    能: 绑定CPU
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.06.11 #
 ******************************************************************************/
static void smti_ssvr_bind_cpu(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    int idx = 0, mod = 0;
    cpu_set_t cpuset;
    smti_cpu_conf_t *cpu = &ctx->conf.cpu;

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
 **函数名称: smti_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数: 
 **     ssvr: Send线程上下文
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.22 #
 ******************************************************************************/
void smti_ssvr_set_rwset(smti_ssvr_t *ssvr) 
{ 
    smti_header_t *head;
    smti_send_snap_t *send = &ssvr->sck.send;

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
 **函数名称: smti_ssvr_event_handler
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
static int smti_ssvr_event_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    fd_set *wset = NULL;
    int ret = 0, intv = SMTI_RECONN_MIN_INTV;
    struct timeval tmout;
    smti_ssvr_sck_t *sck = sck = &ssvr->sck;
    smti_send_snap_t *send = &ssvr->sck.send;


    for (;;)
    {
        /* 2. 连接合法性判断 */
        if (!smti_ssvr_conn_check(ssvr))
        {
            smti_ssvr_clear_msg(ssvr);

            /* 2.1 重连Recv端 */
            ret = smti_ssvr_connect(ssvr, &ctx->conf);
            if (ret < 0)
            {
                log_error(ssvr->log, "Conncet recv server failed!");

                if (intv >  SMTI_RECONN_MAX_INTV)
                {
                    intv = SMTI_RECONN_MAX_INTV;
                }
                Sleep(intv);
                intv <<= 1;
                continue;
            }

            smti_ssvr_link_info_report(ctx, ssvr);

            intv = SMTI_RECONN_MIN_INTV;
        }

        /* 3. 接收数据&命令 */
        smti_ssvr_set_rwset(ssvr);

        tmout.tv_sec = SMTI_SND_TMOUT_SEC;
        tmout.tv_usec = SMTI_SND_TMOUT_USEC;
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
            smti_ssvr_timeout_handler(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(ssvr->sck.fd, &ssvr->wset))
        {
            smti_ssvr_send_all_cmd_handler(ctx, ssvr, NULL);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            smti_ssvr_recv_cmd_handler(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(ssvr->sck.fd, &ssvr->rset))
        {
            smti_ssvr_recv_data_handler(ctx, ssvr);
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: smti_ssvr_routine
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
static void *smti_ssvr_routine(void *args)
{
    smti_ssvr_t *ssvr = NULL;
    smti_ssvr_ctx_t *ctx = (smti_ssvr_ctx_t *)args;

    LogDebug("Send-thread is running!");

    /* 1. 获取发送线程 */
    ssvr = smti_get_curr_ssvr(ctx);
    if (NULL == ssvr)
    {
        log_error(ssvr->log, "Init send thread failed!");
        pthread_exit(NULL);
        return (void *)-1;
    }

    /* 2. 绑定指定CPU */
    smti_ssvr_bind_cpu(ctx, ssvr);

    smti_ssvr_event_handler(ctx, ssvr);

    pthread_exit(NULL);
    return (void *)-1;
}

/******************************************************************************
 **函数名称: smti_ssvr_kpalive_req
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
static int smti_ssvr_kpalive_req(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    void *addr;
    int ret = 0;
    smti_ssvr_sck_t *sck = &ssvr->sck;
    smti_send_snap_t *send = &ssvr->sck.send;
    smti_header_t *head;
    int size = sizeof(smti_header_t);

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0) 
        || (SMTI_KPALIVE_SENT == sck->kpalive)) 
    {
        Close(sck->fd);

        smti_ssvr_release_send_data(ssvr, send);
        smti_reset_send_snap(send);

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
    head = (smti_header_t *)addr;

    head->type = SMTI_KPALIVE_REQ;
    head->body_len = 0;
    head->flag = SMTI_SYS_DATA;
    head->mark = SMTI_MSG_MARK_KEY;

    /* 3. 加入发送列表 */
    smti_ssvr_add_msg(ssvr, addr);

    LogDebug("Add keepalive request success! fd:[%d]", sck->fd);

    smti_set_kpalive_stat(sck, SMTI_KPALIVE_SENT);
    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_link_info_report
 **功    能: 发送链路信息报告
 **输入参数: 
 **     ssvr: Snd线程上下文
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.08.14 #
 ******************************************************************************/
static int smti_ssvr_link_info_report(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    void *addr;
    int ret = 0;
    smti_ssvr_sck_t *sck = &ssvr->sck;
    smti_send_snap_t *send = &ssvr->sck.send;
    smti_header_t *head;
    smti_link_info_report_t *info;
    int size = sizeof(smti_header_t) + sizeof(smti_link_info_report_t);

    /* 1. 申请空间 */
    addr = slab_alloc(&ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return 0;
    }

    /* 2. 设置心跳数据 */
    head = (smti_header_t *)addr;

    head->type = SMTI_LINK_INFO_REPORT;
    head->body_len = sizeof(smti_link_info_report_t);
    head->flag = SMTI_SYS_DATA;
    head->mark = SMTI_MSG_MARK_KEY;

    info = addr + sizeof(smti_header_t);

    info->is_primary = (0 == ssvr->tidx)? 1 : 0;

    /* 3. 加入发送列表 */
    smti_ssvr_add_msg(ssvr, addr);

    LogDebug("Add link info report! fd:[%d]", sck->fd);
    return 0;
}

/******************************************************************************
 **函数名称: smti_get_curr_ssvr
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
static smti_ssvr_t *smti_get_curr_ssvr(smti_ssvr_ctx_t *ctx)
{
    int tidx = -1;
    smti_ssvr_t *ssvr = NULL;

    /* 1. 获取线程索引 */
    tidx = thread_pool_get_tidx(ctx->sendtp);
    if (tidx < 0)
    {
        log_error(ssvr->log, "Get index of current thread failed!");
        return NULL;
    }

    /* 2. 获取SND线程上下文 */
    ssvr = (smti_ssvr_t *)(ctx->sendtp->data + tidx * sizeof(smti_ssvr_t));
    ssvr->tidx = tidx;

    return ssvr;
}

/******************************************************************************
 **函数名称: smti_ssvr_timeout_handler
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
static int smti_ssvr_timeout_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    int ret = 0;
    time_t curr_tm = time(NULL);
    smti_ssvr_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((NULL != ssvr->sck.send.addr)
        || (curr_tm - sck->wr_tm) < SMTI_SCK_KPALIVE_SEC)
    {
        return 0;
    }

    /* 2. 发送保活请求 */
    ret = smti_ssvr_kpalive_req(ctx, ssvr);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return -1;
    }

    sck->wr_tm = curr_tm;

    return 0;
}

/******************************************************************************
 ** Name : smti_ssvr_check_header
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
static int smti_ssvr_check_header(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;

    /* 1. 检查校验值 */
    if (SMTI_MSG_MARK_KEY != head->mark)
    {
        log_error(ssvr->log, "Mark [%u/%u] isn't right! type:%d len:%d flag:%d",
            head->mark, SMTI_MSG_MARK_KEY, head->type, head->body_len, head->flag);
        return SMTI_ERR;
    }

    /* 2. 检查类型 */
    if (!smti_is_type_valid(head->type))
    {
        log_error(ssvr->log, "Data type is invalid! type:%d len:%d", head->type, head->body_len);
        return SMTI_ERR;
    }
 
    /* 3. 检查长度: 因所有队列长度一致 因此使用[0]判断 */
    if (head->body_len + sizeof(smti_header_t) >= SMTI_NULL_MAX_LEN)
    {
        log_error(ssvr->log, "Length is too long! type:%d len:%d", head->type, head->body_len);
        return SMTI_ERR;
    }

    return SMTI_OK;
}



/******************************************************************************
 ** Name : smti_ssvr_read_init
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
static int smti_ssvr_read_init(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    int times = 0;
    smti_read_snap_t *read = &sck->read;

    /* 指向NULL空间 */
    read->rqidx = 0;
    read->dataid = 0;
    read->addr = sck->null;

    /* 3. 设置标识量 */
    read->offset = 0;
    smti_set_read_phase(read, SMTI_PHASE_READ_HEAD);
    
    return SMTI_OK;
}

/******************************************************************************
 ** Name : smti_ssvr_read_header
 ** Desc : Recv head
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     1. incomplete - SMTI_AGAIN
 **     2. done  - SMTI_DONE
 **     3. close - SMTI_SCK_CLOSED
 **     4. failed - SMTI_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but recv again at next time.
 ** Author: # Qifeng.zou # 2014.08.10 #
 ******************************************************************************/
static int smti_ssvr_read_header(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    int n = 0, ret = 0, left = 0;
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;

    /* 1. Compute left characters */
    left = sizeof(smti_header_t) - read->offset;

    /* 2. Recv head */
    n = Readn(sck->fd, read->addr + read->offset,  left);
    if (n != left)
    {
        if (n > 0)
        {
            read->offset += n;
            return SMTI_AGAIN;
        }
        else if (0 == n)
        {
            log_error(ssvr->log, "Server disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                errno, strerror(errno), sck->fd, n, left);
            return SMTI_SCK_CLOSED;
        }

        log_error(ssvr->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SMTI_ERR;
    }
    
    read->offset += n;

    /* 3. Check head */
    ret = smti_ssvr_check_header(ctx, ssvr, sck);
    if (SMTI_OK != ret)
    {
        log_error(ssvr->log, "Check header failed! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->body_len, head->flag, head->mark, SMTI_MSG_MARK_KEY);
        return SMTI_ERR;
    }

    read->total = sizeof(smti_header_t) + head->body_len;
    smti_set_read_phase(read, SMTI_PHASE_READ_BODY);

    return SMTI_DONE;
}

/******************************************************************************
 ** Name : smti_ssvr_read_body
 ** Desc : Recv body
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return:
 **     1. incomplete - SMTI_AGAIN
 **     2. done  - SMTI_DONE
 **     3. close - SMTI_SCK_CLOSED
 **     4. failed - SMTI_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but ssvr again at next time.
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smti_ssvr_read_body(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    int n = 0, left = 0;
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;

    /* 1. Recv body */
    left = head->body_len + sizeof(smti_header_t) - read->offset;

    n = Readn(sck->fd, read->addr + read->offset, left);
    if (n != left)
    {
        if (n > 0)
        {
            read->offset += n;
            return SMTI_AGAIN;
        }
        else if (0 == n)
        {
            log_error(ssvr->log, "Client disconnected. errmsg:[%d] %s! "
                "fd:[%d] type:[%d] flag:[%d] bodylen:[%d] total:[%d] left:[%d] offset:[%d]",
                errno, strerror(errno),
                sck->fd, head->type, head->flag, head->body_len, read->total, left, read->offset);
            return SMTI_SCK_CLOSED;
        }

        log_error(ssvr->log, "errmsg:[%d] %s! type:%d len:%d n:%d fd:%d total:%d offset:%d addr:%p",
            errno, strerror(errno), head->type,
            head->body_len, n, sck->fd, read->total, read->offset, read->addr);

        return SMTI_ERR;
    }

    /* 2. Set flag variables */
    read->offset += n;
    smti_set_read_phase(read, SMTI_PHASE_READ_POST);

    LogTrace("Recv success! type:%d len:%d", head->type, head->body_len);
    
    return SMTI_DONE;
}

/******************************************************************************
 ** Name : smti_ssvr_proc_data
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
static int smti_ssvr_proc_data(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = read->addr;

    /* 1. Handle system data */
    if (SMTI_SYS_DATA == head->flag)
    {
        return smti_ssvr_sys_data_proc(ctx, ssvr, sck);
    }

    /* 2. Forward expand data */
    return smti_ssvr_exp_data_proc(ctx, ssvr, sck);
}

/******************************************************************************
 **函数名称: smti_ssvr_recv_data_handler
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
static int smti_ssvr_recv_data_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    int ret = 0;
    smti_ssvr_sck_t *sck = &ssvr->sck;
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head;

    sck->rd_tm = time(NULL);

    switch (read->phase)
    {
        case SMTI_PHASE_READ_INIT: /* 1. 初始化读取消息 */
        {
            smti_ssvr_read_init(ctx, ssvr, sck);
            /* NOTE: Don't break - Continue handle */
        }
        case SMTI_PHASE_READ_HEAD: /* 2. 读取消息HEAD */
        {
            ret = smti_ssvr_read_header(ctx, ssvr, sck);
            if (SMTI_DONE == ret)
            {
                head = (smti_header_t *)read->addr;
                if (head->body_len > 0)
                {
                    return SMTI_OK;
                }
                goto READ_POST_STEP;
            }
            else if (SMTI_AGAIN == ret)  /* incomplete */
            {
                /* Note: Continue ssvr head at next loop */
                return SMTI_OK;
            }
            else if (SMTI_SCK_CLOSED == ret)
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
        case SMTI_PHASE_READ_BODY: /* 3. 读取消息BODY */
        {
            ret = smti_ssvr_read_body(ctx, ssvr, sck);
            if (SMTI_DONE == ret)
            {
                goto READ_POST_STEP;
            }
            else if (SMTI_AGAIN == ret)
            {
                /* Note: Continue ssvr body at next loop */
                return SMTI_OK;
            }
            else if (SMTI_HDL_DISCARD == ret)
            {
                smti_ssvr_read_release(ctx, ssvr, sck);
                return SMTI_OK;
            }
            else if (SMTI_SCK_CLOSED == ret)
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
        case SMTI_PHASE_READ_POST:
        {
        READ_POST_STEP:
            ret = smti_ssvr_proc_data(ctx, ssvr, sck);
            if (SMTI_OK == ret)
            {
                smti_reset_read_snap(read);
                return SMTI_OK;
            }
            else if ((SMTI_HDL_DONE == ret)
                || (SMTI_HDL_DISCARD == ret))
            {
                smti_ssvr_read_release(ctx, ssvr, sck);
                return SMTI_OK;
            }

            break;
        }
        default:
        {
            log_error(ssvr->log, "Unknown step!");
            break;
        }
    }

    smti_ssvr_read_release(ctx, ssvr, sck);
    smti_ssvr_reset_sck(ssvr, sck);
    return SMTI_ERR;
}

/******************************************************************************
 ** Name : smti_ssvr_read_release
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
static void smti_ssvr_read_release(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;

    /* 1. 释放内存 */
    if ((read->dataid >= 0)
        && (read->addr != sck->null))
    {
        orm_queue_data_free(ctx->recvq[read->rqidx], read->dataid);
    }

    /* 2. 重置标识量 */
    smti_reset_read_snap(read);
}

/******************************************************************************
 **函数名称: smti_ssvr_recv_cmd_handler
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
static int smti_ssvr_recv_cmd_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr)
{
    int ret = 0;
    smti_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    ret = usck_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd));
    if (ret < 0)
    {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* 2. 处理命令 */
    ret = smti_ssvr_cmd_handler(ctx, ssvr, &cmd);
    if (0 != ret)
    {
        log_error(ssvr->log, "Handle command failed");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_cmd_handler
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
static int smti_ssvr_cmd_handler(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, const smti_cmd_t *cmd)
{
    smti_ssvr_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case SMTI_CMD_SEND:
        case SMTI_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd))
            {
                return smti_ssvr_send_all_cmd_handler(ctx,
                        ssvr, (const smti_cmd_send_t *)&cmd->args);
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
 **函数名称: smti_ssvr_send_all_cmd_handler
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
static int smti_ssvr_send_all_cmd_handler(
        smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, const smti_cmd_send_t *args)
{
    int ret = 0;
    size_t n = 0;
    struct timeval tmout;
    time_t ctm = time(NULL);
    smti_header_t *head = NULL;
    smti_ssvr_sck_t *sck = &ssvr->sck;
    smti_send_snap_t *send = &ssvr->sck.send;


    for (;;)
    {
        if (NULL == send->addr)
        {
            /* 1. 取发送的数据 */
            send->addr = smti_ssvr_get_msg(ssvr);
            if (NULL != send->addr)
            {
                head = (smti_header_t *)send->addr;

                send->loc = SMTI_DATA_LOC_SLAB;
                send->total = sizeof(smti_header_t) + head->body_len;
                send->left = send->total;
            }
            else
            {
                send->data_loc_sendq.dataid = orm_queue_pop(ssvr->sq, (void **)&send->addr);
                if (NULLID == send->data_loc_sendq.dataid)
                {
                    return 0;
                }

                head = (smti_header_t *)send->addr;

                send->loc = SMTI_DATA_LOC_ORM_QUEUE;
                send->total = sizeof(smti_header_t) + head->body_len;
                send->left = send->total;
            }
        }

        /* 2. 发送数据 */
        n = Writen(sck->fd, send->addr + send->off, send->left);
        if (n < 0)
        {
            head = (smti_header_t *)send->addr;

        #if defined(__SMTI_DEBUG__)
            send->fail++;
            fprintf(stderr, "succ:%llu fail:%llu again:%llu\n",
                send->succ, send->fail, send->again);
        #endif /*__SMTI_DEBUG__*/

            log_error(ssvr->log, "\terrmsg:[%d] %s! fd:%d type:%d body:%d left:[%d/%d]",
                errno, strerror(errno), sck->fd,
                head->type, head->body_len, send->left, send->total);

            Close(sck->fd);

            smti_ssvr_release_send_data(ssvr, send);
            smti_reset_send_snap(send);
            return -1;
        }
        /* 只发送了部分数据 */
        else if (n != send->left)
        {
            sck->wr_tm = ctm;

            send->left -= n;
            send->off += n;
            
        #if defined(__SMTI_DEBUG__)
            send->again++;
        #endif /*__SMTI_DEBUG__*/
            return 0;
        }

        send->left -= n;
        send->off += n;

        LogDebug("\tfd:%d n:%d left:[%d/%d]", sck->fd, n, send->left, send->total);

        sck->wr_tm = ctm;

        /* 4. 释放空间 */
        smti_ssvr_release_send_data(ssvr, send);

    #if defined(__SMTI_DEBUG__)
        send->succ++;
    #endif /*__SMTI_DEBUG__*/

        smti_reset_send_snap(send);
    }

    return 0;
}

/******************************************************************************
 **函数名称: smti_ssvr_add_msg
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
static int smti_ssvr_add_msg(smti_ssvr_t *ssvr, void *addr)
{
    list_t *add, *item, *tail = NULL;

    /* 1.创建新结点 */
    add = slab_alloc(&ssvr->pool, sizeof(list_t));
    if (NULL == add)
    {
        LogDebug("Alloc memory failed!");
        return SMTI_ERR;
    }

    add->data = addr;
    add->next = NULL;

    /* 2.插入链尾 */
    item = ssvr->sck.message_list;
    if (NULL == item)
    {
        ssvr->sck.message_list = add;

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
 **函数名称: smti_ssvr_get_msg
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
static void *smti_ssvr_get_msg(smti_ssvr_t *ssvr)
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
 **函数名称: smti_ssvr_clear_msg
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
static int smti_ssvr_clear_msg(smti_ssvr_t *ssvr)
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
    return SMTI_OK;
}

/******************************************************************************
 ** Name : smti_ssvr_sys_data_proc
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
static int smti_ssvr_sys_data_proc(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    smti_read_snap_t *read = &sck->read;
    smti_header_t *head = (smti_header_t *)read->addr;

    switch (head->type)
    {
        case SMTI_KPALIVE_REP:  /* 保活应答数据 */
        {
            LogDebug("Received keepalive respond!");

            smti_set_kpalive_stat(sck, SMTI_KPALIVE_SUCC);
            return 0;
        }
        default:
        {
            log_error(ssvr->log, "Give up handle this type [%d]!", head->type);
            return SMTI_HDL_DISCARD;
        }
    }
    
    return SMTI_HDL_DISCARD;
}

/******************************************************************************
 ** Name : smti_ssvr_exp_data_proc
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
static int smti_ssvr_exp_data_proc(smti_ssvr_ctx_t *ctx, smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    int ret;
    smti_read_snap_t *read = &sck->read;

    /* 1. 是否在NULL空间: 直接丢弃 */
    if (read->addr == sck->null)
    {
        log_error(ssvr->log, "Lost data! tidx:[%d] fd:[%d]", ssvr->tidx, sck->fd);
        return SMTI_OK;
    }

    return SMTI_OK;
}

/******************************************************************************
 **函数名称: smti_ssvr_reset_sck
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
static void smti_ssvr_reset_sck(smti_ssvr_t *ssvr, smti_ssvr_sck_t *sck)
{
    smti_send_snap_t *send = &sck->send;

    /* 1. 重置标识量 */
    Close(sck->fd);
    sck->wr_tm = 0;
    sck->rd_tm = 0;

    smti_set_kpalive_stat(sck, SMTI_KPALIVE_STAT_UNKNOWN);

    /* 2. 释放空间 */
    smti_ssvr_clear_msg(ssvr);
}

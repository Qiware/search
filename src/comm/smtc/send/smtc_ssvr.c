#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "syscall.h"
#include "smtc_cmd.h"
#include "smtc_cli.h"
#include "smtc_ssvr.h"

#define SMTC_NULL_MAX_LEN        (10*1024)   /* NULL空间最大长度 */

/* 释放发送的数据 */
#if 0
#define smtc_ssvr_release_send_data(ssvr, send) \
{ \
    switch (send->loc) \
    { \
        case SMTC_DATA_LOC_SHMQ: \
        { \
            shm_queue_dealloc(ssvr->sq, send->data_loc_sendq.dataid); \
            break; \
        } \
        case SMTC_DATA_LOC_SLAB: \
        { \
            slab_dealloc(ssvr->pool, send->addr); \
            send->addr = NULL; \
            break; \
        } \
    } \
}
#else
#define smtc_ssvr_release_send_data(ssvr, send)
#endif

/* 静态函数 */
static void *smtc_ssvr_routine(void *args);

static int smtc_ssvr_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static smtc_ssvr_t *smtc_get_curr_ssvr(smtc_ssvr_cntx_t *ctx);

static int smtc_ssvr_creat_sendq(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);
static int smtc_ssvr_creat_usck(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);
static int smtc_ssvr_connect(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);

static int smtc_ssvr_kpalive_req(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);

static int smtc_ssvr_recv_cmd_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static int smtc_ssvr_recv_data_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);

static void smtc_ssvr_recv_release(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_recv_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_recv_header(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_recv_body(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);

static int smtc_ssvr_proc_data(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_sys_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_exp_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);

static int smtc_ssvr_timeout_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static int smtc_ssvr_cmd_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_t *cmd);
static int smtc_ssvr_cmd_send_all_req_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_send_req_t *args);

static int smtc_ssvr_add_mesg(smtc_ssvr_t *ssvr, void *addr);
static void *smtc_ssvr_fetch_mesg(smtc_ssvr_t *ssvr);
static int smtc_ssvr_clear_msg(smtc_ssvr_t *ssvr);

static void smtc_ssvr_reset_sck(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);

/* 链路状态检测 */
#define smtc_ssvr_conn_check(ssvr) (ssvr->sck.fd >= 0)? 1 : 0 /* 0:ERR 1:OK */

/******************************************************************************
 **函数名称: smtc_ssvr_creat_sendtp
 **功    能: 创建SND线程池
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
int smtc_ssvr_creat_sendtp(smtc_ssvr_cntx_t *ctx)
{
    int ret, idx;
    smtc_ssvr_t *ssvr;
    smtc_ssvr_conf_t *conf = &ctx->conf;

    /* 1. 创建SND线程池 */
    ctx->sendtp = thread_pool_init(conf->snd_thd_num, 4 * KB);
    if (NULL == ctx->sendtp)
    {
        thread_pool_destroy(ctx->sendtp);
        ctx->sendtp = NULL;
        return SMTC_ERR;
    }

    /* 2. 创建SND线程上下文 */
    ctx->sendtp->data = calloc(conf->snd_thd_num, sizeof(smtc_ssvr_t));
    if (NULL == ctx->sendtp->data)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    /* 3. 设置SND线程上下文 */
    ssvr = (smtc_ssvr_t *)ctx->sendtp->data;
    for (idx=0; idx<conf->snd_thd_num; ++idx, ++ssvr)
    {
        ssvr->tidx = idx;

        ret = smtc_ssvr_init(ctx, ssvr);
        if (0 != ret)
        {
            log_error(ssvr->log, "Initialize send thread failed!");
            return SMTC_ERR;
        }
    }

    /* 4. 注册SND线程回调 */
    for (idx=0; idx<conf->snd_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, smtc_ssvr_routine, ctx);
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtc_ssvr_sendtp_destroy
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
void smtc_ssvr_sendtp_destroy(void *_ctx, void *args)
{
    int idx = 0;
    smtc_ssvr_cntx_t *ctx = (smtc_ssvr_cntx_t *)ctx;
    smtc_ssvr_conf_t *conf = &ctx->conf;
    smtc_ssvr_t *ssvr = (smtc_ssvr_t *)args;

    /* 3. 设置SND线程上下文 */
    for (idx=0; idx<conf->snd_thd_num; ++idx, ++ssvr)
    {
        Close(ssvr->cmd_sck_id);
        Close(ssvr->sck.fd);
        // TODO: 删除共享内存 或　程序退出时　自动删除共享内存
        //shm_queue_destory(ssvr->sq);
    }

    free(args);

    return;
}

/******************************************************************************
 **函数名称: smtc_ssvr_init
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
static int smtc_ssvr_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int ret;
    void *addr;
    smtc_ssvr_conf_t *conf = &ctx->conf;
    socket_snap_t *send = &ssvr->sck.send;

    /* 1. 创建发送队列 */
    if (smtc_ssvr_creat_sendq(ssvr, conf))
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SMTC_ERR;
    }

    /* 2. 创建unix套接字 */
    if (smtc_ssvr_creat_usck(ssvr, conf))
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SMTC_ERR;
    }

    /* 3. 创建SLAB内存池 */
    addr = calloc(1, SMTC_MEM_POOL_SIZE);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    ssvr->pool = slab_init(addr, SMTC_MEM_POOL_SIZE);
    if (NULL == ssvr->pool)
    {
        log_error(ssvr->log, "Initialize slab mem-pool failed!");
        return SMTC_ERR;
    }

    ssvr->sck.null = slab_alloc(ssvr->pool, SMTC_NULL_MAX_LEN);
    if (NULL == ssvr->sck.null)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return SMTC_ERR;
    }

    /* 4. 连接接收服务器 */
    ret = smtc_ssvr_connect(ssvr, conf);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connect recv server failed!");
        /* Note: Don't return error! */
    }

    smtc_reset_send_snap(send);

    return 0;
}

/******************************************************************************
 **函数名称: smtc_ssvr_creat_sendq
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
static int smtc_ssvr_creat_sendq(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf)
{
    key_t key;
    char path[FILE_NAME_MAX_LEN];
    const smtc_queue_conf_t *qcf = &conf->send_qcf;


    /* 1. 创建/连接发送队列 */
    snprintf(path, sizeof(path), "%s-%d", qcf->name, ssvr->tidx);

    key = ftok(path, 0);

    ssvr->sq = shm_queue_creat(key, qcf->size, qcf->count);
    if (NULL == ssvr->sq)
    {
        log_error(ssvr->log, "Create send-queue failed!");
        return SMTC_ERR;
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtc_ssvr_creat_usck
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
static int smtc_ssvr_creat_usck(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN] = {0};

    smtc_ssvr_usck_path(conf, path, ssvr->tidx);
    
    ssvr->cmd_sck_id = unix_udp_creat(path);
    if (ssvr->cmd_sck_id < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return -1;
    }

    log_trace(ssvr->log, "cmd_sck_id:[%d] path:%s", ssvr->cmd_sck_id, path);
    return 0;
}

/******************************************************************************
 **函数名称: smtc_ssvr_connect
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
static int smtc_ssvr_connect(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf)
{
    int ret, opt = 1;
    struct sockaddr_in svraddr;
    smtc_ssvr_sck_t *sck = &ssvr->sck;


    /* 1. 创建套接字 */
    sck->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sck->fd < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
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
        return SMTC_ERR;
    }

    fd_set_nonblocking(sck->fd);
    
    return 0;
}

/******************************************************************************
 **函数名称: smtc_ssvr_bind_cpu
 **功    能: 绑定CPU
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.06.11 #
 ******************************************************************************/
static void smtc_ssvr_bind_cpu(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int idx = 0, mod = 0;
    cpu_set_t cpuset;
    smtc_cpu_conf_t *cpu = &ctx->conf.cpu;

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
 **函数名称: smtc_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数: 
 **     ssvr: Send线程上下文
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.07.22 #
 ******************************************************************************/
void smtc_ssvr_set_rwset(smtc_ssvr_t *ssvr) 
{ 
    socket_snap_t *send = &ssvr->sck.send;

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
        || NULL != ssvr->sck.mesg_list
        || 0 != shm_queue_data_count(ssvr->sq))
    {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
    }

    return;
}

/******************************************************************************
 **函数名称: smtc_ssvr_event_handler
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
static int smtc_ssvr_event_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int ret, intv = SMTC_RECONN_MIN_INTV;
    struct timeval tmout;
    smtc_ssvr_sck_t *sck = sck = &ssvr->sck;

    for (;;)
    {
        /* 2. 连接合法性判断 */
        if (!smtc_ssvr_conn_check(ssvr))
        {
            smtc_ssvr_clear_msg(ssvr);

            /* 2.1 重连Recv端 */
            if (smtc_ssvr_connect(ssvr, &ctx->conf) < 0)
            {
                log_error(ssvr->log, "Conncet recv server failed!");

                if (intv >  SMTC_RECONN_MAX_INTV)
                {
                    intv = SMTC_RECONN_MAX_INTV;
                }
                Sleep(intv);
                intv <<= 1;
                continue;
            }

            intv = SMTC_RECONN_MIN_INTV;
        }

        /* 3. 接收数据&命令 */
        smtc_ssvr_set_rwset(ssvr);

        tmout.tv_sec = SMTC_SND_TMOUT_SEC;
        tmout.tv_usec = SMTC_SND_TMOUT_USEC;
        ret = select(ssvr->max+1, &ssvr->rset, &ssvr->wset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_trace(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
        else if (0 == ret)
        {
            smtc_ssvr_timeout_handler(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(ssvr->sck.fd, &ssvr->wset))
        {
            smtc_ssvr_cmd_send_all_req_hdl(ctx, ssvr, NULL);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            smtc_ssvr_recv_cmd_handler(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(ssvr->sck.fd, &ssvr->rset))
        {
            smtc_ssvr_recv_data_handler(ctx, ssvr);
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: smtc_ssvr_routine
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
static void *smtc_ssvr_routine(void *args)
{
    smtc_ssvr_t *ssvr = NULL;
    smtc_ssvr_cntx_t *ctx = (smtc_ssvr_cntx_t *)args;

    log_debug(ssvr->log, "Send-thread is running!");

    /* 1. 获取发送线程 */
    ssvr = smtc_get_curr_ssvr(ctx);
    if (NULL == ssvr)
    {
        log_error(ssvr->log, "Init send thread failed!");
        pthread_exit(NULL);
        return (void *)-1;
    }

    /* 2. 绑定指定CPU */
    smtc_ssvr_bind_cpu(ctx, ssvr);

    smtc_ssvr_event_handler(ctx, ssvr);

    pthread_exit(NULL);
    return (void *)-1;
}

/******************************************************************************
 **函数名称: smtc_ssvr_kpalive_req
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
static int smtc_ssvr_kpalive_req(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    void *addr;
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    socket_snap_t *send = &ssvr->sck.send;
    smtc_header_t *head;
    int size = sizeof(smtc_header_t);

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0) 
        || (SMTC_KPALIVE_STAT_SENT == sck->kpalive)) 
    {
        Close(sck->fd);

        smtc_ssvr_release_send_data(ssvr, send);
        smtc_reset_send_snap(send);

        log_error(ssvr->log, "Didn't get keep-alive respond for a long time!");
        return 0;
    }

    addr = slab_alloc(ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return 0;
    }

    /* 2. 设置心跳数据 */
    head = (smtc_header_t *)addr;

    head->type = SMTC_KPALIVE_REQ;
    head->length = 0;
    head->flag = SMTC_SYS_MESG;
    head->mark = SMTC_MSG_MARK_KEY;

    /* 3. 加入发送列表 */
    smtc_ssvr_add_mesg(ssvr, addr);

    log_debug(ssvr->log, "Add keepalive request success! fd:[%d]", sck->fd);

    smtc_set_kpalive_stat(sck, SMTC_KPALIVE_STAT_SENT);
    return 0;
}

/******************************************************************************
 **函数名称: smtc_get_curr_ssvr
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
static smtc_ssvr_t *smtc_get_curr_ssvr(smtc_ssvr_cntx_t *ctx)
{
    int tidx = -1;
    smtc_ssvr_t *ssvr = NULL;

    /* 1. 获取线程索引 */
    tidx = thread_pool_get_tidx(ctx->sendtp);
    if (tidx < 0)
    {
        log_error(ssvr->log, "Get index of current thread failed!");
        return NULL;
    }

    /* 2. 获取SND线程上下文 */
    ssvr = (smtc_ssvr_t *)(ctx->sendtp->data + tidx * sizeof(smtc_ssvr_t));
    ssvr->tidx = tidx;

    return ssvr;
}

/******************************************************************************
 **函数名称: smtc_ssvr_timeout_handler
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
static int smtc_ssvr_timeout_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int ret = 0;
    time_t curr_tm = time(NULL);
    smtc_ssvr_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((NULL != ssvr->sck.send.addr)
        || (curr_tm - sck->wr_tm) < SMTC_SCK_KPALIVE_SEC)
    {
        return 0;
    }

    /* 2. 发送保活请求 */
    ret = smtc_ssvr_kpalive_req(ctx, ssvr);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return -1;
    }

    sck->wr_tm = curr_tm;

    return 0;
}

/******************************************************************************
 ** Name : smtc_ssvr_check_header
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
static int smtc_ssvr_check_header(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    /* 1. 检查校验值 */
    if (SMTC_MSG_MARK_KEY != head->mark)
    {
        log_error(ssvr->log, "Mark [%u/%u] isn't right! type:%d len:%d flag:%d",
            head->mark, SMTC_MSG_MARK_KEY, head->type, head->length, head->flag);
        return SMTC_ERR;
    }

    /* 2. 检查类型 */
    if (!smtc_is_type_valid(head->type))
    {
        log_error(ssvr->log, "Data type is invalid! type:%d len:%d", head->type, head->length);
        return SMTC_ERR;
    }
 
    /* 3. 检查长度: 因所有队列长度一致 因此使用[0]判断 */
    if (head->length + sizeof(smtc_header_t) >= SMTC_NULL_MAX_LEN)
    {
        log_error(ssvr->log, "Length is too long! type:%d len:%d", head->type, head->length);
        return SMTC_ERR;
    }

    return SMTC_OK;
}



/******************************************************************************
 ** Name : smtc_ssvr_recv_init
 ** Desc : Prepare ssvr recv data
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
static int smtc_ssvr_recv_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_snap_t *recv = &sck->recv;

    /* 指向NULL空间 */
    recv->addr = sck->null;

    /* 3. 设置标识量 */
    recv->off = 0;
    smtc_set_recv_phase(recv, SOCK_PHASE_RECV_HEAD);
    
    return SMTC_OK;
}

/******************************************************************************
 ** Name : smtc_ssvr_recv_header
 ** Desc : Recv head
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 
 **     1. incomplete - SMTC_AGAIN
 **     2. done  - SMTC_DONE
 **     3. close - SMTC_SCK_CLOSED
 **     4. failed - SMTC_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but recv again at next time.
 ** Author: # Qifeng.zou # 2014.08.10 #
 ******************************************************************************/
static int smtc_ssvr_recv_header(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    int n, left;
    socket_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    /* 1. Compute left characters */
    left = sizeof(smtc_header_t) - recv->off;

    /* 2. Recv head */
    n = Readn(sck->fd, recv->addr + recv->off,  left);
    if (n != left)
    {
        if (n > 0)
        {
            recv->off += n;
            return SMTC_AGAIN;
        }
        else if (0 == n)
        {
            log_error(ssvr->log, "Server disconnected. errmsg:[%d] %s! fd:[%d] n:[%d/%d]",
                errno, strerror(errno), sck->fd, n, left);
            return SMTC_SCK_CLOSED;
        }

        log_error(ssvr->log, "errmsg:[%d] %s. fd:[%d]", errno, strerror(errno), sck->fd);
        return SMTC_ERR;
    }
    
    recv->off += n;

    /* 3. Check head */
    if (smtc_ssvr_check_header(ctx, ssvr, sck))
    {
        log_error(ssvr->log, "Check header failed! type:%d len:%d flag:%d mark:[%u/%u]",
            head->type, head->length, head->flag, head->mark, SMTC_MSG_MARK_KEY);
        return SMTC_ERR;
    }

    recv->total = sizeof(smtc_header_t) + head->length;
    smtc_set_recv_phase(recv, SOCK_PHASE_RECV_BODY);

    return SMTC_DONE;
}

/******************************************************************************
 ** Name : smtc_ssvr_recv_body
 ** Desc : Recv body
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return:
 **     1. incomplete - SMTC_AGAIN
 **     2. done  - SMTC_DONE
 **     3. close - SMTC_SCK_CLOSED
 **     4. failed - SMTC_ERR
 ** Proc : 
 ** Note : 
 **     Don't return error when errno is EAGAIN, but ssvr again at next time.
 ** Author: # Qifeng.zou # 2014.04.10 #
 ******************************************************************************/
static int smtc_ssvr_recv_body(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    int n = 0, left = 0;
    socket_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    /* 1. Recv body */
    left = head->length + sizeof(smtc_header_t) - recv->off;

    n = Readn(sck->fd, recv->addr + recv->off, left);
    if (n != left)
    {
        if (n > 0)
        {
            recv->off += n;
            return SMTC_AGAIN;
        }
        else if (0 == n)
        {
            log_error(ssvr->log, "Client disconnected. errmsg:[%d] %s! "
                "fd:[%d] type:[%d] flag:[%d] bodylen:[%d] total:[%d] left:[%d] off:[%d]",
                errno, strerror(errno),
                sck->fd, head->type, head->flag, head->length, recv->total, left, recv->off);
            return SMTC_SCK_CLOSED;
        }

        log_error(ssvr->log, "errmsg:[%d] %s! type:%d len:%d n:%d fd:%d total:%d off:%d addr:%p",
            errno, strerror(errno), head->type,
            head->length, n, sck->fd, recv->total, recv->off, recv->addr);

        return SMTC_ERR;
    }

    /* 2. Set flag variables */
    recv->off += n;
    smtc_set_recv_phase(recv, SOCK_PHASE_RECV_POST);

    log_trace(ssvr->log, "Recv success! type:%d len:%d", head->type, head->length);
    
    return SMTC_DONE;
}

/******************************************************************************
 ** Name : smtc_ssvr_proc_data
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
static int smtc_ssvr_proc_data(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    /* 1. Handle system data */
    if (SMTC_SYS_MESG == head->flag)
    {
        return smtc_ssvr_sys_data_proc(ctx, ssvr, sck);
    }

    /* 2. Forward expand data */
    return smtc_ssvr_exp_data_proc(ctx, ssvr, sck);
}

/******************************************************************************
 **函数名称: smtc_ssvr_recv_data_handler
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
static int smtc_ssvr_recv_data_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int ret = 0;
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    socket_snap_t *recv = &sck->recv;
    smtc_header_t *head;

    sck->rd_tm = time(NULL);

    switch (recv->phase)
    {
        case SOCK_PHASE_RECV_INIT: /* 1. 初始化读取消息 */
        {
            smtc_ssvr_recv_init(ctx, ssvr, sck);
            /* NOTE: Don't break - Continue handle */
        }
        case SOCK_PHASE_RECV_HEAD: /* 2. 读取消息HEAD */
        {
            ret = smtc_ssvr_recv_header(ctx, ssvr, sck);
            if (SMTC_DONE == ret)
            {
                head = (smtc_header_t *)recv->addr;
                if (head->length > 0)
                {
                    return SMTC_OK;
                }
                goto READ_POST_STEP;
            }
            else if (SMTC_AGAIN == ret)  /* incomplete */
            {
                /* Note: Continue ssvr head at next loop */
                return SMTC_OK;
            }
            else if (SMTC_SCK_CLOSED == ret)
            {
                log_debug(ssvr->log, "Server disconnect!");
                break;
            }
            else
            {
                log_error(ssvr->log, "Recv head failed!");
                break; /* error */
            }
            /* NOTE: Don't break - Continue handle */
        }
        case SOCK_PHASE_RECV_BODY: /* 3. 读取消息BODY */
        {
            ret = smtc_ssvr_recv_body(ctx, ssvr, sck);
            if (SMTC_DONE == ret)
            {
                goto READ_POST_STEP;
            }
            else if (SMTC_AGAIN == ret)
            {
                /* Note: Continue ssvr body at next loop */
                return SMTC_OK;
            }
            else if (SMTC_HDL_DISCARD == ret)
            {
                smtc_ssvr_recv_release(ctx, ssvr, sck);
                return SMTC_OK;
            }
            else if (SMTC_SCK_CLOSED == ret)
            {
                log_debug(ssvr->log, "Client disconnect!");
                break;
            }
            else
            {
                log_error(ssvr->log, "Recv body failed!");
                break; /* error */
            }
            /* NOTE: Don't break - Continue handle */
        }
        case SOCK_PHASE_RECV_POST:
        {
        READ_POST_STEP:
            ret = smtc_ssvr_proc_data(ctx, ssvr, sck);
            if (SMTC_OK == ret)
            {
                smtc_reset_recv_snap(recv);
                return SMTC_OK;
            }
            else if ((SMTC_HDL_DONE == ret)
                || (SMTC_HDL_DISCARD == ret))
            {
                smtc_ssvr_recv_release(ctx, ssvr, sck);
                return SMTC_OK;
            }

            break;
        }
        default:
        {
            log_error(ssvr->log, "Unknown step!");
            break;
        }
    }

    smtc_ssvr_recv_release(ctx, ssvr, sck);
    smtc_ssvr_reset_sck(ssvr, sck);
    return SMTC_ERR;
}

/******************************************************************************
 ** Name : smtc_ssvr_recv_release
 ** Desc : Release and reset recv buffer.
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
static void smtc_ssvr_recv_release(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_snap_t *recv = &sck->recv;

    /* 重置标识量 */
    smtc_reset_recv_snap(recv);
}

/******************************************************************************
 **函数名称: smtc_ssvr_recv_cmd_handler
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
static int smtc_ssvr_recv_cmd_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int ret = 0;
    smtc_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    ret = unix_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd));
    if (ret < 0)
    {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* 2. 处理命令 */
    ret = smtc_ssvr_cmd_handler(ctx, ssvr, &cmd);
    if (0 != ret)
    {
        log_error(ssvr->log, "Handle command failed");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: smtc_ssvr_cmd_handler
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
static int smtc_ssvr_cmd_handler(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_t *cmd)
{
    smtc_ssvr_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case SMTC_CMD_SEND:
        case SMTC_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd))
            {
                return smtc_ssvr_cmd_send_all_req_hdl(ctx,
                        ssvr, (const smtc_cmd_send_req_t *)&cmd->args);
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
 **函数名称: smtc_ssvr_cmd_send_all_req_hdl
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
static int smtc_ssvr_cmd_send_all_req_hdl(
        smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_send_req_t *args)
{
    int n, left;
    time_t ctm = time(NULL);
    smtc_header_t *head;
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    socket_snap_t *send = &sck->send;


    for (;;)
    {
        if (NULL == send->addr)
        {
            /* 1. 取发送的数据 */
            send->addr = smtc_ssvr_fetch_mesg(ssvr);
            if (NULL != send->addr)
            {
                head = (smtc_header_t *)send->addr;

                send->loc = SMTC_DATA_LOC_SLAB;
                send->total = sizeof(smtc_header_t) + head->length;
            }
            else
            {
                send->addr = shm_queue_pop(ssvr->sq);
                if (NULL == send->addr)
                {
                    return SMTC_OK;
                }

                head = (smtc_header_t *)send->addr;

                send->loc = SMTC_DATA_LOC_SHMQ;
                send->total = sizeof(smtc_header_t) + head->length;
            }
        }

        /* 2. 发送数据 */
        left = send->total - send->off;

        n = Writen(sck->fd, send->addr + send->off, left);
        if (n < 0)
        {
            head = (smtc_header_t *)send->addr;

        #if defined(__SMTC_DEBUG__)
            send->fail++;
            fprintf(stderr, "succ:%llu fail:%llu again:%llu\n",
                send->succ, send->fail, send->again);
        #endif /*__SMTC_DEBUG__*/

            log_error(ssvr->log, "\terrmsg:[%d] %s! fd:%d type:%d body:%d left:[%d/%d]",
                errno, strerror(errno), sck->fd,
                head->type, head->length, left, send->total);

            Close(sck->fd);

            smtc_ssvr_release_send_data(ssvr, send);
            smtc_reset_send_snap(send);
            return SMTC_ERR;
        }
        /* 只发送了部分数据 */
        else if (n != left)
        {
            sck->wr_tm = ctm;

            send->off += n;
            
        #if defined(__SMTC_DEBUG__)
            send->again++;
        #endif /*__SMTC_DEBUG__*/
            return SMTC_OK;
        }

        send->off += n;

        log_debug(ssvr->log, "\tfd:%d n:%d left:[%d/%d]", sck->fd, n, left, send->total);

        sck->wr_tm = ctm;

        /* 4. 释放空间 */
        smtc_ssvr_release_send_data(ssvr, send);

    #if defined(__SMTC_DEBUG__)
        send->succ++;
    #endif /*__SMTC_DEBUG__*/

        smtc_reset_send_snap(send);
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_add_mesg
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
static int smtc_ssvr_add_mesg(smtc_ssvr_t *ssvr, void *addr)
{
    list_node_t *add;

    /* 1.创建新结点 */
    add = slab_alloc(ssvr->pool, sizeof(list_node_t));
    if (NULL == add)
    {
        log_debug(ssvr->log, "Alloc memory from slab failed!");
        return SMTC_ERR;
    }

    add->data = addr;
    add->next = NULL;

    /* 2.插入链尾 */
    list_insert_tail(ssvr->sck.mesg_list, add);

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_fetch_mesg
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
static void *smtc_ssvr_fetch_mesg(smtc_ssvr_t *ssvr)
{
    void *addr;
    list_node_t *node;

    node = list_remove_head(ssvr->sck.mesg_list);
    if (NULL == node)
    {
        return NULL;
    }
    
    addr = node->data;

    slab_dealloc(ssvr->pool, node);

    return addr;
}

/******************************************************************************
 **函数名称: smtc_ssvr_clear_msg
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
static int smtc_ssvr_clear_msg(smtc_ssvr_t *ssvr)
{
    list_node_t *node;

    node = list_remove_head(ssvr->sck.mesg_list);
    while (NULL != node)
    {
        slab_dealloc(ssvr->pool, node->data);
        slab_dealloc(ssvr->pool, node);

        node = list_remove_head(ssvr->sck.mesg_list);
    }

    ssvr->sck.mesg_list = NULL;
    return SMTC_OK;
}

/******************************************************************************
 ** Name : smtc_ssvr_sys_data_proc
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
static int smtc_ssvr_sys_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    switch (head->type)
    {
        case SMTC_KPALIVE_REP:  /* 保活应答数据 */
        {
            log_debug(ssvr->log, "Received keepalive respond!");

            smtc_set_kpalive_stat(sck, SMTC_KPALIVE_STAT_SUCC);
            return 0;
        }
        default:
        {
            log_error(ssvr->log, "Give up handle this type [%d]!", head->type);
            return SMTC_HDL_DISCARD;
        }
    }
    
    return SMTC_HDL_DISCARD;
}

/******************************************************************************
 ** Name : smtc_ssvr_exp_data_proc
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
static int smtc_ssvr_exp_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_snap_t *recv = &sck->recv;

    /* 1. 是否在NULL空间: 直接丢弃 */
    if (recv->addr == sck->null)
    {
        log_error(ssvr->log, "Lost data! tidx:[%d] fd:[%d]", ssvr->tidx, sck->fd);
        return SMTC_OK;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_reset_sck
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
static void smtc_ssvr_reset_sck(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    /* 1. 重置标识量 */
    Close(sck->fd);
    sck->wr_tm = 0;
    sck->rd_tm = 0;

    smtc_set_kpalive_stat(sck, SMTC_KPALIVE_STAT_UNKNOWN);

    /* 2. 释放空间 */
    smtc_ssvr_clear_msg(ssvr);
}

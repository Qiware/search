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

/* 静态函数 */
static int smtc_ssvr_creat_sendtp(smtc_ssvr_cntx_t *ctx);
static void *smtc_ssvr_routine(void *_ctx);

static int smtc_ssvr_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, int tidx);
static smtc_ssvr_t *smtc_ssvr_get_curr(smtc_ssvr_cntx_t *ctx);

static int smtc_ssvr_creat_sendq(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);
static int smtc_ssvr_creat_usck(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);
static int smtc_ssvr_connect(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);

static int smtc_ssvr_kpalive_req(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static bool smtc_ssvr_head_isvalid(smtc_ssvr_cntx_t *ctx, smtc_header_t *head);

static int smtc_ssvr_recv_cmd_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static int smtc_ssvr_recv_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);

static int smtc_ssvr_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_sys_mesg_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_exp_mesg_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);

static int smtc_ssvr_timeout_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static int smtc_ssvr_cmd_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_t *cmd);
static int smtc_ssvr_cmd_send_all_req_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_send_req_t *args);

static int smtc_ssvr_add_mesg(smtc_ssvr_t *ssvr, void *addr);
static void *smtc_ssvr_get_mesg(smtc_ssvr_t *ssvr);
static int smtc_ssvr_clear_mesg(smtc_ssvr_t *ssvr);

/* 链路状态检测 */
#define smtc_ssvr_conn_isvalid(ssvr) ((ssvr)->sck.fd >= 0? true : false) /* 0:ERR 1:OK */

/******************************************************************************
 **函数名称: smtc_ssvr_startup
 **功    能: 启动发送端
 **输入参数: 
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建上下文对象
 **     2. 加载配置文件
 **     3. 启动各发送服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
smtc_ssvr_cntx_t *smtc_ssvr_startup(const smtc_ssvr_conf_t *conf)
{
    smtc_ssvr_cntx_t *ctx;

    /* 1. 创建上下文对象 */
    ctx = (smtc_ssvr_cntx_t *)calloc(1, sizeof(smtc_ssvr_cntx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smtc_ssvr_conf_t));

    /* 3. 启动各发送服务 */
    if (!smtc_ssvr_creat_sendtp(ctx))
    {
        printf("Initalize send thread failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: smtc_ssvr_creat_sendtp
 **功    能: 创建发送线程池
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_creat_sendtp(smtc_ssvr_cntx_t *ctx)
{
    int idx;
    smtc_ssvr_t *ssvr;
    smtc_ssvr_conf_t *conf = &ctx->conf;

    /* 1. 创建发送线程池 */
    ctx->sendtp = thread_pool_init(conf->snd_thd_num, 4 * KB);
    if (NULL == ctx->sendtp)
    {
        thread_pool_destroy(ctx->sendtp);
        ctx->sendtp = NULL;
        return SMTC_ERR;
    }

    /* 2. 创建发送线程对象 */
    ctx->sendtp->data = calloc(conf->snd_thd_num, sizeof(smtc_ssvr_t));
    if (NULL == ctx->sendtp->data)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    /* 3. 设置发送线程对象 */
    ssvr = (smtc_ssvr_t *)ctx->sendtp->data;
    for (idx=0; idx<conf->snd_thd_num; ++idx, ++ssvr)
    {
        if (smtc_ssvr_init(ctx, ssvr, idx))
        {
            log_fatal(ctx->log, "Initialize send thread failed!");
            return SMTC_ERR;
        }
    }

    /* 4. 注册发送线程回调 */
    for (idx=0; idx<conf->snd_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, smtc_ssvr_routine, ctx);
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_sendtp_destroy
 **功    能: 销毁发送线程
 **输入参数: 
 **     ctx: 发送端上下文
 **     args: 线程池参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.04 #
 ******************************************************************************/
void smtc_ssvr_sendtp_destroy(void *_ctx, void *args)
{
    int idx = 0;
    smtc_ssvr_cntx_t *ctx = (smtc_ssvr_cntx_t *)ctx;
    smtc_ssvr_conf_t *conf = &ctx->conf;
    smtc_ssvr_t *ssvr = (smtc_ssvr_t *)args;

    /* 3. 设置发送线程对象 */
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
 **     ctx: 全局信息
 **     ssvr: 发送线程对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, int tidx)
{
    void *addr;
    smtc_ssvr_conf_t *conf = &ctx->conf;
    socket_recv_snap_t *recv = &ssvr->sck.recv;
    socket_send_snap_t *send = &ssvr->sck.send;

    ssvr->tidx = tidx;

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

    /* 4. 初始化发送缓存 */
    send->addr = calloc(1, SMTC_SSVR_SEND_BUFF_SIZE);
    if (NULL == send->addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }
    send->end = send->addr + SMTC_SSVR_SEND_BUFF_SIZE;
    send->optr = send->addr;
    send->iptr = send->addr;
    send->total = SMTC_SSVR_SEND_BUFF_SIZE;

    /* 5. 初始化接收缓存 */
    recv->addr = calloc(1, SMTC_SSVR_RECV_BUFF_SIZE);
    if (NULL == recv->addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }
    recv->end = recv->addr + SMTC_SSVR_RECV_BUFF_SIZE;
    recv->rptr = recv->addr;
    recv->wptr = recv->addr;
    recv->total = SMTC_SSVR_RECV_BUFF_SIZE;

    /* 6. 连接接收服务器 */
    if (smtc_ssvr_connect(ssvr, conf))
    {
        log_error(ssvr->log, "Connect recv server failed!");
        /* Note: Don't return error! */
    }
 
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_creat_sendq
 **功    能: 创建发送线程的发送队列
 **输入参数: 
 **     ssvr: 发送线程对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
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

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_creat_usck
 **功    能: 创建发送线程的命令接收套接字
 **输入参数: 
 **     ssvr: 发送线程对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_creat_usck(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN];

    smtc_ssvr_usck_path(conf, path, ssvr->tidx);
    
    ssvr->cmd_sck_id = unix_udp_creat(path);
    if (ssvr->cmd_sck_id < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return SMTC_ERR;
    }

    log_trace(ssvr->log, "cmd_sck_id:[%d] path:%s", ssvr->cmd_sck_id, path);
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_connect
 **功    能: 连接远程服务器
 **输入参数: 
 **     ctx: 全局信息
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.25 #
 ******************************************************************************/
static int smtc_ssvr_connect(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf)
{
    ;
   
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_bind_cpu
 **功    能: 绑定CPU
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 线程对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.11 #
 ******************************************************************************/
static void smtc_ssvr_bind_cpu(smtc_ssvr_cntx_t *ctx, int tidx)
{
    int idx, mod;
    cpu_set_t cpuset;
    smtc_cpu_conf_t *cpu = &ctx->conf.cpu;

    mod = sysconf(_SC_NPROCESSORS_CONF) - cpu->start;
    if (mod <= 0)
    {
        idx = tidx % sysconf(_SC_NPROCESSORS_CONF);
    }
    else
    {
        idx = cpu->start + (tidx % mod);
    }

    CPU_ZERO(&cpuset);
    CPU_SET(idx, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

/******************************************************************************
 **函数名称: smtc_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数: 
 **     ssvr: 发送线程对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.07.22 #
 ******************************************************************************/
void smtc_ssvr_set_rwset(smtc_ssvr_t *ssvr) 
{ 
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
    if (NULL != ssvr->sck.mesg_list
        || 0 != shm_queue_data_count(ssvr->sq))
    {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
    }

    return;
}

/******************************************************************************
 **函数名称: smtc_ssvr_routine
 **功    能: Snd线程调用的主程序
 **输入参数: 
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 获取发送线程
 **     2. 绑定CPU
 **     3. 调用发送主程
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.25 #
 ******************************************************************************/
static void *smtc_ssvr_routine(void *_ctx)
{
    int ret;
    smtc_ssvr_t *ssvr;
    smtc_ssvr_sck_t *sck;
    struct timeval timeout;
    smtc_ssvr_cntx_t *ctx = (smtc_ssvr_cntx_t *)_ctx;
    smtc_ssvr_conf_t *conf = &ctx->conf;


    /* 1. 获取发送线程 */
    ssvr = smtc_ssvr_get_curr(ctx);
    if (NULL == ssvr)
    {
        log_fatal(ssvr->log, "Get current thread failed!");
        abort();
        return (void *)-1;
    }

    sck = &ssvr->sck;

    /* 2. 绑定指定CPU */
    smtc_ssvr_bind_cpu(ctx, ssvr->tidx);

    /* 3. 进行事件处理 */
    for (;;)
    {
        /* 3.1 连接合法性判断 */
        if (!smtc_ssvr_conn_isvalid(ssvr))
        {
            smtc_ssvr_clear_mesg(ssvr);

            /* 重连Recv端 */
            if ((sck->fd = tcp_connect(AF_INET, conf->ipaddr, conf->port)) < 0)
            {
                log_error(ssvr->log, "Conncet receive-server failed!");

                Sleep(SMTC_RECONN_INTV);
                continue;
            }
        }

        /* 3.2 等待事件通知 */
        smtc_ssvr_set_rwset(ssvr);

        timeout.tv_sec = SMTC_SSVR_TMOUT_SEC;
        timeout.tv_usec = SMTC_SSVR_TMOUT_USEC;
        ret = select(ssvr->max+1, &ssvr->rset, &ssvr->wset, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_fatal(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return (void *)-1;
        }
        else if (0 == ret)
        {
            smtc_ssvr_timeout_hdl(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(sck->fd, &ssvr->wset))
        {
            smtc_ssvr_cmd_send_all_req_hdl(ctx, ssvr, NULL);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            smtc_ssvr_recv_cmd_hdl(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(sck->fd, &ssvr->rset))
        {
            smtc_ssvr_recv_proc(ctx, ssvr);
        }
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: smtc_ssvr_kpalive_req
 **功    能: 发送保活命令
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: Snd线程对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     因发送KeepAlive请求时，说明链路空闲时间较长，
 **     因此发送数据时，不用判断EAGAIN的情况是否存在。
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_kpalive_req(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    void *addr;
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    socket_send_snap_t *send = &ssvr->sck.send;
    smtc_header_t *head;
    int size = sizeof(smtc_header_t);

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0) 
        || (SMTC_KPALIVE_STAT_SENT == sck->kpalive)) 
    {
        Close(sck->fd);
        Free(send->addr);

        log_error(ssvr->log, "Didn't get keep-alive respond for a long time!");
        return SMTC_OK;
    }

    addr = slab_alloc(ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return SMTC_OK;
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
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_get_curr
 **功    能: 获取当前发送线程的上下文
 **输入参数: 
 **     ssvr: 发送线程对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: Address of sndsvr
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static smtc_ssvr_t *smtc_ssvr_get_curr(smtc_ssvr_cntx_t *ctx)
{
    int tidx;

    /* 1. 获取线程索引 */
    tidx = thread_pool_get_tidx(ctx->sendtp);
    if (tidx < 0)
    {
        log_error(ctx->log, "Get current thread index failed!");
        return NULL;
    }

    /* 2. 返回线程对象 */
    return (smtc_ssvr_t *)(ctx->sendtp->data + tidx * sizeof(smtc_ssvr_t));
}

/******************************************************************************
 **函数名称: smtc_ssvr_timeout_hdl
 **功    能: 超时处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送线程全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断是否长时间无数据通信
 **     2. 发送保活数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_timeout_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int ret = 0;
    time_t curr_tm = time(NULL);
    smtc_ssvr_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((NULL != ssvr->sck.send.addr)
        || (curr_tm - sck->wrtm) < SMTC_SCK_KPALIVE_SEC)
    {
        return SMTC_OK;
    }

    /* 2. 发送保活请求 */
    ret = smtc_ssvr_kpalive_req(ctx, ssvr);
    if (0 != ret)
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return SMTC_ERR;
    }

    sck->wrtm = curr_tm;

    return SMTC_OK;
}

/******************************************************************************
 ** Name : smtc_ssvr_head_isvalid
 ** Desc : Check head
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2015.05.13 #
 ******************************************************************************/
static bool smtc_ssvr_head_isvalid(smtc_ssvr_cntx_t *ctx, smtc_header_t *head)
{
    if ((SMTC_MSG_MARK_KEY != head->mark)
        || !smtc_is_type_valid(head->type)
        || head->length + sizeof(smtc_header_t) >= SMTC_SSVR_RECV_BUFF_SIZE)
    {
        return false;
    }

    return true;
}

/******************************************************************************
 ** Name : smtc_ssvr_data_proc
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
 ** Author: # Qifeng.zou # 2015.05.13 #
 ******************************************************************************/
static int smtc_ssvr_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_recv_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    /* 1. Handle system data */
    if (SMTC_SYS_MESG == head->flag)
    {
        return smtc_ssvr_sys_mesg_proc(ctx, ssvr, sck);
    }

    /* 2. Forward expand data */
    return smtc_ssvr_exp_mesg_proc(ctx, ssvr, sck);
}

/******************************************************************************
 **函数名称: smtc_ssvr_recv_proc
 **功    能: 接收来自服务器的信息
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送线程全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 读取消息HEAD
 **     2. 读取消息BODY
 **     3. 处理消息内容
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_recv_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int n, left;
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    socket_recv_snap_t *recv = &sck->recv;

    sck->rdtm = time(NULL);

    while (1)
    {
        /* 1. 接收网络数据 */
        left = (int)(recv->end - recv->rptr);

        n = read(sck->fd, recv->rptr, left);
        if (n > 0)
        {
            recv->rptr += n;

            /* 2. 进行数据处理 */
            if (smtc_ssvr_data_proc(ctx, ssvr, sck))
            {
                log_error(ssvr->log, "Proc data failed! fd:%d", sck->fd);
                return SMTC_ERR;
            }
            continue;
        }
        else if (0 == n)
        {
            log_info(ssvr->log, "Client disconnected. errmsg:[%d] %s! fd:%d n:%d/%d",
                    errno, strerror(errno), sck->fd, n, left);
            return SMTC_SCK_CLOSED;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return SMTC_OK; /* Again */
        }

        if (EINTR == errno)
        {
            continue;
        }

        log_error(ssvr->log, "errmsg:[%d] %s. fd:%d", errno, strerror(errno), sck->fd);
        return SMTC_ERR;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_recv_cmd_hdl
 **功    能: 接收命令
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送线程对象
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收命令
 **     2. 处理命令
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_recv_cmd_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    smtc_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    if (unix_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd)) < 0)
    {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    /* 2. 处理命令 */
    if (smtc_ssvr_cmd_hdl(ctx, ssvr, &cmd))
    {
        log_error(ssvr->log, "Handle command failed");
        return SMTC_ERR;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_cmd_hdl
 **功    能: 命令处理
 **输入参数: 
 **     ssvr: 发送线程对象
 **     cmd: 接收到的命令信息
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_cmd_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_t *cmd)
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
            return SMTC_OK;
        }
        default:
        {
            log_error(ssvr->log, "Received unknown command! type:[%d]", cmd->type);
            return SMTC_OK;
        }
    }
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_fill_send_buff
 **功    能: 填充发送缓冲区
 **输入参数: 
 **     ssvr: 发送线程
 **     sck: 套接字对象
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 从消息链表取数据
 **     2. 从发送队列取数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_fill_send_buff(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    void *addr;
    list_node_t *node;
    int left, one_mesg_len;
    smtc_header_t *head;
    socket_send_snap_t *send = &sck->send;

    /* 1. 从消息链表取数据 */
    for (;;)
    {
        /* 1.1 判断发送缓存的剩余空间是否足够 */
        node = ssvr->sck.mesg_list->head;
        if (NULL == node)
        {
            break; /* 无数据 */
        }

        head = (smtc_header_t *)node->data;

        left = (int)(send->end - send->iptr);
        one_mesg_len = sizeof(smtc_header_t) + head->length;
        if (left < one_mesg_len)
        {
            break;
        }

        /* 1.2 取发送的数据 */
        addr = smtc_ssvr_get_mesg(ssvr);

        head = (smtc_header_t *)addr;

        head->type = htonl(head->type);
        head->length = htonl(head->length);
        head->flag = htonl(head->flag);
        head->mark = htonl(head->mark);

        /* 1.3 拷贝至发送缓存 */
        memcpy(send->iptr, addr, one_mesg_len);

        send->iptr += one_mesg_len;
        continue;
    }

    /* 2. 从发送队列取数据 */
    for (;;)
    {
        /* 2.1 判断发送缓存的剩余空间是否足够 */
        left = (int)(send->end - send->iptr);
        if (left < ssvr->sq->size)
        {
            break;
        }

        /* 2.2 取发送的数据 */
        addr = shm_queue_pop(ssvr->sq);
        if (NULL == addr)
        {
            break;
        }

        head = (smtc_header_t *)addr;

        one_mesg_len = sizeof(smtc_header_t) + head->length;

        head->type = htonl(head->type);
        head->length = htonl(head->length);
        head->flag = htonl(head->flag);
        head->mark = htonl(head->mark);

        /* 2.3 拷贝至发送缓存 */
        memcpy(send->iptr, addr, one_mesg_len);

        send->iptr += one_mesg_len;
        continue;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_cmd_send_all_req_hdl
 **功    能: 发送数据的请求处理
 **输入参数: 
 **     ssvr: 发送线程
 **     args: 命令参数
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_ssvr_cmd_send_all_req_hdl(
        smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_send_req_t *args)
{
    int n, len;
    time_t ctm = time(NULL);
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    socket_send_snap_t *send = &sck->send;

    sck->wrtm = ctm;

    for (;;)
    {
        /* 1. 填充缓存 */
        if (send->iptr == send->optr)
        {
            smtc_ssvr_fill_send_buff(ssvr, sck);
        }

        /* 2. 发送数据 */
        len = send->iptr - send->optr;

        n = Writen(sck->fd, send->optr, len);
        if (n < 0)
        {
        #if defined(__SMTC_DEBUG__)
            send->fail++;
            fprintf(stderr, "succ:%llu fail:%llu again:%llu\n",
                send->succ, send->fail, send->again);
        #endif /*__SMTC_DEBUG__*/

            log_error(ssvr->log, "\terrmsg:[%d] %s! fd:%d len:[%d]",
                errno, strerror(errno), sck->fd, len);

            Close(sck->fd);
            Free(send->addr);
            return SMTC_ERR;
        }
        /* 只发送了部分数据 */
        else if (n != len)
        {
            send->optr += n;
        #if defined(__SMTC_DEBUG__)
            send->again++;
        #endif /*__SMTC_DEBUG__*/
            return SMTC_OK;
        }

        send->optr += n;

        /* 4. 重置标识量 */
        send->optr = send->addr;
        send->iptr = send->addr;
    #if defined(__SMTC_DEBUG__)
        send->succ++;
    #endif /*__SMTC_DEBUG__*/
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
 **作    者: # Qifeng.zou # 2015.07.04 #
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
 **函数名称: smtc_ssvr_get_mesg
 **功    能: 获取发送数据
 **输入参数: 
 **    ssvr: Recv对象
 **    sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.07.04 #
 ******************************************************************************/
static void *smtc_ssvr_get_mesg(smtc_ssvr_t *ssvr)
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
 **函数名称: smtc_ssvr_clear_mesg
 **功    能: 清空发送数据
 **输入参数: 
 **    ssvr: Send对象
 **    sck: 套接字对象
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.07.04 #
 ******************************************************************************/
static int smtc_ssvr_clear_mesg(smtc_ssvr_t *ssvr)
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
 ** Name : smtc_ssvr_sys_mesg_proc
 ** Desc : Handle system data
 ** Input: 
 **     ctx: 上下文
 **     ssvr: Recv context
 **     sck: Socket information
 ** Output: NONE
 ** Return: 0:Succ !0:Fail
 ** Proc : 
 ** Note : 
 ** Author: # Qifeng.zou # 2015.04.14 #
 ******************************************************************************/
static int smtc_ssvr_sys_mesg_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    socket_recv_snap_t *recv = &sck->recv;
    smtc_header_t *head = (smtc_header_t *)recv->addr;

    switch (head->type)
    {
        case SMTC_KPALIVE_REP:  /* 保活应答数据 */
        {
            log_debug(ssvr->log, "Received keepalive respond!");

            smtc_set_kpalive_stat(sck, SMTC_KPALIVE_STAT_SUCC);
            return SMTC_OK;
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
 ** Name : smtc_ssvr_exp_mesg_proc
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
 **      | 已处理 |     未处理     |       剩余空间       |
 **       ------------------------------------------------
 **      |XXXXXXXX|////////////////|                      |
 **      |XXXXXXXX|////////////////|         left         |
 **      |XXXXXXXX|////////////////|                      |
 **       ------------------------------------------------
 **      ^        ^                ^                      ^
 **      |        |                |                      |
 **     addr     wptr             rptr                   end
 ** Author: # Qifeng.zou # 2015.04.10 #
 ******************************************************************************/
static int smtc_ssvr_exp_mesg_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    smtc_header_t *head;
    int len, one_mesg_len;
    socket_recv_snap_t *recv = &sck->recv;

    while (1)
    {
        head = (smtc_header_t *)recv->wptr;
        len = (int)(recv->rptr - recv->wptr);

        /* 1. 不足一条数据时 */
        one_mesg_len = sizeof(smtc_header_t) + head->length;
        if (len < sizeof(smtc_header_t)
            || len < one_mesg_len)
        {
            memcpy(recv->addr, recv->wptr, len);
            recv->wptr = recv->addr;
            recv->rptr = recv->wptr + len;
            return SMTC_OK;
        }

        /* 2. 至少一条数据时 */
        /* 2.1 转化字节序 */
        head->type = ntohl(head->type);
        head->length = ntohl(head->length);
        head->flag = ntohl(head->flag);
        head->mark = ntohl(head->mark);

        /* 2.2 校验合法性 */
        if (smtc_ssvr_head_isvalid(ctx, head))
        {
            log_error(ssvr->log, "Header is invalid! Mark:%u/%u type:%d len:%d flag:%d",
                    head->mark, SMTC_MSG_MARK_KEY, head->type, head->length, head->flag);
            return SMTC_ERR;
        }

        /* 2.3 进行数据处理 */
        if (SMTC_SYS_MESG == head->flag)
        {
            smtc_ssvr_sys_mesg_proc(ctx, ssvr, sck);
        }
        else
        {
            smtc_ssvr_exp_mesg_proc(ctx, ssvr, sck);
        }

        recv->wptr += one_mesg_len;
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
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
void smtc_ssvr_reset_sck(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    /* 1. 重置标识量 */
    Close(sck->fd);
    sck->wrtm = 0;
    sck->rdtm = 0;

    smtc_set_kpalive_stat(sck, SMTC_KPALIVE_STAT_UNKNOWN);

    /* 2. 释放空间 */
    smtc_ssvr_clear_mesg(ssvr);
}

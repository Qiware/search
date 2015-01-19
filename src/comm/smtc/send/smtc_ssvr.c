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

#include "shm_opt.h"
#include "syscall.h"
#include "smtc_cmd.h"
#include "smtc_cli.h"
#include "smtc_ssvr.h"

/* 静态函数 */
static int _smtc_ssvr_startup(smtc_ssvr_cntx_t *ctx);
static void *smtc_ssvr_routine(void *_ctx);

static int smtc_ssvr_init(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, int tidx);
static smtc_ssvr_t *smtc_ssvr_get_curr(smtc_ssvr_cntx_t *ctx);

static int smtc_ssvr_creat_sendq(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);
static int smtc_ssvr_creat_usck(smtc_ssvr_t *ssvr, const smtc_ssvr_conf_t *conf);

static int smtc_ssvr_kpalive_req(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);

static int smtc_ssvr_recv_cmd(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static int smtc_ssvr_recv_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);

static int smtc_ssvr_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck);
static int smtc_ssvr_sys_mesg_proc(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck, void *addr);
static int smtc_ssvr_exp_mesg_proc(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck, void *addr);

static int smtc_ssvr_timeout_hdl(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);
static int smtc_ssvr_proc_cmd(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_t *cmd);
static int smtc_ssvr_send_data(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr);

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
smtc_ssvr_cntx_t *smtc_ssvr_startup(const smtc_ssvr_conf_t *conf, log_cycle_t *log)
{
    smtc_ssvr_cntx_t *ctx;

    /* 1. 创建上下文对象 */
    ctx = (smtc_ssvr_cntx_t *)calloc(1, sizeof(smtc_ssvr_cntx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smtc_ssvr_conf_t));

    /* 3. 启动各发送服务 */
    if (_smtc_ssvr_startup(ctx))
    {
        printf("Initalize send thread failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: _smtc_ssvr_startup
 **功    能: 启动发送端
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建发送线程池
 **     2. 创建发送线程对象
 **     3. 设置发送线程对象
 **     4. 注册发送线程回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int _smtc_ssvr_startup(smtc_ssvr_cntx_t *ctx)
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
    smtc_snap_t *recv = &ssvr->sck.recv;
    smtc_snap_t *send = &ssvr->sck.send;

    ssvr->tidx = tidx;
    ssvr->log = ctx->log;

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
    addr = calloc(1, conf->send_buff_size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    smtc_snap_setup(send, addr, conf->send_buff_size);

    /* 5. 初始化接收缓存 */
    addr = calloc(1, conf->recv_buff_size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    smtc_snap_setup(recv, addr, conf->recv_buff_size);

    /* 6. 连接接收服务器 */
    if ((ssvr->sck.fd = tcp_connect(AF_INET, conf->ipaddr, conf->port)) < 0)
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
    const smtc_queue_conf_t *qcf = &conf->qcf;

    /* 1. 创建/连接发送队列 */
    key = shm_ftok(qcf->name, ssvr->tidx);
    if (-1 == key)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

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
 **函数名称: smtc_ssvr_bind_cpu
 **功    能: 绑定CPU
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 线程对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
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
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
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
    if (NULL != ssvr->sck.mesg_list.head
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
 **作    者: # Qifeng.zou # 2015.01.16 #
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
            abort();
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
            smtc_ssvr_send_data(ctx, ssvr);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            smtc_ssvr_recv_cmd(ctx, ssvr);
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
    smtc_header_t *head;
    int size = sizeof(smtc_header_t);
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    smtc_snap_t *send = &ssvr->sck.send;

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0) 
        || (SMTC_KPALIVE_STAT_SENT == sck->kpalive)) 
    {
        Close(sck->fd);
        Free(send->addr);

        log_error(ssvr->log, "Didn't get keepalive respond for a long time!");
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
    head->checksum = SMTC_CHECK_SUM;

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
    time_t curr_tm = time(NULL);
    smtc_ssvr_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((curr_tm - sck->wrtm) < SMTC_KPALIVE_INTV)
    {
        return SMTC_OK;
    }

    /* 2. 发送保活请求 */
    if (smtc_ssvr_kpalive_req(ctx, ssvr))
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return SMTC_ERR;
    }

    sck->wrtm = curr_tm;

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_head_isvalid
 **功    能: 判断报头合法性
 **输入参数: 
 **     ctx: 全局信息
 **     head: 报头合法性
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断校验值、数据类型、长度的合法性
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
#define smtc_ssvr_head_isvalid(ctx, head) \
    (((SMTC_CHECK_SUM != head->checksum) \
        || !smtc_is_type_valid(head->type) \
        || (head->length + sizeof(smtc_header_t) >= SMTC_BUFF_SIZE))? false : true)

/******************************************************************************
 **函数名称: smtc_ssvr_recv_proc
 **功    能: 接收网络数据
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收网络数据
 **     2. 进行数据处理
 **注意事项: 
 **       ------------------------------------------------
 **      | 已处理 |     未处理     |       剩余空间       |
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
static int smtc_ssvr_recv_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int n, left;
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    smtc_snap_t *recv = &sck->recv;

    sck->rdtm = time(NULL);

    while (1)
    {
        /* 1. 接收网络数据 */
        left = (int)(recv->end - recv->iptr);

        n = read(sck->fd, recv->iptr, left);
        if (n > 0)
        {
            recv->iptr += n;

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
            log_info(ssvr->log, "Client disconnected. fd:%d n:%d/%d", sck->fd, n, left);
            return SMTC_DISCONN;
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
 **函数名称: smtc_ssvr_data_proc
 **功    能: 进行数据处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 是否含有完整数据
 **     2. 校验数据合法性
 **     3. 进行数据处理
 **注意事项: 
 **       ------------------------------------------------
 **      | 已处理 |     未处理     |       剩余空间       |
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
static int smtc_ssvr_data_proc(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    smtc_header_t *head;
    int len, mesg_len;
    smtc_snap_t *recv = &sck->recv;

    while (1)
    {
        head = (smtc_header_t *)recv->optr;
        len = (int)(recv->iptr - recv->optr);

        /* 1. 不足一条数据时 */
        mesg_len = sizeof(smtc_header_t) + head->length;
        if (len < sizeof(smtc_header_t)
            || len < mesg_len)
        {
            if (recv->iptr == recv->end) 
            {
                /* 防止OverWrite的情况发生 */
                if ((recv->optr - recv->addr) < (recv->end - recv->iptr))
                {
                    log_error(ssvr->log, "Data length is invalid!");
                    return SMTC_ERR;
                }

                memcpy(recv->addr, recv->optr, len);
                recv->optr = recv->addr;
                recv->iptr = recv->optr + len;
                return SMTC_OK;
            }
            return SMTC_OK;
        }

        /* 2. 至少一条数据时 */
        /* 2.1 转化字节序 */
        head->type = ntohl(head->type);
        head->length = ntohl(head->length);
        head->flag = ntohl(head->flag);
        head->checksum = ntohl(head->checksum);

        /* 2.2 校验合法性 */
        if (smtc_ssvr_head_isvalid(ctx, head))
        {
            log_error(ssvr->log, "Header is invalid! CheckSum:%u/%u type:%d len:%d flag:%d",
                    head->checksum, SMTC_CHECK_SUM, head->type, head->length, head->flag);
            return SMTC_ERR;
        }

        /* 2.3 进行数据处理 */
        if (SMTC_SYS_MESG == head->flag)
        {
            smtc_ssvr_sys_mesg_proc(ssvr, sck, recv->addr);
        }
        else
        {
            smtc_ssvr_exp_mesg_proc(ssvr, sck, recv->addr);
        }

        recv->optr += mesg_len;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_recv_cmd
 **功    能: 接收命令数据
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
static int smtc_ssvr_recv_cmd(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
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
    return smtc_ssvr_proc_cmd(ctx, ssvr, &cmd);
}

/******************************************************************************
 **函数名称: smtc_ssvr_proc_cmd
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
static int smtc_ssvr_proc_cmd(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr, const smtc_cmd_t *cmd)
{
    smtc_ssvr_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case SMTC_CMD_SEND:
        case SMTC_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd))
            {
                return smtc_ssvr_send_data(ctx, ssvr);
            }
            return SMTC_OK;
        }
        default:
        {
            log_error(ssvr->log, "Unknown command! type:[%d]", cmd->type);
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
 **     sck: 连接对象
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 从消息链表取数据
 **     2. 从发送队列取数据
 **注意事项: 
 **       ------------------------------------------------
 **      | 已处理 |     未处理     |       剩余空间       |
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
static int smtc_ssvr_fill_send_buff(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck)
{
    void *addr;
    list_node_t *node;
    int left, mesg_len;
    smtc_header_t *head;
    smtc_snap_t *send = &sck->send;

    /* 1. 从消息链表取数据 */
    for (;;)
    {
        /* 1.1 是否有数据 */
        node = ssvr->sck.mesg_list.head;
        if (NULL == node)
        {
            break; /* 无数据 */
        }

        /* 1.2 判断剩余空间 */
        head = (smtc_header_t *)node->data;

        left = (int)(send->end - send->iptr);
        mesg_len = sizeof(smtc_header_t) + head->length;
        if (left < mesg_len)
        {
            break; /* 空间不足 */
        }

        /* 1.3 取发送的数据 */
        addr = smtc_ssvr_get_mesg(ssvr);

        head = (smtc_header_t *)addr;

        head->type = htonl(head->type);
        head->length = htonl(head->length);
        head->flag = htonl(head->flag);
        head->checksum = htonl(head->checksum);

        /* 1.4 拷贝至发送缓存 */
        memcpy(send->iptr, addr, mesg_len);

        send->iptr += mesg_len;
        continue;
    }

    /* 2. 从发送队列取数据 */
    for (;;)
    {
        /* 2.1 判断发送缓存的剩余空间是否足够 */
        left = (int)(send->end - send->iptr);
        if (left < ssvr->sq->size)
        {
            break;  /* 空间不足 */
        }

        /* 2.2 取发送的数据 */
        addr = shm_queue_pop(ssvr->sq);
        if (NULL == addr)
        {
            break;  /* 无数据 */
        }

        head = (smtc_header_t *)addr;

        mesg_len = sizeof(smtc_header_t) + head->length;

        head->type = htonl(head->type);
        head->length = htonl(head->length);
        head->flag = htonl(head->flag);
        head->checksum = htonl(head->checksum);

        /* 2.3 拷贝至发送缓存 */
        memcpy(send->iptr, addr, mesg_len);

        send->iptr += mesg_len;
        continue;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_send_data
 **功    能: 发送数据的请求处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送线程
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 填充发送缓存
 **     2. 发送缓存数据
 **     3. 重置标识量
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
static int smtc_ssvr_send_data(smtc_ssvr_cntx_t *ctx, smtc_ssvr_t *ssvr)
{
    int n, len;
    time_t ctm = time(NULL);
    smtc_ssvr_sck_t *sck = &ssvr->sck;
    smtc_snap_t *send = &sck->send;

    sck->wrtm = ctm;

    for (;;)
    {
        /* 1. 填充发送缓存 */
        if (send->iptr == send->optr)
        {
            smtc_ssvr_fill_send_buff(ssvr, sck);
        }

        /* 2. 发送缓存数据 */
        len = send->iptr - send->optr;

        n = Writen(sck->fd, send->optr, len);
        if (n < 0)
        {
        #if defined(__SMTC_DEBUG__)
            send->fail++;
        #endif /*__SMTC_DEBUG__*/

            log_error(ssvr->log, "errmsg:[%d] %s! fd:%d len:[%d]",
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

        /* 3. 重置标识量 */
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
 **功    能: 添加发送消息
 **输入参数: 
 **     ssvr: 发送服务
 **     addr: 消息地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1.创建新结点
 **     2.插入链尾
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
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
    list_insert_tail(&ssvr->sck.mesg_list, add);

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_get_mesg
 **功    能: 获取发送消息
 **输入参数: 
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     取出链首结点的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static void *smtc_ssvr_get_mesg(smtc_ssvr_t *ssvr)
{
    void *addr;
    list_node_t *node;

    node = list_remove_head(&ssvr->sck.mesg_list);
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
 **功    能: 清空发送消息
 **输入参数: 
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     依次取出每条消息, 并释放所占有的空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static int smtc_ssvr_clear_mesg(smtc_ssvr_t *ssvr)
{
    list_node_t *node;

    while (NULL != (node = list_remove_head(&ssvr->sck.mesg_list)))
    {
        slab_dealloc(ssvr->pool, node->data);
        slab_dealloc(ssvr->pool, node);
    }

    ssvr->sck.mesg_list.head = NULL;
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_sys_mesg_proc
 **功    能: 系统消息的处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     根据消息类型调用对应的处理接口
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static int smtc_ssvr_sys_mesg_proc(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck, void *addr)
{
    smtc_header_t *head = (smtc_header_t *)addr;

    switch (head->type)
    {
        case SMTC_KPALIVE_REP:  /* 保活应答 */
        {
            log_debug(ssvr->log, "Received keepalive respond!");

            smtc_set_kpalive_stat(sck, SMTC_KPALIVE_STAT_SUCC);
            return SMTC_OK;
        }
        default:
        {
            log_error(ssvr->log, "Unknown type [%d]!", head->type);
            return SMTC_ERR;
        }
    }
    
    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_ssvr_exp_mesg_proc
 **功    能: 自定义消息的处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static int smtc_ssvr_exp_mesg_proc(smtc_ssvr_t *ssvr, smtc_ssvr_sck_t *sck, void *addr)
{
    return SMTC_OK;
}

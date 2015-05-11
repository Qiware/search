
#include "shm_opt.h"
#include "syscall.h"
#include "sdtp_cmd.h"
#include "sdtp_cli.h"
#include "sdtp_ssvr.h"

/* 静态函数 */
static int _sdtp_ssvr_startup(sdtp_ssvr_cntx_t *ctx);
static void *sdtp_ssvr_routine(void *_ctx);

static int sdtp_ssvr_init(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr, int tidx);
static sdtp_ssvr_t *sdtp_ssvr_get_self(sdtp_ssvr_cntx_t *ctx);

static int sdtp_ssvr_creat_sendq(sdtp_ssvr_t *ssvr, const sdtp_ssvr_conf_t *conf);
static int sdtp_ssvr_creat_usck(sdtp_ssvr_t *ssvr, const sdtp_ssvr_conf_t *conf);

static int sdtp_ssvr_kpalive_req(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr);

static int sdtp_ssvr_recv_cmd(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr);
static int sdtp_ssvr_recv_proc(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr);

static int sdtp_ssvr_data_proc(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck);
static int sdtp_ssvr_sys_mesg_proc(sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck, void *addr);
static int sdtp_ssvr_exp_mesg_proc(sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck, void *addr);

static int sdtp_ssvr_timeout_hdl(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr);
static int sdtp_ssvr_proc_cmd(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr, const sdtp_cmd_t *cmd);
static int sdtp_ssvr_send_data(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr);

static int sdtp_ssvr_clear_mesg(sdtp_ssvr_t *ssvr);

/******************************************************************************
 **函数名称: sdtp_ssvr_startup
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
sdtp_ssvr_cntx_t *sdtp_ssvr_startup(const sdtp_ssvr_conf_t *conf, log_cycle_t *log)
{
    sdtp_ssvr_cntx_t *ctx;

    /* 1. 创建上下文对象 */
    ctx = (sdtp_ssvr_cntx_t *)calloc(1, sizeof(sdtp_ssvr_cntx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(sdtp_ssvr_conf_t));

    /* 3. 启动各发送服务 */
    if (_sdtp_ssvr_startup(ctx))
    {
        printf("Initalize send thread failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: _sdtp_ssvr_startup
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
static int _sdtp_ssvr_startup(sdtp_ssvr_cntx_t *ctx)
{
    int idx;
    sdtp_ssvr_t *ssvr;
    thread_pool_opt_t opt;
    sdtp_ssvr_conf_t *conf = &ctx->conf;

    /* > 创建发送线程对象 */
    ssvr = calloc(conf->snd_thd_num, sizeof(sdtp_ssvr_t));
    if (NULL == ssvr)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建内存池 */
    ctx->slab = slab_creat_by_calloc(30 * MB);
    if (NULL == ctx->slab)
    {
        log_error(ctx->log, "Initialize slab failed!");
        FREE(ssvr);
        return SDTP_ERR;
    }

    /* > 创建发送线程池 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->slab;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->sendtp = thread_pool_init(conf->snd_thd_num, &opt, (void *)ssvr);
    if (NULL == ctx->sendtp)
    {
        free(ssvr);
        thread_pool_destroy(ctx->sendtp);
        ctx->sendtp = NULL;
        log_error(ctx->log, "Initialize thread pool failed!");
        return SDTP_ERR;
    }

    /* 3. 设置发送线程对象 */
    for (idx=0; idx<conf->snd_thd_num; ++idx)
    {
        if (sdtp_ssvr_init(ctx, ssvr+idx, idx))
        {
            free(ssvr);
            thread_pool_destroy(ctx->sendtp);
            log_fatal(ctx->log, "Initialize send thread failed!");
            return SDTP_ERR;
        }
    }

    /* 4. 注册发送线程回调 */
    for (idx=0; idx<conf->snd_thd_num; idx++)
    {
        thread_pool_add_worker(ctx->sendtp, sdtp_ssvr_routine, ctx);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_init
 **功    能: 初始化发送线程
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int sdtp_ssvr_init(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr, int tidx)
{
    void *addr;
    list_opt_t opt;
    sdtp_ssvr_conf_t *conf = &ctx->conf;
    sdtp_snap_t *recv = &ssvr->sck.recv;
    sdtp_snap_t *send = &ssvr->sck.send[SDTP_SNAP_SYS_DATA];

    ssvr->tidx = tidx;
    ssvr->log = ctx->log;

    /* > 创建发送队列 */
    if (sdtp_ssvr_creat_sendq(ssvr, conf))
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SDTP_ERR;
    }

    /* > 创建unix套接字 */
    if (sdtp_ssvr_creat_usck(ssvr, conf))
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SDTP_ERR;
    }

    /* > 创建SLAB内存池 */
    ssvr->pool = slab_creat_by_calloc(SDTP_MEM_POOL_SIZE);
    if (NULL == ssvr->pool)
    {
        log_error(ssvr->log, "Initialize slab mem-pool failed!");
        return SDTP_ERR;
    }

    /* > 创建发送链表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ssvr->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ssvr->sck.mesg_list = list_creat(&opt);
    if (NULL == ssvr->sck.mesg_list)
    {
        log_error(ssvr->log, "Create list failed!");
        return SDTP_ERR;
    }

    /* > 初始化发送缓存 */
    addr = calloc(1, conf->send_buff_size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    sdtp_snap_setup(send, addr, conf->send_buff_size);

    /* 5. 初始化接收缓存 */
    addr = calloc(1, conf->recv_buff_size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    sdtp_snap_setup(recv, addr, conf->recv_buff_size);

    /* 6. 连接接收服务器 */
    if ((ssvr->sck.fd = tcp_connect(AF_INET, conf->ipaddr, conf->port)) < 0)
    {
        log_error(ssvr->log, "Connect recv server failed!");
        /* Note: Don't return error! */
    }
 
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_creat_sendq
 **功    能: 创建发送线程的发送队列
 **输入参数: 
 **     ssvr: 发送服务对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int sdtp_ssvr_creat_sendq(sdtp_ssvr_t *ssvr, const sdtp_ssvr_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN];
    const sdtp_queue_conf_t *qcf = &conf->qcf;

    /* 1. 创建/连接发送队列 */
    snprintf(path, sizeof(path), "%s-%d", qcf->name, ssvr->tidx);

    ssvr->sq = sdtp_pool_creat(path, qcf->count, qcf->size);
    if (NULL == ssvr->sq)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_creat_usck
 **功    能: 创建发送线程的命令接收套接字
 **输入参数: 
 **     ssvr: 发送服务对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int sdtp_ssvr_creat_usck(sdtp_ssvr_t *ssvr, const sdtp_ssvr_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN];

    sdtp_ssvr_usck_path(conf, path, ssvr->tidx);
    
    ssvr->cmd_sck_id = unix_udp_creat(path);
    if (ssvr->cmd_sck_id < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return SDTP_ERR;
    }

    log_trace(ssvr->log, "cmd_sck_id:[%d] path:%s", ssvr->cmd_sck_id, path);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_bind_cpu
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
static void sdtp_ssvr_bind_cpu(sdtp_ssvr_cntx_t *ctx, int tidx)
{
    int idx, mod;
    cpu_set_t cpuset;
    sdtp_cpu_conf_t *cpu = &ctx->conf.cpu;

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
 **函数名称: sdtp_switch_send_data
 **功    能: 切换发送数据的处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.11 #
 ******************************************************************************/
void sdtp_switch_send_data(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    sdtp_snap_t *send;
    sdtp_pool_page_t *page;
    sdtp_ssvr_sck_t *sck = &ssvr->sck;

    /* > 检查是否发送完系统消息 */
    switch (sck->send_type)
    {
        case SDTP_SNAP_SYS_DATA:
        {
            send = &sck->send[SDTP_SNAP_SYS_DATA];
            if (!list_isempty(sck->mesg_list)
                || (send->iptr != send->optr))
            {
                return; /* 系统消息还未发送完成: 不用切换 */
            }
            break;
        }
        case SDTP_SNAP_EXP_DATA:
        default:
        {
            /* > 检查是否发送完外部数据 */
            send = &sck->send[SDTP_SNAP_EXP_DATA];
            if (send->iptr != send->optr)
            {
                return; /* 缓存数据仍然未发送完全 */
            }

            if (!list_isempty(sck->mesg_list))
            {
                sck->send_type = SDTP_SNAP_SYS_DATA;
                return; /* 有消息可发送 */
            }
            break;
        }
    }

    page = sdtp_pool_switch(ssvr->sq);
    if (NULL == page)
    {
        return; /* 无可发送的数据 */
    }

    sck->send_type = SDTP_SNAP_EXP_DATA;
    send = &sck->send[SDTP_SNAP_EXP_DATA];

    send->addr = (void *)ssvr->sq->head + page->begin;
    send->end = send->addr + page->off;
    send->total = page->off;
    send->optr = send->addr;
    send->iptr = send->addr + page->off;

#if 0
    /* > 校验数据合法性(测试数据时使用) */
    head = (sdtp_header_t *)send->addr;
    for (idx=0; idx<page->num; ++idx)
    {
        if (!dtsd_header_isvalid(ctx, head))
        {
            assert(0);
        }
        head = (void *)head + sizeof(sdtp_header_t) + head->length;
    }
#endif

    return;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数: 
 **     ssvr: 发送服务对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
void sdtp_ssvr_set_rwset(sdtp_ssvr_t *ssvr) 
{ 
    int idx;
    sdtp_snap_t *snap;

    FD_ZERO(&ssvr->rset);
    FD_ZERO(&ssvr->wset);

    FD_SET(ssvr->cmd_sck_id, &ssvr->rset);

    if (ssvr->sck.fd < 0)
    {
        return;
    }

    ssvr->max = MAX(ssvr->cmd_sck_id, ssvr->sck.fd);

    /* 1 设置读集合 */
    FD_SET(ssvr->sck.fd, &ssvr->rset);

    /* 2 设置写集合: 发送至接收端 */
    if (!list_isempty(ssvr->sck.mesg_list))
    {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
        return;
    }

    snap = &ssvr->sck.send[SDTP_SNAP_SYS_DATA];
    for (idx=0; idx<SDTP_SNAP_TOTAL; ++idx, ++snap)
    {
        if (snap->iptr != snap->optr)
        {
            FD_SET(ssvr->sck.fd, &ssvr->wset);
            return;
        }
    }

    return;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_routine
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
static void *sdtp_ssvr_routine(void *_ctx)
{
    int ret;
    sdtp_ssvr_t *ssvr;
    sdtp_ssvr_sck_t *sck;
    struct timeval timeout;
    sdtp_ssvr_cntx_t *ctx = (sdtp_ssvr_cntx_t *)_ctx;
    sdtp_ssvr_conf_t *conf = &ctx->conf;


    /* 1. 获取发送线程 */
    ssvr = sdtp_ssvr_get_self(ctx);
    if (NULL == ssvr)
    {
        log_fatal(ssvr->log, "Get current thread failed!");
        abort();
        return (void *)-1;
    }

    sck = &ssvr->sck;

    /* 2. 绑定指定CPU */
    sdtp_ssvr_bind_cpu(ctx, ssvr->tidx);

    /* 3. 进行事件处理 */
    for (;;)
    {
        /* 3.1 连接合法性判断 */
        if (sck->fd < 0)
        {
            sdtp_ssvr_clear_mesg(ssvr);

            /* 重连Recv端 */
            if ((sck->fd = tcp_connect(AF_INET, conf->ipaddr, conf->port)) < 0)
            {
                log_error(ssvr->log, "Conncet receive-server failed!");

                Sleep(SDTP_RECONN_INTV);
                continue;
            }
        }

        sdtp_switch_send_data(ctx, ssvr);

        /* 3.2 等待事件通知 */
        sdtp_ssvr_set_rwset(ssvr);

        timeout.tv_sec = SDTP_SSVR_TMOUT_SEC;
        timeout.tv_usec = SDTP_SSVR_TMOUT_USEC;
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
            sdtp_ssvr_timeout_hdl(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(sck->fd, &ssvr->wset))
        {
            sdtp_ssvr_send_data(ctx, ssvr);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            sdtp_ssvr_recv_cmd(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(sck->fd, &ssvr->rset))
        {
            sdtp_ssvr_recv_proc(ctx, ssvr);
        }
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_kpalive_req
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
static int sdtp_ssvr_kpalive_req(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    void *addr;
    sdtp_header_t *head;
    int size = sizeof(sdtp_header_t);
    sdtp_ssvr_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *send = &ssvr->sck.send[SDTP_SNAP_SYS_DATA];

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0) 
        || (SDTP_KPALIVE_STAT_SENT == sck->kpalive)) 
    {
        CLOSE(sck->fd);
        FREE(send->addr);

        log_error(ssvr->log, "Didn't get keepalive respond for a long time!");
        return SDTP_OK;
    }

    addr = slab_alloc(ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return SDTP_ERR;
    }

    /* 2. 设置心跳数据 */
    head = (sdtp_header_t *)addr;

    head->type = SDTP_KPALIVE_REQ;
    head->length = 0;
    head->flag = SDTP_SYS_MESG;
    head->checksum = SDTP_CHECK_SUM;

    /* 3. 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr))
    {
        slab_dealloc(ssvr->pool, addr);
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return SDTP_ERR;
    }

    log_debug(ssvr->log, "Add keepalive request success! fd:[%d]", sck->fd);

    sdtp_set_kpalive_stat(sck, SDTP_KPALIVE_STAT_SENT);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_get_self
 **功    能: 获取当前发送线程的上下文
 **输入参数: 
 **     ssvr: 发送服务对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: Address of sndsvr
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static sdtp_ssvr_t *sdtp_ssvr_get_self(sdtp_ssvr_cntx_t *ctx)
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
    return (sdtp_ssvr_t *)(ctx->sendtp->data + tidx * sizeof(sdtp_ssvr_t));
}

/******************************************************************************
 **函数名称: sdtp_ssvr_timeout_hdl
 **功    能: 超时处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断是否长时间无数据通信
 **     2. 发送保活数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int sdtp_ssvr_timeout_hdl(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    time_t curr_tm = time(NULL);
    sdtp_ssvr_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((curr_tm - sck->wrtm) < SDTP_KPALIVE_INTV)
    {
        return SDTP_OK;
    }

    /* 2. 发送保活请求 */
    if (sdtp_ssvr_kpalive_req(ctx, ssvr))
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return SDTP_ERR;
    }

    sck->wrtm = curr_tm;

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_recv_proc
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
static int sdtp_ssvr_recv_proc(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    int n, left;
    sdtp_ssvr_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *recv = &sck->recv;

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
            if (sdtp_ssvr_data_proc(ctx, ssvr, sck))
            {
                log_error(ssvr->log, "Proc data failed! fd:%d", sck->fd);

                CLOSE(sck->fd);
                sdtp_snap_reset(recv);
                return SDTP_ERR;
            }
            continue;
        }
        else if (0 == n)
        {
            log_info(ssvr->log, "Client disconnected. fd:%d n:%d/%d", sck->fd, n, left);
            CLOSE(sck->fd);
            sdtp_snap_reset(recv);
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

        log_error(ssvr->log, "errmsg:[%d] %s. fd:%d", errno, strerror(errno), sck->fd);

        CLOSE(sck->fd);
        sdtp_snap_reset(recv);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_data_proc
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
static int sdtp_ssvr_data_proc(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck)
{
    sdtp_header_t *head;
    uint32_t len, mesg_len;
    sdtp_snap_t *recv = &sck->recv;

    while (1)
    {
        head = (sdtp_header_t *)recv->optr;
        len = (int)(recv->iptr - recv->optr);
        if (len < sizeof(sdtp_header_t))
        {
            goto LEN_NOT_ENOUGH; /* 不足一条数据时 */
        }

        /* 1. 是否不足一条数据 */
        mesg_len = sizeof(sdtp_header_t) + ntohl(head->length);
        if (len < mesg_len)
        {
        LEN_NOT_ENOUGH:
            if (recv->iptr == recv->end) 
            {
                /* 防止OverWrite的情况发生 */
                if ((recv->optr - recv->addr) < (recv->end - recv->iptr))
                {
                    log_error(ssvr->log, "Data length is invalid!");
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
            log_error(ssvr->log, "Header is invalid! CheckSum:%u/%u type:%d len:%d flag:%d",
                    head->checksum, SDTP_CHECK_SUM, head->type, head->length, head->flag);
            return SDTP_ERR;
        }

        /* 2.3 进行数据处理 */
        if (SDTP_SYS_MESG == head->flag)
        {
            sdtp_ssvr_sys_mesg_proc(ssvr, sck, recv->addr);
        }
        else
        {
            sdtp_ssvr_exp_mesg_proc(ssvr, sck, recv->addr);
        }

        recv->optr += mesg_len;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_recv_cmd
 **功    能: 接收命令数据
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务对象
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收命令
 **     2. 处理命令
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int sdtp_ssvr_recv_cmd(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    sdtp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    if (unix_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd)) < 0)
    {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* 2. 处理命令 */
    return sdtp_ssvr_proc_cmd(ctx, ssvr, &cmd);
}

/******************************************************************************
 **函数名称: sdtp_ssvr_proc_cmd
 **功    能: 命令处理
 **输入参数: 
 **     ssvr: 发送服务对象
 **     cmd: 接收到的命令信息
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int sdtp_ssvr_proc_cmd(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr, const sdtp_cmd_t *cmd)
{
    sdtp_ssvr_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case SDTP_CMD_SEND:
        case SDTP_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd))
            {
                return sdtp_ssvr_send_data(ctx, ssvr);
            }
            return SDTP_OK;
        }
        default:
        {
            log_error(ssvr->log, "Unknown command! type:[%d]", cmd->type);
            return SDTP_OK;
        }
    }
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_fill_send_buff
 **功    能: 填充发送缓冲区
 **输入参数: 
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: 
 **返    回: 需要发送的数据长度
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
static int sdtp_ssvr_fill_send_buff(sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck)
{
    uint32_t left, mesg_len;
    sdtp_header_t *head;
    sdtp_snap_t *send = &sck->send[SDTP_SNAP_SYS_DATA];

    /* > 从消息链表取数据 */
    for (;;)
    {
        /* 1. 是否有数据 */
        head = (sdtp_header_t *)list_lpop(ssvr->sck.mesg_list);
        if (NULL == head)
        {
            return (send->iptr - send->optr);
        }

        /* 2. 判断剩余空间 */
        if (SDTP_CHECK_SUM != head->checksum)
        {
            assert(0);
        }

        left = (uint32_t)(send->end - send->iptr);
        mesg_len = sizeof(sdtp_header_t) + head->length;
        if (left < mesg_len)
        {
            list_lpush(ssvr->sck.mesg_list, head);
            break; /* 空间不足 */
        }

        /* 3. 取发送的数据 */
        head->type = htons(head->type);
        head->flag = head->flag;
        head->length = htonl(head->length);
        head->checksum = htonl(head->checksum);

        /* 4. 拷贝至发送缓存 */
        memcpy(send->iptr, (void *)head, mesg_len);

        send->iptr += mesg_len;
    }

    return (send->iptr - send->optr);
}

/******************************************************************************
 **函数名称: sdtp_ssvr_send_data
 **功    能: 发送系统消息
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
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
static int sdtp_ssvr_send_sys_data(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    int n, len;
    sdtp_ssvr_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *send = &sck->send[SDTP_SNAP_SYS_DATA];

    sck->wrtm = time(NULL);

    for (;;)
    {
        /* 1. 填充发送缓存 */
        if (send->iptr == send->optr)
        {
            if ((len = sdtp_ssvr_fill_send_buff(ssvr, sck)) <= 0)
            {
                break;
            }
        }
        else
        {
            len = send->iptr - send->optr;
        }

        /* 2. 发送缓存数据 */
        n = Writen(sck->fd, send->optr, len);
        if (n < 0)
        {
            log_error(ssvr->log, "errmsg:[%d] %s! fd:%d len:[%d]",
                    errno, strerror(errno), sck->fd, len);
            CLOSE(sck->fd);
            sdtp_snap_reset(send);
            return SDTP_ERR;
        }
        /* 只发送了部分数据 */
        else if (n != len)
        {
            send->optr += n;
            return SDTP_OK;
        }

        /* 3. 重置标识量 */
        sdtp_snap_reset(send);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_send_data
 **功    能: 发送扩展消息
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 发送缓存数据
 **     2. 重置标识量
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
static int sdtp_ssvr_send_exp_data(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    int n, len;
    sdtp_ssvr_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *send = &sck->send[SDTP_SNAP_EXP_DATA];

    sck->wrtm = time(NULL);

    /* > 发送缓存数据 */
    len = send->iptr - send->optr;

    n = Writen(sck->fd, send->optr, len);
    if (n < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s! fd:%d len:[%d]",
                errno, strerror(errno), sck->fd, len);
        CLOSE(sck->fd);
        sdtp_snap_reset(send);
        return SDTP_ERR;
    }
    /* 只发送了部分数据 */
    else if (n != len)
    {
        send->optr += n;
        return SDTP_OK;
    }

    /* > 重置标识量 */
    sdtp_snap_reset(send);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_send_data
 **功    能: 发送数据的请求处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **作    者: # Qifeng.zou # 2015.04.11 #
 ******************************************************************************/
static int sdtp_ssvr_send_data(sdtp_ssvr_cntx_t *ctx, sdtp_ssvr_t *ssvr)
{
    sdtp_ssvr_sck_t *sck = &ssvr->sck;

    if (SDTP_SNAP_SYS_DATA == sck->send_type)
    {
        return sdtp_ssvr_send_sys_data(ctx, ssvr);
    }

    return sdtp_ssvr_send_exp_data(ctx, ssvr);
}

/******************************************************************************
 **函数名称: sdtp_ssvr_clear_mesg
 **功    能: 清空发送消息
 **输入参数: 
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次取出每条消息, 并释放所占有的空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static int sdtp_ssvr_clear_mesg(sdtp_ssvr_t *ssvr)
{
    void *data;

    while (1)
    {
        data = list_lpop(ssvr->sck.mesg_list);
        if (NULL == data)
        {
            return SDTP_OK;
        }

        slab_dealloc(ssvr->pool, data);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_sys_mesg_proc
 **功    能: 系统消息的处理
 **输入参数: 
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 根据消息类型调用对应的处理接口
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static int sdtp_ssvr_sys_mesg_proc(sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck, void *addr)
{
    sdtp_header_t *head = (sdtp_header_t *)addr;

    switch (head->type)
    {
        case SDTP_KPALIVE_REP:  /* 保活应答 */
        {
            log_debug(ssvr->log, "Received keepalive respond!");

            sdtp_set_kpalive_stat(sck, SDTP_KPALIVE_STAT_SUCC);
            return SDTP_OK;
        }
        default:
        {
            log_error(ssvr->log, "Unknown type [%d]!", head->type);
            return SDTP_ERR;
        }
    }
    
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_ssvr_exp_mesg_proc
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
static int sdtp_ssvr_exp_mesg_proc(sdtp_ssvr_t *ssvr, sdtp_ssvr_sck_t *sck, void *addr)
{
    return SDTP_OK;
}

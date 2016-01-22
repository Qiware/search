#include "redo.h"
#include "shm_opt.h"
#include "sdtp_cmd.h"
#include "sdsd_cli.h"
#include "sdtp_comm.h"
#include "sdsd_send.h"

/* 静态函数 */
static sdsd_ssvr_t *sdsd_ssvr_get_curr(sdsd_cntx_t *ctx);

static int sdsd_ssvr_creat_sendq(sdsd_ssvr_t *ssvr, const sdsd_conf_t *conf);
static int sdsd_ssvr_creat_usck(sdsd_ssvr_t *ssvr, const sdsd_conf_t *conf);

static int sdsd_ssvr_recv_cmd(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr);
static int sdsd_ssvr_recv_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr);

static int sdsd_ssvr_data_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck);
static int sdsd_ssvr_sys_mesg_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck, void *addr);
static int sdsd_ssvr_exp_mesg_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck, void *addr);

static int sdsd_ssvr_timeout_hdl(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr);
static int sdsd_ssvr_proc_cmd(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, const sdtp_cmd_t *cmd);
static int sdsd_ssvr_send_data(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr);

static int sdsd_ssvr_clear_mesg(sdsd_ssvr_t *ssvr);

static int sdsd_ssvr_kpalive_req(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr);

static int sdtp_link_auth_req(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr);
static int sdtp_link_auth_rsp_hdl(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck, sdtp_link_auth_rsp_t *rsp);

/******************************************************************************
 **函数名称: sdsd_ssvr_init
 **功    能: 初始化发送线程
 **输入参数:
 **     ctx: 全局信息
 **     ssvr: 发送服务对象
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
int sdsd_ssvr_init(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, int idx)
{
    void *addr;
    list_opt_t opt;
    sdsd_conf_t *conf = &ctx->conf;
    sdtp_snap_t *recv = &ssvr->sck.recv;
    sdtp_snap_t *send = &ssvr->sck.send[SDTP_SNAP_SHOT_SYS_DATA];

    ssvr->id = idx;
    ssvr->log = ctx->log;
    ssvr->sck.fd = INVALID_FD;

    /* > 创建发送队列 */
    if (sdsd_ssvr_creat_sendq(ssvr, conf)) {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SDTP_ERR;
    }

    /* > 创建unix套接字 */
    if (sdsd_ssvr_creat_usck(ssvr, conf)) {
        log_error(ssvr->log, "Initialize send queue failed!");
        return SDTP_ERR;
    }

    /* > 创建发送链表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    ssvr->sck.mesg_list = list_creat(&opt);
    if (NULL == ssvr->sck.mesg_list) {
        log_error(ssvr->log, "Create list failed!");
        return SDTP_ERR;
    }

    /* > 初始化发送缓存(注: 程序退出时才可释放此空间，其他任何情况下均不释放) */
    addr = calloc(1, conf->send_buff_size);
    if (NULL == addr) {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    sdtp_snap_setup(send, addr, conf->send_buff_size);

    /* 5. 初始化接收缓存(注: 程序退出时才可释放此空间，其他任何情况下均不释放) */
    addr = calloc(1, conf->recv_buff_size);
    if (NULL == addr) {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    sdtp_snap_setup(recv, addr, conf->recv_buff_size);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_creat_recvq
 **功    能: 创建发送线程的接收队列
 **输入参数:
 **     ssvr: 发送服务对象
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次创建接收队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
static int sdsd_ssvr_creat_recvq(sdsd_cntx_t *ctx, const sdsd_conf_t *conf)
{
    int idx;
    const queue_conf_t *qcf = &conf->recvq;

    /* > 创建对象 */
    ctx->recvq = (queue_t **)calloc(conf->work_thd_num, sizeof(queue_t *));
    if (NULL == ctx->recvq) {
        log_fatal(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 创建队列 */
    for (idx=0; idx<conf->work_thd_num; ++idx) {
        ctx->recvq[idx] = queue_creat(qcf->max, qcf->size);
        if (NULL == ctx->recvq[idx]) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_creat_sendq
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
static int sdsd_ssvr_creat_sendq(sdsd_ssvr_t *ssvr, const sdsd_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN];
    const sdtp_queue_conf_t *qcf = &conf->sendq;

    /* 1. 创建/连接发送队列 */
    snprintf(path, sizeof(path), "%s-%d", qcf->name, ssvr->id);

    ssvr->sendq = sdsd_pool_creat(path, qcf->count, qcf->size);
    if (NULL == ssvr->sendq) {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_creat_usck
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
static int sdsd_ssvr_creat_usck(sdsd_ssvr_t *ssvr, const sdsd_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN];

    sdsd_ssvr_usck_path(conf, path, ssvr->id);

    ssvr->cmd_sck_id = unix_udp_creat(path);
    if (ssvr->cmd_sck_id < 0) {
        log_error(ssvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return SDTP_ERR;
    }

    log_trace(ssvr->log, "cmd_sck_id:[%d] path:%s", ssvr->cmd_sck_id, path);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_bind_cpu
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
static void sdsd_ssvr_bind_cpu(sdsd_cntx_t *ctx, int id)
{
    int idx, mod;
    cpu_set_t cpuset;
    sdtp_cpu_conf_t *cpu = &ctx->conf.cpu;

    mod = sysconf(_SC_NPROCESSORS_CONF) - cpu->start;
    if (mod <= 0) {
        idx = id % sysconf(_SC_NPROCESSORS_CONF);
    }
    else
    {
        idx = cpu->start + (id % mod);
    }

    CPU_ZERO(&cpuset);
    CPU_SET(idx, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

/******************************************************************************
 **函数名称: sdsd_ssvr_switch_send_buff
 **功    能: 切换发送缓存
 **输入参数:
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.11 #
 ******************************************************************************/
void sdsd_ssvr_switch_send_buff(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    sdtp_snap_t *send;
    sdsd_pool_page_t *page;
    sdsd_sck_t *sck = &ssvr->sck;

    /* > 检查是否发送完系统消息 */
    switch (sck->send_type)
    {
        case SDTP_SNAP_SHOT_SYS_DATA:
        {
            send = &sck->send[SDTP_SNAP_SHOT_SYS_DATA];
            if (!list_empty(sck->mesg_list)
                || (send->iptr != send->optr))
            {
                return; /* 系统消息还未发送完成: 不用切换 */
            }
            break;
        }
        case SDTP_SNAP_SHOT_EXP_DATA:
        default:
        {
            /* > 检查是否发送完外部数据 */
            send = &sck->send[SDTP_SNAP_SHOT_EXP_DATA];
            if (send->iptr != send->optr) {
                return; /* 缓存数据仍然未发送完全 */
            }

            if (!list_empty(sck->mesg_list)) {
                sck->send_type = SDTP_SNAP_SHOT_SYS_DATA;
                return; /* 有消息可发送 */
            }
            break;
        }
    }

    page = sdsd_pool_switch(ssvr->sendq);
    if (NULL == page) {
        return; /* 无可发送的数据 */
    }

    sck->send_type = SDTP_SNAP_SHOT_EXP_DATA;
    send = &sck->send[SDTP_SNAP_SHOT_EXP_DATA];

    send->addr = (void *)ssvr->sendq->head + page->begin;
    send->end = send->addr + page->off;
    send->size = page->off;
    send->optr = send->addr;
    send->iptr = send->addr + page->off;

    if (page->off > 1 * GB) {
        assert(0);
    }

#if 0
    int idx;
    sdtp_header_t *head;

    /* > 校验数据合法性(测试数据时使用) */
    head = (sdtp_header_t *)send->addr;
    for (idx=0; idx<page->num; ++idx) {
        if (!SDTP_HEAD_ISVALID(head)) {
            assert(0);
        }
        head = (void *)head + sizeof(sdtp_header_t) + head->length;
    }
#endif

    return;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数:
 **     ssvr: 发送服务对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
void sdsd_ssvr_set_rwset(sdsd_ssvr_t *ssvr)
{
    int idx;
    sdtp_snap_t *snap;

    FD_ZERO(&ssvr->rset);
    FD_ZERO(&ssvr->wset);

    FD_SET(ssvr->cmd_sck_id, &ssvr->rset);

    ssvr->max = MAX(ssvr->cmd_sck_id, ssvr->sck.fd);

    /* 1 设置读集合 */
    FD_SET(ssvr->sck.fd, &ssvr->rset);

    /* 2 设置写集合: 发送至接收端 */
    if (!list_empty(ssvr->sck.mesg_list)) {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
        return;
    }

    snap = &ssvr->sck.send[SDTP_SNAP_SHOT_SYS_DATA];
    for (idx=0; idx<SDTP_SNAP_SHOT_TOTAL; ++idx, ++snap) {
        if (snap->iptr != snap->optr) {
            FD_SET(ssvr->sck.fd, &ssvr->wset);
            return;
        }
    }

    return;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_routine
 **功    能: 发送线程入口函数
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
void *sdsd_ssvr_routine(void *_ctx)
{
    int ret;
    sdsd_sck_t *sck;
    sdsd_ssvr_t *ssvr;
    struct timeval timeout;
    sdsd_cntx_t *ctx = (sdsd_cntx_t *)_ctx;
    sdsd_conf_t *conf = &ctx->conf;

    /* 1. 获取发送线程 */
    ssvr = sdsd_ssvr_get_curr(ctx);
    if (NULL == ssvr) {
        log_fatal(ssvr->log, "Get current thread failed!");
        abort();
        return (void *)-1;
    }

    sck = &ssvr->sck;

    /* 2. 绑定指定CPU */
    sdsd_ssvr_bind_cpu(ctx, ssvr->id);

    /* 3. 进行事件处理 */
    for (;;) {
        /* 3.1 连接合法性判断 */
        if (sck->fd < 0) {
            sdsd_ssvr_clear_mesg(ssvr);

            /* 重连Recv端 */
            if ((sck->fd = tcp_connect(AF_INET, conf->ipaddr, conf->port)) < 0) {
                log_error(ssvr->log, "Conncet receive-server failed!");

                Sleep(SDTP_RECONN_INTV);
                continue;
            }

            sdtp_set_kpalive_stat(sck, SDTP_KPALIVE_STAT_UNKNOWN);
            sdtp_link_auth_req(ctx, ssvr); /* 发起鉴权请求 */
        }

        sdsd_ssvr_switch_send_buff(ctx, ssvr);

        /* 3.2 等待事件通知 */
        sdsd_ssvr_set_rwset(ssvr);

        timeout.tv_sec = SDTP_SSVR_TMOUT_SEC;
        timeout.tv_usec = SDTP_SSVR_TMOUT_USEC;
        ret = select(ssvr->max+1, &ssvr->rset, &ssvr->wset, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            log_fatal(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret) {
            sdsd_ssvr_timeout_hdl(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(sck->fd, &ssvr->wset)) {
            sdsd_ssvr_send_data(ctx, ssvr);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset)) {
            sdsd_ssvr_recv_cmd(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(sck->fd, &ssvr->rset)) {
            sdsd_ssvr_recv_proc(ctx, ssvr);
        }
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_kpalive_req
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
static int sdsd_ssvr_kpalive_req(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    void *addr;
    sdtp_header_t *head;
    int size = sizeof(sdtp_header_t);
    sdsd_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *send = &ssvr->sck.send[SDTP_SNAP_SHOT_SYS_DATA];

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0)
        || (SDTP_KPALIVE_STAT_SENT == sck->kpalive))
    {
        CLOSE(sck->fd);
        sdtp_snap_reset(send);
        log_error(ssvr->log, "Didn't get keepalive respond for a long time!");
        return SDTP_OK;
    }

    addr = (void *)calloc(1, size);
    if (NULL == addr) {
        log_error(ssvr->log, "Alloc memory failed! errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* 2. 设置心跳数据 */
    head = (sdtp_header_t *)addr;

    head->type = SDTP_KPALIVE_REQ;
    head->length = 0;
    head->flag = SDTP_SYS_MESG;
    head->checksum = SDTP_CHECK_SUM;

    /* 3. 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr)) {
        FREE(addr);
        log_error(ssvr->log, "List rpush failed!");
        return SDTP_ERR;
    }

    log_debug(ssvr->log, "Add keepalive request success! fd:[%d]", sck->fd);

    sdtp_set_kpalive_stat(sck, SDTP_KPALIVE_STAT_SENT);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_get_curr
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
static sdsd_ssvr_t *sdsd_ssvr_get_curr(sdsd_cntx_t *ctx)
{
    int id;

    /* 1. 获取线程索引 */
    id = thread_pool_get_tidx(ctx->sendtp);
    if (id < 0) {
        log_error(ctx->log, "Get current thread index failed!");
        return NULL;
    }

    /* 2. 返回线程对象 */
    return (sdsd_ssvr_t *)(ctx->sendtp->data + id * sizeof(sdsd_ssvr_t));
}

/******************************************************************************
 **函数名称: sdsd_ssvr_timeout_hdl
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
static int sdsd_ssvr_timeout_hdl(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    time_t curr_tm = time(NULL);
    sdsd_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((curr_tm - sck->wrtm) < SDTP_KPALIVE_INTV) {
        return SDTP_OK;
    }

    /* 2. 发送保活请求 */
    if (sdsd_ssvr_kpalive_req(ctx, ssvr)) {
        log_error(ssvr->log, "Connection keepalive failed!");
        return SDTP_ERR;
    }

    sck->wrtm = curr_tm;

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_recv_proc
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
static int sdsd_ssvr_recv_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    int n, left;
    sdsd_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *recv = &sck->recv;

    sck->rdtm = time(NULL);

    while (1) {
        /* 1. 接收网络数据 */
        left = (int)(recv->end - recv->iptr);

        n = read(sck->fd, recv->iptr, left);
        if (n > 0) {
            recv->iptr += n;

            /* 2. 进行数据处理 */
            if (sdsd_ssvr_data_proc(ctx, ssvr, sck)) {
                log_error(ssvr->log, "Proc data failed! fd:%d", sck->fd);

                CLOSE(sck->fd);
                sdtp_snap_reset(recv);
                return SDTP_ERR;
            }
            continue;
        }
        else if (0 == n) {
            log_info(ssvr->log, "Client disconnected. fd:%d n:%d/%d", sck->fd, n, left);
            CLOSE(sck->fd);
            sdtp_snap_reset(recv);
            return SDTP_SCK_DISCONN;
        }
        else if ((n < 0) && (EAGAIN == errno)) {
            return SDTP_OK; /* Again */
        }

        if (EINTR == errno) {
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
 **函数名称: sdsd_ssvr_data_proc
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
static int sdsd_ssvr_data_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck)
{
    sdtp_header_t *head;
    uint32_t len, mesg_len;
    sdtp_snap_t *recv = &sck->recv;

    while (1) {
        head = (sdtp_header_t *)recv->optr;
        len = (int)(recv->iptr - recv->optr);
        if (len < sizeof(sdtp_header_t)) {
            goto LEN_NOT_ENOUGH; /* 不足一条数据时 */
        }

        /* 1. 是否不足一条数据 */
        mesg_len = sizeof(sdtp_header_t) + ntohl(head->length);
        if (len < mesg_len) {
        LEN_NOT_ENOUGH:
            if (recv->iptr == recv->end) {
                /* 防止OverWrite的情况发生 */
                if ((recv->optr - recv->addr) < (recv->end - recv->iptr)) {
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
        if (!SDTP_HEAD_ISVALID(head)) {
            ++ssvr->err_total;
            log_error(ssvr->log, "Header is invalid! CheckSum:%u/%u type:%d len:%d flag:%d",
                    head->checksum, SDTP_CHECK_SUM, head->type, head->length, head->flag);
            return SDTP_ERR;
        }

        /* 2.3 进行数据处理 */
        if (SDTP_SYS_MESG == head->flag) {
            sdsd_ssvr_sys_mesg_proc(ctx, ssvr, sck, recv->optr);
        }
        else
        {
            sdsd_ssvr_exp_mesg_proc(ctx, ssvr, sck, recv->optr);
        }

        recv->optr += mesg_len;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_recv_cmd
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
static int sdsd_ssvr_recv_cmd(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    sdtp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    if (unix_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd)) < 0) {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* 2. 处理命令 */
    return sdsd_ssvr_proc_cmd(ctx, ssvr, &cmd);
}

/******************************************************************************
 **函数名称: sdsd_ssvr_proc_cmd
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
static int sdsd_ssvr_proc_cmd(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, const sdtp_cmd_t *cmd)
{
    sdsd_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case SDTP_CMD_SEND:
        case SDTP_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd)) {
                return sdsd_ssvr_send_data(ctx, ssvr);
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
 **函数名称: sdsd_ssvr_fill_send_buff
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
static int sdsd_ssvr_fill_send_buff(sdsd_ssvr_t *ssvr, sdsd_sck_t *sck)
{
    uint32_t left, mesg_len;
    sdtp_header_t *head;
    sdtp_snap_t *send = &sck->send[SDTP_SNAP_SHOT_SYS_DATA];

    /* > 从消息链表取数据 */
    for (;;) {
        /* 1. 是否有数据 */
        head = (sdtp_header_t *)list_lpop(ssvr->sck.mesg_list);
        if (NULL == head) {
            return (send->iptr - send->optr);
        }

        /* 2. 判断剩余空间 */
        if (SDTP_CHECK_SUM != head->checksum) {
            assert(0);
        }

        left = (uint32_t)(send->end - send->iptr);
        mesg_len = sizeof(sdtp_header_t) + head->length;
        if (left < mesg_len) {
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
 **函数名称: sdsd_ssvr_send_data
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
static int sdsd_ssvr_send_sys_data(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    int n, len;
    sdsd_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *send = &sck->send[SDTP_SNAP_SHOT_SYS_DATA];

    sck->wrtm = time(NULL);

    for (;;) {
        /* 1. 填充发送缓存 */
        if (send->iptr == send->optr) {
            if ((len = sdsd_ssvr_fill_send_buff(ssvr, sck)) <= 0) {
                break;
            }
        }
        else
        {
            len = send->iptr - send->optr;
        }

        /* 2. 发送缓存数据 */
        n = Writen(sck->fd, send->optr, len);
        if (n < 0) {
            log_error(ssvr->log, "errmsg:[%d] %s! fd:%d len:[%d]",
                    errno, strerror(errno), sck->fd, len);
            CLOSE(sck->fd);
            sdtp_snap_reset(send);
            return SDTP_ERR;
        }
        /* 只发送了部分数据 */
        else if (n != len) {
            send->optr += n;
            return SDTP_OK;
        }

        /* 3. 重置标识量 */
        sdtp_snap_reset(send);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_send_data
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
static int sdsd_ssvr_send_exp_data(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    int n, len;
    sdsd_sck_t *sck = &ssvr->sck;
    sdtp_snap_t *send = &sck->send[SDTP_SNAP_SHOT_EXP_DATA];

    sck->wrtm = time(NULL);

    /* > 发送缓存数据 */
    len = send->iptr - send->optr;

    n = Writen(sck->fd, send->optr, len);
    if (n < 0) {
        log_error(ssvr->log, "errmsg:[%d] %s! fd:%d len:[%d]",
                errno, strerror(errno), sck->fd, len);
        CLOSE(sck->fd);
        sdtp_snap_reset(send);
        return SDTP_ERR;
    }
    /* 只发送了部分数据 */
    else if (n != len) {
        send->optr += n;
        return SDTP_OK;
    }

    /* > 重置标识量 */
    sdtp_snap_reset(send);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_send_data
 **功    能: 发送数据的请求处理
 **输入参数:
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **作    者: # Qifeng.zou # 2015.04.11 #
 ******************************************************************************/
static int sdsd_ssvr_send_data(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    sdsd_sck_t *sck = &ssvr->sck;

    if (SDTP_SNAP_SHOT_SYS_DATA == sck->send_type) {
        return sdsd_ssvr_send_sys_data(ctx, ssvr);
    }

    return sdsd_ssvr_send_exp_data(ctx, ssvr);
}

/******************************************************************************
 **函数名称: sdsd_ssvr_clear_mesg
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
static int sdsd_ssvr_clear_mesg(sdsd_ssvr_t *ssvr)
{
    void *data;

    while (1) {
        data = list_lpop(ssvr->sck.mesg_list);
        if (NULL == data) {
            return SDTP_OK;
        }

        FREE(data);
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_sys_mesg_proc
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
static int sdsd_ssvr_sys_mesg_proc(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck, void *addr)
{
    sdtp_header_t *head = (sdtp_header_t *)addr;

    switch (head->type)
    {
        case SDTP_KPALIVE_REP:      /* 保活应答 */
        {
            log_debug(ssvr->log, "Received keepalive respond!");

            sdtp_set_kpalive_stat(sck, SDTP_KPALIVE_STAT_SUCC);
            return SDTP_OK;
        }
        case SDTP_LINK_AUTH_REP:    /* 链路鉴权应答 */
        {
            return sdtp_link_auth_rsp_hdl(ctx, ssvr, sck, addr + sizeof(sdtp_header_t));
        }
    }

    log_error(ssvr->log, "Unknown type [%d]!", head->type);
    return SDTP_ERR;
}

/******************************************************************************
 **函数名称: sdsd_ssvr_exp_mesg_proc
 **功    能: 自定义消息的处理
 **输入参数:
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **     sck: 连接对象
 **     addr: 数据地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将自定义消息放入工作队列中, 一次只放入一条数据
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
static int sdsd_ssvr_exp_mesg_proc(
        sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck, void *addr)
{
    void *data;
    int idx, len;
    sdtp_group_t *group;
    sdtp_header_t *head = (sdtp_header_t *)addr;

    ++ssvr->recv_total;

    idx = rand() % ctx->conf.work_thd_num;
    len = SDTP_DATA_TOTAL_LEN(head);

    /* > 申请空间 */
    data = queue_malloc(ctx->recvq[idx], len+sizeof(sdtp_group_t));
    if (NULL == data) {
        ++ssvr->drop_total;
        log_error(ctx->log, "Alloc from queue failed! drop:%lu recv:%lu len:%d/%d",
                ssvr->recv_total, ssvr->drop_total, len+sizeof(sdtp_group_t), queue_size(ctx->recvq[idx]));
        return SDTP_ERR;
    }

    /* > 放入队列 */
    group = (sdtp_group_t *)data;
    group->num = 1; /* 放入一条数据 */
    memcpy(data+sizeof(sdtp_group_t), addr, len);

    queue_push(ctx->recvq[idx], data);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_link_auth_req
 **功    能: 发起链路鉴权请求
 **输入参数:
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将鉴权请求放入发送队列中
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int sdtp_link_auth_req(sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr)
{
    int size;
    void *addr;
    sdtp_header_t *head;
    sdsd_sck_t *sck = &ssvr->sck;
    sdtp_link_auth_req_t *link_auth_req;

    /* > 申请内存空间 */
    size = sizeof(sdtp_header_t) + sizeof(sdtp_link_auth_req_t);

    addr = (void *)calloc(1, size);
    if (NULL == addr) {
        log_error(ssvr->log, "Alloc memory failed! errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    /* > 设置头部数据 */
    head = (sdtp_header_t *)addr;

    head->type = SDTP_LINK_AUTH_REQ;
    head->length = sizeof(sdtp_link_auth_req_t);
    head->flag = SDTP_SYS_MESG;
    head->checksum = SDTP_CHECK_SUM;

    /* > 设置鉴权信息 */
    link_auth_req = addr + sizeof(sdtp_header_t);

    link_auth_req->nodeid = htonl(ctx->conf.nodeid);
    snprintf(link_auth_req->usr, sizeof(link_auth_req->usr), "%s", ctx->conf.auth.usr);
    snprintf(link_auth_req->passwd, sizeof(link_auth_req->passwd), "%s", ctx->conf.auth.passwd);

    /* > 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr)) {
        FREE(addr);
        log_error(ssvr->log, "Alloc memory failed! errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    log_debug(ssvr->log, "Add link auth request success! fd:[%d]", sck->fd);

    return SDTP_OK;
}
/******************************************************************************
 **函数名称: sdtp_link_auth_rsp_hdl
 **功    能: 链路鉴权请求应答的处理
 **输入参数:
 **     ctx: 全局信息
 **     ssvr: 发送服务
 **     sck: 连接对象
 **     addr: 数据地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 判断鉴权成功还是失败
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
static int sdtp_link_auth_rsp_hdl(
        sdsd_cntx_t *ctx, sdsd_ssvr_t *ssvr, sdsd_sck_t *sck, sdtp_link_auth_rsp_t *rsp)
{
    return ntohl(rsp->is_succ)? SDTP_OK : SDTP_ERR;
}

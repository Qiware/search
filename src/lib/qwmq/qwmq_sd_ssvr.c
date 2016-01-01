#include "redo.h"
#include "qwmq_cmd.h"
#include "qwmq_comm.h"
#include "qwmq_sd_send.h"

#define qwsd_ssvr_usck_path(conf, path, id) \
    snprintf(path, sizeof(path), "%s/%d/qwsd-ssvr.%d", conf->path, conf->nodeid, id)

/* 静态函数 */
static qwsd_ssvr_t *qwsd_ssvr_get_curr(qwsd_cntx_t *ctx);

static int qwsd_ssvr_creat_usck(qwsd_ssvr_t *ssvr, const qwsd_conf_t *conf);

static int qwsd_ssvr_recv_cmd(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);
static int qwsd_ssvr_recv_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);

static int qwsd_ssvr_data_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck);
static int qwsd_ssvr_sys_mesg_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck, void *addr);
static int qwsd_ssvr_exp_mesg_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck, void *addr);

static int qwsd_ssvr_timeout_hdl(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);
static int qwsd_ssvr_proc_cmd(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, const qwmq_cmd_t *cmd);
static int qwsd_ssvr_send_data(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);

static int qwsd_ssvr_clear_mesg(qwsd_ssvr_t *ssvr);

static int qwsd_ssvr_kpalive_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);

static int qwmq_link_auth_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);
static int qwmq_link_auth_rsp_hdl(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck, qwmq_link_auth_rsp_t *rsp);

static int qwsd_ssvr_cmd_proc_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, int rqid);
static int qwsd_ssvr_cmd_proc_all_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr);

/******************************************************************************
 **函数名称: qwsd_ssvr_init
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
int qwsd_ssvr_init(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, int idx)
{
    void *addr;
    list_opt_t opt;
    qwsd_conf_t *conf = &ctx->conf;
    qwmq_snap_t *recv = &ssvr->sck.recv;
    qwmq_snap_t *send = &ssvr->sck.send;

    ssvr->id = idx;
    ssvr->log = ctx->log;
    ssvr->sck.fd = INVALID_FD;

    /* > 创建发送队列 */
    ssvr->sendq = ctx->sendq[idx];

    /* > 创建unix套接字 */
    if (qwsd_ssvr_creat_usck(ssvr, conf))
    {
        log_error(ssvr->log, "Initialize send queue failed!");
        return QWMQ_ERR;
    }

    /* > 创建SLAB内存池 */
    ssvr->pool = slab_creat_by_calloc(QWMQ_MEM_POOL_SIZE, ssvr->log);
    if (NULL == ssvr->pool)
    {
        log_error(ssvr->log, "Initialize slab mem-pool failed!");
        return QWMQ_ERR;
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
        return QWMQ_ERR;
    }

    /* > 初始化发送缓存(注: 程序退出时才可释放此空间，其他任何情况下均不释放) */
    addr = calloc(1, conf->send_buff_size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return QWMQ_ERR;
    }

    qwmq_snap_setup(send, addr, conf->send_buff_size);

    /* 5. 初始化接收缓存(注: 程序退出时才可释放此空间，其他任何情况下均不释放) */
    addr = calloc(1, conf->recv_buff_size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return QWMQ_ERR;
    }

    qwmq_snap_setup(recv, addr, conf->recv_buff_size);

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_creat_usck
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
static int qwsd_ssvr_creat_usck(qwsd_ssvr_t *ssvr, const qwsd_conf_t *conf)
{
    char path[FILE_PATH_MAX_LEN];

    qwsd_ssvr_usck_path(conf, path, ssvr->id);

    ssvr->cmd_sck_id = unix_udp_creat(path);
    if (ssvr->cmd_sck_id < 0)
    {
        log_error(ssvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return QWMQ_ERR;
    }

    log_trace(ssvr->log, "cmd_sck_id:[%d] path:%s", ssvr->cmd_sck_id, path);
    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_bind_cpu
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
static void qwsd_ssvr_bind_cpu(qwsd_cntx_t *ctx, int id)
{
    int idx, mod;
    cpu_set_t cpuset;
    qwmq_cpu_conf_t *cpu = &ctx->conf.cpu;

    mod = sysconf(_SC_NPROCESSORS_CONF) - cpu->start;
    if (mod <= 0)
    {
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
 **函数名称: qwsd_ssvr_set_rwset
 **功    能: 设置读写集
 **输入参数:
 **     ssvr: 发送服务对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
void qwsd_ssvr_set_rwset(qwsd_ssvr_t *ssvr)
{
    qwmq_snap_t *snap;

    FD_ZERO(&ssvr->rset);
    FD_ZERO(&ssvr->wset);

    FD_SET(ssvr->cmd_sck_id, &ssvr->rset);

    ssvr->max = MAX(ssvr->cmd_sck_id, ssvr->sck.fd);

    /* 1 设置读集合 */
    FD_SET(ssvr->sck.fd, &ssvr->rset);

    /* 2 设置写集合: 发送至接收端 */
    if (!list_empty(ssvr->sck.mesg_list)
        || !queue_empty(ssvr->sendq))
    {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
        return;
    }

    snap = &ssvr->sck.send;
    if (snap->iptr != snap->optr)
    {
        FD_SET(ssvr->sck.fd, &ssvr->wset);
        return;
    }

    return;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_routine
 **功    能: 发送线程入口函数
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
void *qwsd_ssvr_routine(void *_ctx)
{
    int ret;
    qwsd_sck_t *sck;
    qwsd_ssvr_t *ssvr;
    struct timeval timeout;
    qwsd_cntx_t *ctx = (qwsd_cntx_t *)_ctx;
    qwsd_conf_t *conf = &ctx->conf;

    nice(-20);

    /* 1. 获取发送线程 */
    ssvr = qwsd_ssvr_get_curr(ctx);
    if (NULL == ssvr)
    {
        log_fatal(ssvr->log, "Get current thread failed!");
        abort();
        return (void *)-1;
    }

    sck = &ssvr->sck;

    /* 2. 绑定指定CPU */
    qwsd_ssvr_bind_cpu(ctx, ssvr->id);

    /* 3. 进行事件处理 */
    for (;;)
    {
        /* 3.1 连接合法性判断 */
        if (sck->fd < 0)
        {
            qwsd_ssvr_clear_mesg(ssvr);

            /* 重连Recv端 */
            if ((sck->fd = tcp_connect(AF_INET, conf->ipaddr, conf->port)) < 0)
            {
                log_error(ssvr->log, "Conncet receive-server failed!");

                Sleep(QWMQ_RECONN_INTV);
                continue;
            }

            qwmq_set_kpalive_stat(sck, QWMQ_KPALIVE_STAT_UNKNOWN);
            qwmq_link_auth_req(ctx, ssvr); /* 发起鉴权请求 */
        }

        /* 3.2 等待事件通知 */
        qwsd_ssvr_set_rwset(ssvr);

        timeout.tv_sec = QWMQ_SSVR_TMOUT_SEC;
        timeout.tv_usec = QWMQ_SSVR_TMOUT_USEC;
        ret = select(ssvr->max+1, &ssvr->rset, &ssvr->wset, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno) { continue; }
            log_fatal(ssvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret)
        {
            qwsd_ssvr_timeout_hdl(ctx, ssvr);
            continue;
        }

        /* 发送数据: 发送优先 */
        if (FD_ISSET(sck->fd, &ssvr->wset))
        {
            qwsd_ssvr_send_data(ctx, ssvr);
        }

        /* 接收命令 */
        if (FD_ISSET(ssvr->cmd_sck_id, &ssvr->rset))
        {
            qwsd_ssvr_recv_cmd(ctx, ssvr);
        }

        /* 接收Recv服务的数据 */
        if (FD_ISSET(sck->fd, &ssvr->rset))
        {
            qwsd_ssvr_recv_proc(ctx, ssvr);
        }
    }

    abort();
    return (void *)-1;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_kpalive_req
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
static int qwsd_ssvr_kpalive_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    void *addr;
    qwmq_header_t *head;
    int size = sizeof(qwmq_header_t);
    qwsd_sck_t *sck = &ssvr->sck;
    qwmq_snap_t *send = &ssvr->sck.send;

    /* 1. 上次发送保活请求之后 仍未收到应答 */
    if ((sck->fd < 0)
        || (QWMQ_KPALIVE_STAT_SENT == sck->kpalive))
    {
        CLOSE(sck->fd);
        qwmq_snap_reset(send);
        log_error(ssvr->log, "Didn't get keepalive respond for a long time!");
        return QWMQ_OK;
    }

    addr = slab_alloc(ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return QWMQ_ERR;
    }

    /* 2. 设置心跳数据 */
    head = (qwmq_header_t *)addr;

    head->type = QWMQ_KPALIVE_REQ;
    head->nodeid = ctx->conf.nodeid;
    head->length = 0;
    head->flag = QWMQ_SYS_MESG;
    head->checksum = QWMQ_CHECK_SUM;

    /* 3. 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr))
    {
        slab_dealloc(ssvr->pool, addr);
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return QWMQ_ERR;
    }

    log_debug(ssvr->log, "Add keepalive request success! fd:[%d]", sck->fd);

    qwmq_set_kpalive_stat(sck, QWMQ_KPALIVE_STAT_SENT);
    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_get_curr
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
static qwsd_ssvr_t *qwsd_ssvr_get_curr(qwsd_cntx_t *ctx)
{
    int id;

    /* 1. 获取线程索引 */
    id = thread_pool_get_tidx(ctx->sendtp);
    if (id < 0)
    {
        log_error(ctx->log, "Get current thread index failed!");
        return NULL;
    }

    /* 2. 返回线程对象 */
    return (qwsd_ssvr_t *)(ctx->sendtp->data + id * sizeof(qwsd_ssvr_t));
}

/******************************************************************************
 **函数名称: qwsd_ssvr_timeout_hdl
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
static int qwsd_ssvr_timeout_hdl(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    time_t curr_tm = time(NULL);
    qwsd_sck_t *sck = &ssvr->sck;

    /* 1. 判断是否长时无数据 */
    if ((curr_tm - sck->wrtm) < QWMQ_KPALIVE_INTV)
    {
        return QWMQ_OK;
    }

    /* 2. 发送保活请求 */
    if (qwsd_ssvr_kpalive_req(ctx, ssvr))
    {
        log_error(ssvr->log, "Connection keepalive failed!");
        return QWMQ_ERR;
    }

    sck->wrtm = curr_tm;

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_recv_proc
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
static int qwsd_ssvr_recv_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    int n, left;
    qwsd_sck_t *sck = &ssvr->sck;
    qwmq_snap_t *recv = &sck->recv;

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
            if (qwsd_ssvr_data_proc(ctx, ssvr, sck))
            {
                log_error(ssvr->log, "Proc data failed! fd:%d", sck->fd);

                CLOSE(sck->fd);
                qwmq_snap_reset(recv);
                return QWMQ_ERR;
            }
            continue;
        }
        else if (0 == n)
        {
            log_info(ssvr->log, "Client disconnected. fd:%d n:%d/%d", sck->fd, n, left);
            CLOSE(sck->fd);
            qwmq_snap_reset(recv);
            return QWMQ_SCK_DISCONN;
        }
        else if ((n < 0) && (EAGAIN == errno))
        {
            return QWMQ_OK; /* Again */
        }
        else if (EINTR == errno)
        {
            continue;
        }

        log_error(ssvr->log, "errmsg:[%d] %s. fd:%d", errno, strerror(errno), sck->fd);

        CLOSE(sck->fd);
        qwmq_snap_reset(recv);
        return QWMQ_ERR;
    }

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_data_proc
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
static int qwsd_ssvr_data_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck)
{
    qwmq_header_t *head;
    uint32_t len, mesg_len;
    qwmq_snap_t *recv = &sck->recv;

    while (1)
    {
        head = (qwmq_header_t *)recv->optr;
        len = (int)(recv->iptr - recv->optr);
        if (len < sizeof(qwmq_header_t))
        {
            goto LEN_NOT_ENOUGH; /* 不足一条数据时 */
        }

        /* 1. 是否不足一条数据 */
        mesg_len = sizeof(qwmq_header_t) + ntohl(head->length);
        if (len < mesg_len)
        {
        LEN_NOT_ENOUGH:
            if (recv->iptr == recv->end)
            {
                /* 防止OverWrite的情况发生 */
                if ((recv->optr - recv->addr) < (recv->end - recv->iptr))
                {
                    log_fatal(ssvr->log, "Data length is invalid!");
                    return QWMQ_ERR;
                }

                memcpy(recv->addr, recv->optr, len);
                recv->optr = recv->addr;
                recv->iptr = recv->optr + len;
                return QWMQ_OK;
            }
            return QWMQ_OK;
        }

        /* 2. 至少一条数据时 */
        /* 2.1 转化字节序 */
        head->type = ntohs(head->type);
        head->flag = head->flag;
        head->length = ntohl(head->length);
        head->checksum = ntohl(head->checksum);

        /* 2.2 校验合法性 */
        if (!QWMQ_HEAD_ISVALID(head))
        {
            ++ssvr->err_total;
            log_error(ssvr->log, "Header is invalid! CheckSum:%u/%u type:%d len:%d flag:%d",
                    head->checksum, QWMQ_CHECK_SUM, head->type, head->length, head->flag);
            return QWMQ_ERR;
        }

        /* 2.3 进行数据处理 */
        if (QWMQ_SYS_MESG == head->flag)
        {
            qwsd_ssvr_sys_mesg_proc(ctx, ssvr, sck, recv->optr);
        }
        else
        {
            qwsd_ssvr_exp_mesg_proc(ctx, ssvr, sck, recv->optr);
        }

        recv->optr += mesg_len;
    }

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_recv_cmd
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
static int qwsd_ssvr_recv_cmd(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    qwmq_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    if (unix_udp_recv(ssvr->cmd_sck_id, &cmd, sizeof(cmd)) < 0)
    {
        log_error(ssvr->log, "Recv command failed! errmsg:[%d] %s!", errno, strerror(errno));
        return QWMQ_ERR;
    }

    /* 2. 处理命令 */
    return qwsd_ssvr_proc_cmd(ctx, ssvr, &cmd);
}

/******************************************************************************
 **函数名称: qwsd_ssvr_proc_cmd
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
static int qwsd_ssvr_proc_cmd(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, const qwmq_cmd_t *cmd)
{
    qwsd_sck_t *sck = &ssvr->sck;

    switch (cmd->type)
    {
        case QWMQ_CMD_SEND:
        case QWMQ_CMD_SEND_ALL:
        {
            if (fd_is_writable(sck->fd))
            {
                return qwsd_ssvr_send_data(ctx, ssvr);
            }
            return QWMQ_OK;
        }
        default:
        {
            log_error(ssvr->log, "Unknown command! type:[%d]", cmd->type);
            return QWMQ_OK;
        }
    }
    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_fill_send_buff
 **功    能: 填充发送缓冲区
 **输入参数:
 **     ssvr: 发送服务
 **     sck: 连接对象
 **输出参数:
 **返    回: 需要发送的数据长度
 **实现描述:
 **     1. 从消息链表取数据
 **     2. 从发送队列取数据
 **注意事项: WARNNING: 千万勿将共享变量参与MIN()三目运算, 否则可能出现严重错误!!!!且很难找出原因!
 **          原因: MIN()不是原子运算, 使用共享变量可能导致判断成立后, 而返回时共
 **                享变量的值可能被其他进程或线程修改, 导致出现严重错误!
 **内存结构:
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
static int qwsd_ssvr_fill_send_buff(qwsd_ssvr_t *ssvr, qwsd_sck_t *sck)
{
#define RTSD_POP_NUM    (1024)
    int num, idx;
    void *data[RTSD_POP_NUM];
    uint32_t left, mesg_len;
    qwmq_header_t *head;
    qwmq_snap_t *send = &sck->send;

    /* > 从消息链表取数据 */
    for (;;)
    {
        /* > 是否有数据 */
        head = (qwmq_header_t *)list_lpop(sck->mesg_list);
        if (NULL == head)
        {
            break; /* 无数据 */
        }
        else if (QWMQ_CHECK_SUM != head->checksum)
        {
            assert(0);
        }

        /* > 判断剩余空间 */
        left = (uint32_t)(send->end - send->iptr);
        mesg_len = sizeof(qwmq_header_t) + head->length;
        if (left < mesg_len)
        {
            list_lpush(sck->mesg_list, head);
            break; /* 空间不足 */
        }

        /* > 取发送的数据 */
        head->type = htons(head->type);
        head->nodeid = htonl(head->nodeid);
        head->flag = head->flag;
        head->length = htonl(head->length);
        head->checksum = htonl(head->checksum);

        /* > 拷贝至发送缓存 */
        memcpy(send->iptr, (void *)head, mesg_len);

        send->iptr += mesg_len;

        /* > 释放空间 */
        slab_dealloc(ssvr->pool, (void *)head);
    }

    /* > 从发送队列取数据 */
    for (;;) {
        /* > 判断剩余空间(WARNNING: 勿将共享变量参与三目运算, 否则可能出现严重错误!!!) */
        left = send->end - send->iptr;
         
        num = MIN(left/queue_size(ssvr->sendq), RTSD_POP_NUM);
        num = MIN(num, queue_used(ssvr->sendq));
        if (0 == num)
        {
            break; /* 空间不足 */
        }

        /* > 弹出发送数据 */
        num = queue_mpop(ssvr->sendq, data, num);
        if (0 == num)
        {
            continue;
        }

        log_trace(ssvr->log, "Multi-pop num:%d!", num);

        for (idx=0; idx<num; ++idx)
        {
            /* > 是否有数据 */
            head = (qwmq_header_t *)data[idx];
            if (QWMQ_CHECK_SUM != head->checksum)
            {
                assert(0);
            }

            mesg_len = sizeof(qwmq_header_t) + head->length;

            /* > 设置发送数据 */
            head->type = htons(head->type);
            head->flag = head->flag;
            head->nodeid = htonl(head->nodeid);
            head->length = htonl(head->length);
            head->checksum = htonl(head->checksum);

            /* > 拷贝至发送缓存 */
            memcpy(send->iptr, (void *)head, mesg_len);

            send->iptr += mesg_len;

            /* > 释放空间 */
            queue_dealloc(ssvr->sendq, (void *)head);
        }
    }

    return (send->iptr - send->optr);
}

/******************************************************************************
 **函数名称: qwsd_ssvr_send_data
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
static int qwsd_ssvr_send_data(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    int n, len;
    qwsd_sck_t *sck = &ssvr->sck;
    qwmq_snap_t *send = &sck->send;

    sck->wrtm = time(NULL);

    for (;;)
    {
        /* 1. 填充发送缓存 */
        len = send->iptr - send->optr;
        if (0 == len)
        {
            if ((len = qwsd_ssvr_fill_send_buff(ssvr, sck)) <= 0)
            {
                break;
            }
        }

        /* 2. 发送缓存数据 */
        n = Writen(sck->fd, send->optr, len);
        if (n < 0)
        {
            log_error(ssvr->log, "errmsg:[%d] %s! fd:%d len:[%d]",
                    errno, strerror(errno), sck->fd, len);
            CLOSE(sck->fd);
            qwmq_snap_reset(send);
            return QWMQ_ERR;
        }
        /* 只发送了部分数据 */
        else if (n != len)
        {
            send->optr += n;
            return QWMQ_OK;
        }

        /* 3. 重置标识量 */
        qwmq_snap_reset(send);
    }

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_clear_mesg
 **功    能: 清空发送消息
 **输入参数:
 **     ssvr: 发送服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次取出每条消息, 并释放所占有的空间
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.16 #
 ******************************************************************************/
static int qwsd_ssvr_clear_mesg(qwsd_ssvr_t *ssvr)
{
    void *data;

    while (1) {
        data = list_lpop(ssvr->sck.mesg_list);
        if (NULL == data) {
            return QWMQ_OK;
        }
        slab_dealloc(ssvr->pool, data);
    }

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_sys_mesg_proc
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
static int qwsd_ssvr_sys_mesg_proc(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck, void *addr)
{
    qwmq_header_t *head = (qwmq_header_t *)addr;

    switch (head->type)
    {
        case QWMQ_KPALIVE_RSP:      /* 保活应答 */
        {
            log_debug(ssvr->log, "Received keepalive respond!");

            qwmq_set_kpalive_stat(sck, QWMQ_KPALIVE_STAT_SUCC);
            return QWMQ_OK;
        }
        case QWMQ_LINK_AUTH_RSP:    /* 链路鉴权应答 */
        {
            return qwmq_link_auth_rsp_hdl(ctx, ssvr, sck, addr + sizeof(qwmq_header_t));
        }
    }

    log_error(ssvr->log, "Unknown type [%d]!", head->type);
    return QWMQ_ERR;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_exp_mesg_proc
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
static int qwsd_ssvr_exp_mesg_proc(
        qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, qwsd_sck_t *sck, void *addr)
{
    void *data;
    int idx, len;
    qwmq_header_t *head = (qwmq_header_t *)addr;

    ++ssvr->recv_total;

    /* > 验证长度 */
    len = QWMQ_DATA_TOTAL_LEN(head);
    if ((int)len > queue_size(ctx->recvq[0]))
    {
        ++ssvr->drop_total;
        log_error(ctx->log, "Data is too long! len:%d drop:%lu total:%lu",
                len, ssvr->drop_total, ssvr->recv_total);
        return QWMQ_ERR_TOO_LONG;
    }

   /* > 申请空间 */
    idx = rand() % ctx->conf.work_thd_num;

    data = queue_malloc(ctx->recvq[idx], len);
    if (NULL == data)
    {
        ++ssvr->drop_total;
        log_error(ctx->log, "Alloc from queue failed! drop:%lu recv:%lu size:%d/%d",
                ssvr->drop_total, ssvr->recv_total, len, queue_size(ctx->recvq[idx]));
        return QWMQ_ERR;
    }

    /* > 放入队列 */
    memcpy(data, addr, len);

    if (queue_push(ctx->recvq[idx], data))
    {
        ++ssvr->drop_total;
        log_error(ctx->log, "Push into queue failed! len:%d drop:%lu total:%lu",
                len, ssvr->drop_total, ssvr->recv_total);
        queue_dealloc(ctx->recvq[idx], data);
        return QWMQ_ERR;
    }

    qwsd_ssvr_cmd_proc_req(ctx, ssvr, idx);    /* 发送处理请求 */

    return QWMQ_OK;
}

/******************************************************************************
 **函数名称: qwmq_link_auth_req
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
static int qwmq_link_auth_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    int size;
    void *addr;
    qwmq_header_t *head;
    qwsd_sck_t *sck = &ssvr->sck;
    qwmq_link_auth_req_t *link_auth_req;

    /* > 申请内存空间 */
    size = sizeof(qwmq_header_t) + sizeof(qwmq_link_auth_req_t);

    addr = slab_alloc(ssvr->pool, size);
    if (NULL == addr)
    {
        log_error(ssvr->log, "Alloc memory from slab failed!");
        return QWMQ_ERR;
    }

    /* > 设置头部数据 */
    head = (qwmq_header_t *)addr;

    head->type = QWMQ_LINK_AUTH_REQ;
    head->length = sizeof(qwmq_link_auth_req_t);
    head->flag = QWMQ_SYS_MESG;
    head->checksum = QWMQ_CHECK_SUM;

    /* > 设置鉴权信息 */
    link_auth_req = addr + sizeof(qwmq_header_t);

    link_auth_req->nodeid = htonl(ctx->conf.nodeid);
    snprintf(link_auth_req->usr, sizeof(link_auth_req->usr), "%s", ctx->conf.auth.usr);
    snprintf(link_auth_req->passwd, sizeof(link_auth_req->passwd), "%s", ctx->conf.auth.passwd);

    /* > 加入发送列表 */
    if (list_rpush(sck->mesg_list, addr))
    {
        slab_dealloc(ssvr->pool, addr);
        log_error(ssvr->log, "Insert mesg list failed!");
        return QWMQ_ERR;
    }

    log_debug(ssvr->log, "Add link auth request success! fd:[%d]", sck->fd);

    return QWMQ_OK;
}
/******************************************************************************
 **函数名称: qwmq_link_auth_rsp_hdl
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
static int qwmq_link_auth_rsp_hdl(qwsd_cntx_t *ctx,
        qwsd_ssvr_t *ssvr, qwsd_sck_t *sck, qwmq_link_auth_rsp_t *rsp)
{
    return ntohl(rsp->is_succ)? QWMQ_OK : QWMQ_ERR;
}

/******************************************************************************
 **函数名称: qwsd_ssvr_cmd_proc_req
 **功    能: 发送处理请求
 **输入参数:
 **     ctx: 全局对象
 **     ssvr: 接收服务
 **     rqid: 队列ID(与工作队列ID一致)
 **输出参数: NONE
 **返    回: >0:成功 <=0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.08 #
 ******************************************************************************/
static int qwsd_ssvr_cmd_proc_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr, int rqid)
{
    qwmq_cmd_t cmd;
    char path[FILE_PATH_MAX_LEN];
    qwmq_cmd_proc_req_t *req = (qwmq_cmd_proc_req_t *)&cmd.param;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = QWMQ_CMD_PROC_REQ;
    req->ori_svr_id = ssvr->id;
    req->num = -1;
    req->rqidx = rqid;

    /* > 获取Worker路径 */
    qwsd_worker_usck_path(&ctx->conf, path, rqid);

    /* > 发送处理命令 */
    return unix_udp_send(ssvr->cmd_sck_id, path, &cmd, sizeof(qwmq_cmd_t));
}

/******************************************************************************
 **函数名称: qwsd_ssvr_cmd_proc_all_req
 **功    能: 发送处理请求
 **输入参数:
 **     ctx: 全局对象
 **     ssvr: 接收服务
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 遍历所有接收队列, 并发送处理请求
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.08 #
 ******************************************************************************/
static int qwsd_ssvr_cmd_proc_all_req(qwsd_cntx_t *ctx, qwsd_ssvr_t *ssvr)
{
    int idx;

    for (idx=0; idx<ctx->conf.send_thd_num; ++idx)
    {
        qwsd_ssvr_cmd_proc_req(ctx, ssvr, idx);
    }

    return QWMQ_OK;
}

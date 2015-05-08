/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp.c
 ** 版本号: 1.0
 ** 描  述: 共享消息传输通道(Sharing Message Transaction Channel)
 **         1. 主要用于异步系统之间数据消息的传输
 ** 作  者: # Qifeng.zou # 2014.12.29 #
 ******************************************************************************/

#include "sdtp.h"
#include "syscall.h"
#include "sdtp_cmd.h"
#include "sdtp_priv.h"
#include "thread_pool.h"

/* 静态函数 */
static sdtp_lsn_t *sdtp_listen_init(sdtp_cntx_t *ctx);
static int sdtp_lsn_accept(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn);

static int sdtp_lsn_cmd_core_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn);
static int sdtp_cmd_rand_to_recv(sdtp_cntx_t *ctx, int cmd_sck_id, const sdtp_cmd_t *cmd);
static int sdtp_lsn_cmd_query_conf_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn, sdtp_cmd_t *cmd);
static int sdtp_lsn_cmd_query_recv_stat_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn, sdtp_cmd_t *cmd);
static int sdtp_lsn_cmd_query_proc_stat_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn, sdtp_cmd_t *cmd);

/* 随机选择接收线程 */
#define sdtp_rand_recv(ctx) ((ctx)->listen.total++ % (ctx->recvtp->num))

/******************************************************************************
 **函数名称: sdtp_listen_routine
 **功    能: 启动SDTP侦听线程
 **输入参数: 
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **     1. 初始化侦听
 **     2. 等待请求和命令
 **     3. 接收请求和命令
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
void *sdtp_listen_routine(void *args)
{
#define SDTP_LSN_TMOUT_SEC 30
#define SDTP_LSN_TMOUT_USEC 0
    fd_set rdset;
    int ret, max;
    sdtp_lsn_t *lsn;
    struct timeval timeout;
    sdtp_cntx_t *ctx = (sdtp_cntx_t *)args;

    /* 1. 初始化侦听 */
    lsn = sdtp_listen_init(ctx);
    if (NULL == lsn)
    {
        log_error(ctx->log, "Initialize listen failed!");
        abort();
        return (void *)-1;
    }

    for (;;)
    {
        /* 2. 等待请求和命令 */
        FD_ZERO(&rdset);

        FD_SET(lsn->lsn_sck_id, &rdset);
        FD_SET(lsn->cmd_sck_id, &rdset);

        max = MAX(lsn->lsn_sck_id, lsn->cmd_sck_id);

        timeout.tv_sec = SDTP_LSN_TMOUT_SEC;
        timeout.tv_usec = SDTP_LSN_TMOUT_USEC;
        ret = select(max+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(lsn->log, "errmsg:[%d] %s", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == ret)
        {
            continue;
        }

        /* 3. 接收连接请求 */
        if (FD_ISSET(lsn->lsn_sck_id, &rdset))
        {
            sdtp_lsn_accept(ctx, lsn);
        }

        /* 4. 接收处理命令 */
        if (FD_ISSET(lsn->cmd_sck_id, &rdset))
        {
            sdtp_lsn_cmd_core_hdl(ctx, lsn);
        }
    }

    pthread_exit(NULL);
    return (void *)-1;
}

/******************************************************************************
 **函数名称: sdtp_listen_init
 **功    能: 启动SDTP侦听线程
 **输入参数: 
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 侦听对象
 **实现描述: 
 **     1. 侦听指定端口
 **     2. 创建CMD套接字
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static sdtp_lsn_t *sdtp_listen_init(sdtp_cntx_t *ctx)
{
    char path[FILE_NAME_MAX_LEN];
    sdtp_lsn_t *lsn = &ctx->listen;
    sdtp_conf_t *conf = &ctx->conf;

    lsn->log = ctx->log;

    /* 1. 侦听指定端口 */
    lsn->lsn_sck_id = tcp_listen(ctx->conf.port);
    if (lsn->lsn_sck_id < 0)
    {
        log_error(lsn->log, "Listen special port failed!");
        return NULL;
    }

    /* 2. 创建CMD套接字 */
    sdtp_lsn_usck_path(conf, path);

    lsn->cmd_sck_id = unix_udp_creat(path);
    if (lsn->cmd_sck_id < 0)
    {
        CLOSE(lsn->lsn_sck_id);
        log_error(lsn->log, "Create unix udp socket failed!");
        return NULL;
    }

    return lsn;
}

/******************************************************************************
 **函数名称: sdtp_lsn_destroy
 **功    能: 销毁侦听线程
 **输入参数: 
 **     lsn: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.07 #
 ******************************************************************************/
int sdtp_listen_destroy(sdtp_lsn_t *lsn)
{
    CLOSE(lsn->lsn_sck_id);
    CLOSE(lsn->cmd_sck_id);

    pthread_cancel(lsn->tid);
    return SDTP_OK;
}



/******************************************************************************
 **函数名称: sdtp_listen_accept
 **功    能: 接收连接请求
 **输入参数: 
 **     ctx: 全局对象
 **     lsn: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收连接请求
 **     2. 发送至接收端
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_lsn_accept(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn)
{
    int sckid;
    socklen_t len;
    sdtp_cmd_t cmd;
    struct sockaddr_in cliaddr;
    sdtp_cmd_add_sck_t *args = (sdtp_cmd_add_sck_t *)&cmd.args;

    /* 1. 接收连接请求 */
    for (;;)
    {
        memset(&cliaddr, 0, sizeof(cliaddr));

        len = sizeof(struct sockaddr_in);
        
        sckid = accept(lsn->lsn_sck_id, (struct sockaddr *)&cliaddr, &len);
        if (sckid >= 0)
        {
            log_debug(lsn->log, "New connection! sckid:%d ip:%s",
                    sckid, inet_ntoa(cliaddr.sin_addr));
            break;
        }
        else if (EINTR == errno)
        {
            continue;
        }

        log_error(lsn->log, "errmsg:[%d] %s", errno, strerror(errno));
        return SDTP_ERR;
    }

    fd_set_nonblocking(sckid);

    /* 2. 发送至接收端 */
    memset(&cmd, 0, sizeof(cmd));

    cmd.type = SDTP_CMD_ADD_SCK;
    args->sckid = sckid; 
    snprintf(args->ipaddr, sizeof(args->ipaddr), "%s", inet_ntoa(cliaddr.sin_addr));

    if (sdtp_cmd_rand_to_recv(ctx, lsn->cmd_sck_id, &cmd) < 0)
    {
        CLOSE(sckid);
        log_error(lsn->log, "Send command failed! sckid:[%d]", sckid);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_lsn_cmd_core_hdl
 **功    能: 接收和处理命令
 **输入参数: 
 **     ctx: 全局对象
 **     lsn: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收命令
 **     2. 处理命令
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_lsn_cmd_core_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn)
{
    sdtp_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    /* 1. 接收命令 */
    if (unix_udp_recv(lsn->cmd_sck_id, (void *)&cmd, sizeof(cmd)) < 0)
    {
        log_error(lsn->log, "Recv command failed! errmsg:[%d] %s", errno, strerror(errno));
        return SDTP_ERR_RECV_CMD;
    }

    /* 2. 处理命令 */
    switch (cmd.type)
    {
        case SDTP_CMD_QUERY_CONF_REQ:
        {
            return sdtp_lsn_cmd_query_conf_hdl(ctx, lsn, &cmd);
        }
        case SDTP_CMD_QUERY_RECV_STAT_REQ:
        {
            return sdtp_lsn_cmd_query_recv_stat_hdl(ctx, lsn, &cmd);
        }
        case SDTP_CMD_QUERY_PROC_STAT_REQ:
        {
            return sdtp_lsn_cmd_query_proc_stat_hdl(ctx, lsn, &cmd);
        }
        default:
        {
            log_error(lsn->log, "Unknown command! type:%d", cmd.type);
            return SDTP_ERR_UNKNOWN_CMD;
        }
    }

    return SDTP_ERR_UNKNOWN_CMD;
}

/******************************************************************************
 **函数名称: sdtp_cmd_rand_to_recv
 **功    能: 发送命令到接收线程
 **输入参数: 
 **     ctx: 全局对象
 **     cmd_sck_id: 命令套接字
 **     cmd: 处理命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 随机选择接收线程
 **     2. 发送命令至接收线程
 **注意事项: 如果发送失败，最多重复3次发送!
 **作    者: # Qifeng.zou # 2015.01.09 #
 ******************************************************************************/
static int sdtp_cmd_rand_to_recv(sdtp_cntx_t *ctx, int cmd_sck_id, const sdtp_cmd_t *cmd)
{
    int tidx, times = 0;
    char path[FILE_PATH_MAX_LEN];
    sdtp_conf_t *conf = &ctx->conf;

AGAIN:
    /* 1. 随机选择接收线程 */
    tidx = sdtp_rand_recv(ctx);

    sdtp_rsvr_usck_path(conf, path, tidx);

    /* 2. 发送命令至接收线程 */
    if (unix_udp_send(cmd_sck_id, path, cmd, sizeof(sdtp_cmd_t)) < 0)
    {
        if (times++ < 3)
        {
            goto AGAIN;
        }
        
        log_error(ctx->log, "errmsg:[%d] %s! path:%s type:%d",
                errno, strerror(errno), path, cmd->type);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_lsn_cmd_query_conf_hdl
 **功    能: 查询配置信息
 **输入参数: 
 **     ctx: 全局对象
 **     lsn: 侦听对象
 **     cmd: 处理命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 设置应答参数
 **     2. 发送应答信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_lsn_cmd_query_conf_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn, sdtp_cmd_t *cmd)
{
    sdtp_cmd_t rep;
    sdtp_conf_t *cf = &ctx->conf;
    sdtp_cmd_conf_t *args = (sdtp_cmd_conf_t *)&rep.args;

    memset(&rep, 0, sizeof(rep));

    /* 1. 设置应答信息 */
    rep.type = SDTP_CMD_QUERY_CONF_REP;

    snprintf(args->name, sizeof(args->name), "%s", cf->name);
    args->port = cf->port;
    args->recv_thd_num = cf->recv_thd_num;
    args->work_thd_num = cf->work_thd_num;
    args->rqnum = cf->rqnum;

    args->qmax = cf->recvq.max;
    args->qsize = cf->recvq.size;

    /* 2. 发送应答信息 */
    if (unix_udp_send(lsn->cmd_sck_id, cmd->src_path, &rep, sizeof(rep)) < 0)
    {
        log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_lsn_cmd_query_recv_stat_hdl
 **功    能: 查询接收线程状态
 **输入参数: 
 **     ctx: 全局对象
 **     lsn: 侦听对象
 **     cmd: 处理命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 设置应答信息
 **     2. 发送应答信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_lsn_cmd_query_recv_stat_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn, sdtp_cmd_t *cmd)
{
    int idx;
    sdtp_cmd_t rep;
    sdtp_cmd_recv_stat_t *stat = (sdtp_cmd_recv_stat_t *)&rep.args;
    const sdtp_rsvr_t *rsvr = (const sdtp_rsvr_t *)ctx->recvtp->data;

    for (idx=0; idx<ctx->conf.recv_thd_num; ++idx, ++rsvr)
    {
        /* 1. 设置应答信息 */
        rep.type = SDTP_CMD_QUERY_RECV_STAT_REP;

        stat->connections = rsvr->connections;
        stat->recv_total = rsvr->recv_total;
        stat->drop_total = rsvr->drop_total;
        stat->err_total = rsvr->err_total;

        /* 2. 发送命令信息 */
        if (unix_udp_send(rsvr->cmd_sck_id, cmd->src_path, &rep, sizeof(rep)) < 0)
        {
            log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return SDTP_ERR;
        }
    }
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_lsn_cmd_query_proc_stat_hdl
 **功    能: 查询工作线程状态
 **输入参数: 
 **     ctx: 全局对象
 **     lsn: 侦听对象
 **     cmd: 处理命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 设置应答信息
 **     2. 发送应答信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.30 #
 ******************************************************************************/
static int sdtp_lsn_cmd_query_proc_stat_hdl(sdtp_cntx_t *ctx, sdtp_lsn_t *lsn, sdtp_cmd_t *cmd)
{
    int idx;
    sdtp_cmd_t rep;
    sdtp_cmd_proc_stat_t *stat = (sdtp_cmd_proc_stat_t *)&rep.args;
    const sdtp_worker_t *worker = (sdtp_worker_t *)ctx->worktp->data;

    for (idx=0; idx<ctx->conf.work_thd_num; ++idx, ++worker)
    {
        /* 1. 设置应答信息 */
        rep.type = SDTP_CMD_QUERY_PROC_STAT_REP;

        stat->proc_total = worker->proc_total;
        stat->drop_total = worker->drop_total;
        stat->err_total = worker->err_total;

        /* 2. 发送应答信息 */
        if (unix_udp_send(worker->cmd_sck_id, cmd->src_path, &rep, sizeof(rep)) < 0)
        {
            log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return SDTP_ERR;
        }
    }

    return SDTP_OK;
}
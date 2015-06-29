#include "sck.h"
#include "comm.h"
#include "mesg.h"
#include "search.h"
#include "syscall.h"
#include "command.h"
#include "agent_rsvr.h"
#include "agent_listen.h"

static int agent_listen_accept(agent_cntx_t *ctx, agent_listen_t *lsn);
static int agent_listen_send_add_sck_req(agent_cntx_t *ctx, agent_listen_t *lsn, int idx);

/******************************************************************************
 **函数名称: agent_listen_routine
 **功    能: 运行侦听线程
 **输入参数:
 **     _ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 初始化过程在程序启动时已经完成 - 在子线程中初始化，一旦出现异常将不好处理!
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
void *agent_listen_routine(void *_ctx)
{
    int ret, max;
    fd_set rdset;
    struct timeval tv;
    agent_cntx_t *ctx = (agent_cntx_t *)_ctx;
    agent_listen_t *lsn = ctx->lsn;

    /* > 接收网络连接  */
    while (1)
    {
        FD_ZERO(&rdset);

        FD_SET(lsn->lsn_sck_id, &rdset);
        FD_SET(lsn->cmd_sck_id, &rdset);

        max = MAX(lsn->lsn_sck_id, lsn->cmd_sck_id);

        /* > 等待事件通知 */
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        ret = select(max+1, &rdset, NULL, NULL, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
            continue;
        }
        else if (0 == ret)
        {
            continue;
        }

        /* > 接收网络连接 */
        if (FD_ISSET(lsn->lsn_sck_id, &rdset))
        {
            agent_listen_accept(ctx, lsn);
        }
    }
    return (void *)0;
}

/******************************************************************************
 **函数名称: agent_listen_init
 **功    能: 初始化侦听线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.19 #
 ******************************************************************************/
int agent_listen_init(agent_cntx_t *ctx)
{
    agent_listen_t *lsn;
    char path[FILE_NAME_MAX_LEN];
    agent_conf_t *conf = ctx->conf;

    /* > 创建LSN对象 */
    lsn = (agent_listen_t *)slab_alloc(ctx->slab, sizeof(agent_listen_t));
    if (NULL == lsn)
    {
        log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    lsn->log = ctx->log;

    do
    {
        /* > 侦听指定端口 */
        lsn->lsn_sck_id = tcp_listen(conf->connections.port);
        if (lsn->lsn_sck_id < 0)
        {
            log_error(lsn->log, "errmsg:[%d] %s! port:%d",
                    errno, strerror(errno), ctx->conf->connections.port);
            break;
        }

        /* > 创建命令套接字 */
        agent_lsvr_cmd_usck_path(conf, path, sizeof(path));

        lsn->cmd_sck_id = unix_udp_creat(path);
        if (lsn->cmd_sck_id < 0)
        {
            log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        ctx->lsn = lsn;
        return AGENT_OK;
    } while(0);

    CLOSE(lsn->lsn_sck_id);
    CLOSE(lsn->cmd_sck_id);
    slab_dealloc(ctx->slab, lsn);
    return AGENT_ERR;
}

/******************************************************************************
 **函数名称: agent_listen_accept
 **功    能: 接收连接请求
 **输入参数:
 **     ctx: 全局信息
 **     lsn: 侦听对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 接收连接请求
 **     2. 将通信套接字放入队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.20 #
 ******************************************************************************/
static int agent_listen_accept(agent_cntx_t *ctx, agent_listen_t *lsn)
{
    int fd, idx;
    agent_add_sck_t *add;
    struct sockaddr_in cliaddr;

    /* > 接收连接请求 */
    fd = tcp_accept(lsn->lsn_sck_id, (struct sockaddr *)&cliaddr);
    if (fd < 0)
    {
        log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGENT_ERR;
    }

    ++lsn->serial; /* 计数 */

    /* > 将通信套接字放入队列 */
    idx = lsn->serial % ctx->conf->agent_num;

    add = queue_malloc(ctx->connq[idx], sizeof(agent_add_sck_t));
    if (NULL == add)
    {
        log_error(lsn->log, "Alloc from queue failed! fd:%d size:%d/%d",
                fd, sizeof(agent_add_sck_t), queue_size(ctx->connq[idx]));
        CLOSE(fd);
        return AGENT_ERR;
    }

    add->fd = fd;
    add->serial = lsn->serial;

    log_debug(lsn->log, "Push data! fd:%d addr:%p serial:%ld", fd, add, lsn->serial);

    queue_push(ctx->connq[idx], add);

    /* > 发送ADD-SCK请求 */
    agent_listen_send_add_sck_req(ctx, lsn, idx);

    return AGENT_OK;
}

/******************************************************************************
 **函数名称: agent_listen_send_add_sck_req
 **功    能: 发送ADD-SCK请求
 **输入参数:
 **     ctx: 全局信息
 **     lsn: 侦听对象
 **     idx: 接收服务的索引号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-22 21:47:52 #
 ******************************************************************************/
static int agent_listen_send_add_sck_req(agent_cntx_t *ctx, agent_listen_t *lsn, int idx)
{
    cmd_data_t cmd;
    char path[FILE_NAME_MAX_LEN];
    agent_conf_t *conf = ctx->conf;

    cmd.type = CMD_ADD_SCK;
    agent_rsvr_cmd_usck_path(conf, idx, path, sizeof(path));

    unix_udp_send(lsn->cmd_sck_id, path, &cmd, sizeof(cmd));

    return AGENT_OK;
}

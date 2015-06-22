#include "sck.h"
#include "comm.h"
#include "mesg.h"
#include "search.h"
#include "syscall.h"
#include "agent_rsvr.h"
#include "agent_listen.h"

agent_listen_t *agent_listen_init(agent_cntx_t *ctx);
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
 **     1. 初始化侦听线程
 **     2. 接收网络连接
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
void *agent_listen_routine(void *_ctx)
{
    int ret, max;
    fd_set rdset;
    struct timeval tv;
    agent_listen_t *lsn;
    agent_cntx_t *ctx = (agent_cntx_t *)_ctx;

    /* > 初始化侦听线程 */
    lsn = agent_listen_init(ctx);
    if (NULL == lsn)
    {
        log_error(ctx->log, "Initialize listen thread failed!");
        return (void *)-1;
    }

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
 **返    回: LSN对象
 **实现描述: 
 **     1. 创建LSN对象
 **     2. 侦听指定端口
 **     3. 创建命令套接字
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.19 #
 ******************************************************************************/
agent_listen_t *agent_listen_init(agent_cntx_t *ctx)
{
    agent_listen_t *lsn;

    /* 1. 创建LSN对象 */
    lsn = (agent_listen_t *)calloc(1, sizeof(agent_listen_t));
    if (NULL == lsn)
    {
        log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    lsn->log = ctx->log;

    do
    {
        /* 2. 侦听指定端口 */
        lsn->lsn_sck_id = tcp_listen(ctx->conf->connections.port);
        if (lsn->lsn_sck_id < 0)
        {
            log_error(lsn->log, "errmsg:[%d] %s! port:%d",
                    errno, strerror(errno), ctx->conf->connections.port);
            break;
        }

        /* 3. 创建命令套接字 */
        lsn->cmd_sck_id = unix_udp_creat(AGENT_LSN_CMD_PATH);
        if (lsn->cmd_sck_id < 0)
        {
            log_error(lsn->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        
        return lsn;
    } while(0);

    CLOSE(lsn->lsn_sck_id);
    CLOSE(lsn->cmd_sck_id);
    free(lsn);
    return NULL;
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

    cmd.type = CMD_ADD_SCK;
    snprintf(path, sizeof(path), AGENT_RCV_CMD_PATH, idx);

    unix_udp_send(lsn->cmd_sck_id, path, &cmd, sizeof(cmd));

    return AGENT_OK;
}

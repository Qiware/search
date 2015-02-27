/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_agent.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责对外提供查询、操作的接口
 ** 作  者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"
#include "crawler.h"
#include "crwl_cmd.h"
#include "crwl_agent.h"

static crwl_agent_t *crwl_agt_init(crwl_cntx_t *ctx);
static int crwl_agt_event_hdl(crwl_cntx_t *ctx, crwl_agent_t *agt);

static int crwl_agt_set_reg(crwl_agent_t *agt);
static uint32_t crwl_agt_reg_key_cb(void *_reg, size_t len);
static int crwl_agt_reg_cmp_cb(void *pkey, const void *_reg);

static void crwl_agt_rwset(crwl_agent_t *agt);
static int crwl_agt_cmd_recv(crwl_agent_t *agt);
static int crwl_agt_cmd_send(crwl_agent_t *agt);
static int crwl_cmd_add_seed_req_hdl(int type, void *buff, void *args);
static int crwl_cmd_query_conf_req_hdl(int type, void *buff, void *args);
static int crwl_cmd_query_queue_req_hdl(int type, void *buff, void *args);
static int crwl_cmd_query_worker_req_hdl(int type, void *buff, void *args);

/******************************************************************************
 **函数名称: crwl_agt_routine
 **功    能: 代理运行的接口
 **输入参数: 
 **     _ctx: 全局信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
void *crwl_agt_routine(void *_ctx)
{
    int ret;
    crwl_agent_t *agt;
    fd_set rdset, wrset;
    struct timeval tmout;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;

    /* 1. 初始化代理 */
    agt = crwl_agt_init(ctx);
    if (NULL == agt)
    {
        log_error(ctx->log, "Initialize agent failed!");
        return (void *)-1;
    }

    while (1)
    {
        /* 2. 等待事件通知 */
        crwl_agt_rwset(agt);

        tmout.tv_sec = 30;
        tmout.tv_usec = 0;
        ret = select(agt->fd+1, &rdset, &wrset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                log_warn(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
                continue;
            }

            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return (void *)-1;
        }

        /* 3. 进行事件处理 */
        crwl_agt_event_hdl(ctx, agt);
    }

    return (void *)0;
}

/******************************************************************************
 **函数名称: crwl_agt_init
 **功    能: Initialize agent of crawler
 **输入参数: 
 **     ctx: 全局信息
 **输出参数:
 **返    回: Agent of crawler
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.12 #
 ******************************************************************************/
static crwl_agent_t *crwl_agt_init(crwl_cntx_t *ctx)
{
    crwl_agent_t *agt;
    avl_option_t option;

    /* > 创建对象 */
    agt = (crwl_agent_t *)slab_alloc(ctx->slab, sizeof(crwl_agent_t));
    if (NULL == agt)
    {
        return NULL;
    }

    agt->log = ctx->log;
    agt->slab = ctx->slab;

    /* > 创建AVL树 */
    memset(&option, 0, sizeof(option));

    option.pool = agt->slab;
    option.alloc = (mem_alloc_cb_t)slab_alloc;
    option.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    agt->reg = avl_creat(&option, (key_cb_t)crwl_agt_reg_key_cb, (avl_cmp_cb_t)crwl_agt_reg_cmp_cb);
    if (NULL == agt->reg)
    {
        log_error(agt->log, "Create AVL failed!");
        slab_dealloc(ctx->slab, agt);
        return NULL;
    }

    /* > 注册回调函数 */
    if (crwl_agt_set_reg(agt))
    {
        log_error(agt->log, "Register callback failed!");
        avl_destroy(agt->reg);
        slab_dealloc(ctx->slab, agt);
        return NULL;
    }

    /* > 新建套接字 */
    if (udp_listen(CRWL_AGT_PORT))
    {
        log_error(agt->log, "Listen special port [%d] failed!", CRWL_AGT_PORT);
        avl_destroy(agt->reg);
        slab_dealloc(ctx->slab, agt);
        return NULL;
    }

    return agt;
}

/******************************************************************************
 **函数名称: crwl_agt_rwset
 **功    能: 设置读写集合
 **输入参数: 
 **     agt: 代理对象
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **     使用FD_ZERO() FD_SET()等接口
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.16 #
 ******************************************************************************/
static void crwl_agt_rwset(crwl_agent_t *agt)
{
    FD_ZERO(&agt->rdset);
    FD_ZERO(&agt->wrset);

    FD_SET(agt->fd, &agt->rdset);

    if (NULL != agt->mesg_list.head)
    {
        FD_SET(agt->fd, &agt->wrset);
    }

    return;
}

/******************************************************************************
 **函数名称: crwl_agt_register
 **功    能: 注册回调函数
 **输入参数: 
 **     agt: 代理对象
 **     type: 命令类型
 **     proc: 回调函数
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static int crwl_agt_register(crwl_agent_t *agt, int type, crwl_agt_reg_cb_t proc, void *args)
{
    int ret;
    crwl_agt_reg_t *reg;

    /* 申请空间 */
    reg = slab_alloc(agt->slab, sizeof(crwl_agt_reg_t));
    if (NULL == reg)
    {
        log_error(agt->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    /* 设置数据 */
    reg->type = type;
    reg->proc = proc;
    reg->args = NULL;

    /* 插入平衡二叉树 */
    reg->type = type;

    if (0 != (ret = avl_insert(agt->reg, &type, sizeof(type), reg)))
    {
        log_error(agt->log, "Register failed! ret:%d", ret);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_agt_set_reg
 **功    能: 设置命令处理函数
 **输入参数: 
 **     agt: 代理对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static int crwl_agt_set_reg(crwl_agent_t *agt)
{
#define CRWL_CHECK(ret) \
    if (0 != ret) \
    { \
        return CRWL_ERR; \
    }

    /* 注册回调函数 */
    CRWL_CHECK(crwl_agt_register(agt, CRWL_CMD_ADD_SEED_REQ,
                (crwl_agt_reg_cb_t)crwl_cmd_add_seed_req_hdl, agt));
    CRWL_CHECK(crwl_agt_register(agt, CRWL_CMD_QUERY_CONF_REQ,
                (crwl_agt_reg_cb_t)crwl_cmd_query_conf_req_hdl, agt));
    CRWL_CHECK(crwl_agt_register(agt, CRWL_CMD_QUERY_QUEUE_REQ,
                (crwl_agt_reg_cb_t)crwl_cmd_query_queue_req_hdl, agt));
    CRWL_CHECK(crwl_agt_register(agt, CRWL_CMD_QUERY_WORKER_REQ,
                (crwl_agt_reg_cb_t)crwl_cmd_query_worker_req_hdl, agt));

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_agt_reg_key_cb
 **功    能: 生成KEY的函数
 **输入参数: 
 **     _reg: 注册数据
 **     len: 注册数据长度
 **输出参数:
 **返    回: KEY值
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static uint32_t crwl_agt_reg_key_cb(void *_reg, size_t len)
{
    crwl_agt_reg_t *reg = (crwl_agt_reg_t *)_reg;

    return reg->type;
}

/******************************************************************************
 **函数名称: crwl_agt_reg_cmp_cb
 **功    能: 比较KEY的函数
 **输入参数: 
 **     agt: 代理对象
 **     _reg: 注册的数据信息(类型: crwl_agt_reg_t)
 **输出参数:
 **返    回: =0:相等 <0:小于 >0:大于
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static int crwl_agt_reg_cmp_cb(void *pkey, const void *_reg)
{
    const crwl_agt_reg_t *reg = (const crwl_agt_reg_t *)_reg;

    return (*(uint32_t *)pkey == reg->type);
}

/******************************************************************************
 **函数名称: crwl_agt_event_hdl
 **功    能: 命令处理
 **输入参数: 
 **     ctx: 全局信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 获取命令
 **     2. 查询回调
 **     3. 执行回调
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_agt_event_hdl(crwl_cntx_t *ctx, crwl_agent_t *agt)
{
    /* > 接收命令 */
    if (FD_ISSET(agt->fd, &agt->rdset))
    {
        if (crwl_agt_cmd_recv(agt))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return CRWL_ERR;
        }
    }

    /* > 发送数据 */
    if (FD_ISSET(agt->fd, &agt->wrset))
    {
        if (crwl_agt_cmd_send(agt))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return CRWL_ERR;
        }
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_agt_cmd_recv
 **功    能: 接收命令数据
 **输入参数: 
 **     agt: 代理对象
 **     buff: 缓存空间
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     接收命令，并对命令类型和数据做相应的处理.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_agt_cmd_recv(crwl_agent_t *agt)
{
    ssize_t n;
    crwl_cmd_t cmd;
    avl_node_t *node;
    socklen_t addrlen;
    crwl_agt_reg_t *reg;
    struct sockaddr_un from;

    /* > 接收命令 */
    memset(&from, 0, sizeof(struct sockaddr_un));

    from.sun_family = AF_INET;

    n = recvfrom(agt->fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
    if (n < 0)
    {
        log_error(agt->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    /* > 查找回调 */
    node = avl_query(agt->reg, (void *)&cmd.type, sizeof(cmd.type));
    if (NULL == node)
    {
        log_error(ctx->log, "Didn't register callback for type [%d]!", cmd.type);
        return CRWL_ERR;
    }

    reg = (crwl_agt_reg_t *)node->data;

    /* > 执行回调 */
    return reg->proc(cmd->type, cmd.data, reg->args);
}

/******************************************************************************
 **函数名称: crwl_cmd_add_seed_req_hdl
 **功    能: 添加爬虫种子
 **输入参数: 
 **     type: 命令类型
 **     seed: 种子信息
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_cmd_add_seed_req_hdl(int type, void *buff, void *args)
{
    crwl_agent_t *agt = (crwl_agent_t *)args;
    crwl_cmd_add_seed_t *seed = (crwl_cmd_add_seed_t *)buff;

    log_debug(agt->log, "Call %s(). Url:%s", __func__, seed->url);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_cmd_query_conf_req_hdl
 **功    能: 查询配置信息
 **输入参数: 
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_cmd_query_conf_req_hdl(int type, void *buff, void *args)
{
    crwl_cmd_conf_resp_t *conf;
    crwl_agent_t *agt = (crwl_agent_t *)args;

    log_debug(agt->log, "Call %s()!", __func__);

    conf = slab_slab(agt->slab, sizeof(crwl_cmd_conf_resp_t));
    if (NULL == conf)
    {
        log_error(agt->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    conf->sched_num = 2;
    conf->worker_num = 2;

    if (list_insert(&agt->mesg_list, conf))
    {
        log_error(agt->log, "Insert list failed!");
        slab_dealloc(agt->slab, conf);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_cmd_query_worker_req_hdl
 **功    能: 查询爬虫信息
 **输入参数: 
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_cmd_query_worker_req_hdl(int type, void *buff, void *args)
{
    log_debug(agt->log, "Call %s()!", __func__);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_cmd_query_queue_req_hdl
 **功    能: 查询队列信息
 **输入参数: 
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_cmd_query_queue_req_hdl(int type, void *buff, void *args)
{
    log_debug(agt->log, "Call %s()!", __func__);
    return CRWL_OK;
}

static int crwl_agt_cmd_hdl(crwl_agent_t *agt)
{
    log_debug(agt->log, "Call %s()!", __func__);
    return CRWL_OK;
}

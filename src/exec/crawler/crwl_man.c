/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_man.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责对外提供查询、操作的接口
 ** 作  者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/

#include "common.h"
#include "syscall.h"
#include "crawler.h"
#include "crwl_cmd.h"
#include "crwl_man.h"
#include "crwl_conf.h"
#include "crwl_worker.h"

/* 命令应答信息 */
typedef struct
{
    crwl_cmd_t cmd;
    struct sockaddr_un to;
} crwl_cmd_item_t;

/* 静态函数 */
static crwl_man_t *crwl_man_init(crwl_cntx_t *ctx);
static int crwl_man_event_hdl(crwl_cntx_t *ctx, crwl_man_t *man);

static int crwl_man_reg_cb(crwl_man_t *man);
static uint32_t crwl_man_reg_key_cb(void *_reg, size_t len);
static int crwl_man_reg_cmp_cb(void *type, const void *_reg);

static int crwl_man_recv_cmd(crwl_cntx_t *ctx, crwl_man_t *man);
static int crwl_man_send_cmd(crwl_cntx_t *ctx, crwl_man_t *man);

/******************************************************************************
 **函数名称: crwl_man_rwset
 **功    能: 设置读写集合
 **输入参数: 
 **     man: 管理对象
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **     使用FD_ZERO() FD_SET()等接口
 **注意事项: 
 **     当链表为空时, 则不用加入可写集合
 **作    者: # Qifeng.zou # 2015.02.16 #
 ******************************************************************************/
#define crwl_man_rwset(man) \
{ \
    FD_ZERO(&man->rdset); \
    FD_ZERO(&man->wrset); \
 \
    FD_SET(man->fd, &man->rdset); \
 \
    if (!list_isempty(man->mesg_list)) \
    { \
        FD_SET(man->fd, &man->wrset); \
    } \
}

/******************************************************************************
 **函数名称: crwl_manager_routine
 **功    能: 代理运行的接口
 **输入参数: 
 **     _ctx: 全局信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
void *crwl_manager_routine(void *_ctx)
{
    int ret;
    crwl_man_t *man;
    struct timeval tmout;
    crwl_cntx_t *ctx = (crwl_cntx_t *)_ctx;

    /* 1. 初始化代理 */
    man = crwl_man_init(ctx);
    if (NULL == man)
    {
        log_error(ctx->log, "Initialize agent failed!");
        return (void *)-1;
    }

    while (1)
    {
        /* 2. 等待事件通知 */
        crwl_man_rwset(man);

        tmout.tv_sec = 30;
        tmout.tv_usec = 0;
        ret = select(man->fd+1, &man->rdset, &man->wrset, NULL, &tmout);
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
        crwl_man_event_hdl(ctx, man);
    }

    return (void *)0;
}

/******************************************************************************
 **函数名称: crwl_man_init
 **功    能: 初始化代理
 **输入参数: 
 **     ctx: 全局信息
 **输出参数:
 **返    回: 管理对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.12 #
 ******************************************************************************/
static crwl_man_t *crwl_man_init(crwl_cntx_t *ctx)
{
    crwl_man_t *man;
    avl_option_t option;
    list_option_t list_option;

    /* > 创建对象 */
    man = (crwl_man_t *)slab_alloc(ctx->slab, sizeof(crwl_man_t));
    if (NULL == man)
    {
        return NULL;
    }

    memset(man, 0, sizeof(crwl_man_t));

    man->log = ctx->log;
    man->fd = INVALID_FD;
    man->slab = ctx->slab;

    do
    {
        /* > 创建AVL树 */
        memset(&option, 0, sizeof(option));

        option.pool = man->slab;
        option.alloc = (mem_alloc_cb_t)slab_alloc;
        option.dealloc = (mem_dealloc_cb_t)slab_dealloc;

        man->reg = avl_creat(&option, (key_cb_t)crwl_man_reg_key_cb, (avl_cmp_cb_t)crwl_man_reg_cmp_cb);
        if (NULL == man->reg)
        {
            log_error(man->log, "Create AVL failed!");
            return NULL;
        }

        /* > 创建链表 */
        memset(&list_option, 0, sizeof(list_option));

        list_option.pool = man->slab;
        list_option.alloc = (mem_alloc_cb_t)slab_alloc;
        list_option.dealloc = (mem_dealloc_cb_t)slab_dealloc;

        man->mesg_list = list_creat(&list_option);
        if (NULL == man->mesg_list)
        {
            log_error(man->log, "Create list failed!");
            break;
        }

        /* > 注册回调函数 */
        if (crwl_man_reg_cb(man))
        {
            log_error(man->log, "Register callback failed!");
            break;
        }

        /* > 新建套接字 */
        man->fd = udp_listen(ctx->conf->man_port);
        if (man->fd < 0)
        {
            log_error(man->log, "Listen port [%d] failed!", ctx->conf->man_port);
            break;
        }
        return man;
    } while(0);

    /* > 释放空间 */
    if (NULL != man->reg)
    {
        avl_destroy(man->reg);
    }
    if (NULL != man->mesg_list)
    {
        list_destroy(man->mesg_list);
    }
    Close(man->fd);
    slab_dealloc(ctx->slab, man);

    return NULL;
}

/******************************************************************************
 **函数名称: crwl_man_register
 **功    能: 注册回调函数
 **输入参数: 
 **     man: 管理对象
 **     type: 命令类型
 **     proc: 回调函数
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static int crwl_man_register(crwl_man_t *man, int type, crwl_man_reg_cb_t proc, void *args)
{
    int ret;
    crwl_man_reg_t *reg;

    /* > 申请空间 */
    reg = slab_alloc(man->slab, sizeof(crwl_man_reg_t));
    if (NULL == reg)
    {
        log_error(man->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    /* > 设置数据 */
    reg->type = type;
    reg->proc = proc;
    reg->args = NULL;

    /* > 插入平衡二叉树 */
    reg->type = type;

    ret = avl_insert(man->reg, &type, sizeof(type), reg);
    if (0 != ret)
    {
        log_error(man->log, "Register failed! ret:%d", ret);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_man_reg_key_cb
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
static uint32_t crwl_man_reg_key_cb(void *_reg, size_t len)
{
    crwl_man_reg_t *reg = (crwl_man_reg_t *)_reg;

    return reg->type;
}

/******************************************************************************
 **函数名称: crwl_man_reg_cmp_cb
 **功    能: 比较KEY的函数
 **输入参数: 
 **     man: 管理对象
 **     _reg: 注册的数据信息(类型: crwl_man_reg_t)
 **输出参数:
 **返    回: =0:相等 <0:小于 >0:大于
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static int crwl_man_reg_cmp_cb(void *type, const void *_reg)
{
    const crwl_man_reg_t *reg = (const crwl_man_reg_t *)_reg;

    return (*(uint32_t *)type - reg->type);
}

/******************************************************************************
 **函数名称: crwl_man_event_hdl
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
static int crwl_man_event_hdl(crwl_cntx_t *ctx, crwl_man_t *man)
{
    /* > 接收命令 */
    if (FD_ISSET(man->fd, &man->rdset))
    {
        if (crwl_man_recv_cmd(ctx, man))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return CRWL_ERR;
        }
    }

    /* > 发送数据 */
    if (FD_ISSET(man->fd, &man->wrset))
    {
        if (crwl_man_send_cmd(ctx, man))
        {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return CRWL_ERR;
        }
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_man_recv_cmd
 **功    能: 接收命令数据
 **输入参数: 
 **     man: 管理对象
 **     buff: 缓存空间
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     接收命令，并对命令类型和数据做相应的处理.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_man_recv_cmd(crwl_cntx_t *ctx, crwl_man_t *man)
{
    ssize_t n;
    crwl_cmd_t cmd;
    avl_node_t *node;
    socklen_t addrlen;
    crwl_man_reg_t *reg;
    struct sockaddr_un from;

    /* > 接收命令 */
    memset(&from, 0, sizeof(from));

    from.sun_family = AF_INET;
    addrlen = sizeof(from);

    n = recvfrom(man->fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
    if (n < 0)
    {
        log_error(man->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return CRWL_ERR;
    }

    cmd.type = ntohl(cmd.type);

    /* > 查找回调 */
    node = avl_query(man->reg, (void *)&cmd.type, sizeof(cmd.type));
    if (NULL == node)
    {
        log_error(man->log, "Didn't register callback for type [%d]!", cmd.type);
        return CRWL_ERR;
    }

    reg = (crwl_man_reg_t *)node->data;

    /* > 执行回调 */
    return reg->proc(ctx, man, cmd.type, (void *)&cmd.data, &from, reg->args);
}

/******************************************************************************
 **函数名称: crwl_man_send_cmd
 **功    能: 发送应答数据
 **输入参数: 
 **     man: 管理对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     记得释放链表数据的空间，否则存在内存泄露的问题
 **作    者: # Qifeng.zou # 2015.02.28 #
 ******************************************************************************/
static int crwl_man_send_cmd(crwl_cntx_t *ctx, crwl_man_t *man)
{
    int n;
    crwl_cmd_item_t *item;

    /* > 弹出数据 */
    item = list_lpop(man->mesg_list);
    if (NULL == item)
    {
        log_error(man->log, "Didn't pop data from list!");
        return CRWL_ERR;
    }

    /* > 发送命令 */
    n = sendto(man->fd, &item->cmd, sizeof(crwl_cmd_t), 0, (struct sockaddr *)&item->to, sizeof(item->to));
    if (n < 0)
    {
        log_error(man->log, "errmsg:[%d] %s!", errno, strerror(errno));
        slab_dealloc(man->slab, item);
        return CRWL_OK;
    }

    /* > 释放空间 */
    slab_dealloc(man->slab, item);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_man_query_conf_req_hdl
 **功    能: 查询配置信息
 **输入参数: 
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int crwl_man_query_conf_req_hdl(crwl_cntx_t *ctx,
        crwl_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    crwl_cmd_t *cmd;
    crwl_cmd_item_t *item;
    crwl_cmd_conf_t *conf;

    log_debug(man->log, "Call %s()!", __func__);

    /* > 申请应答空间 */
    item = slab_alloc(man->slab, sizeof(crwl_cmd_item_t));
    if (NULL == item)
    {
        log_error(man->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置应答信息 */
    cmd = &item->cmd;
    conf = (crwl_cmd_conf_t *)&cmd->data;

    cmd->type = htonl(CRWL_CMD_QUERY_CONF_RESP);

    conf->log.level = htonl(ctx->conf->log.level);      /* 日志级别 */
    conf->log.syslevel = htonl(ctx->conf->log.syslevel);    /* 系统日志级别 */

    conf->download.depth = htonl(ctx->conf->download.depth);/* 最大爬取深度 */
    snprintf(conf->download.path, sizeof(conf->download.path), "%s", ctx->conf->download.path); /* 网页存储路径 */

    conf->workq_count = htonl(ctx->conf->workq_count);  /* 工作队列容量 */

    conf->worker.num = htonl(ctx->conf->worker.num);    /* 爬虫总数 */
    conf->worker.conn_max_num = htonl(ctx->conf->worker.conn_max_num); /* 最大并发量 */
    conf->worker.conn_tmout_sec = htonl(ctx->conf->worker.conn_tmout_sec); /* 超时时间 */

    /* > 加入应答列表 */
    if (list_rpush(man->mesg_list, item))
    {
        log_error(man->log, "Insert list failed!");
        slab_dealloc(man->slab, conf);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_man_query_worker_stat_req_hdl
 **功    能: 查询爬虫信息
 **输入参数: 
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int crwl_man_query_worker_stat_req_hdl(crwl_cntx_t *ctx,
        crwl_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    int idx;
    crwl_cmd_t *cmd;
    crwl_worker_t *worker;
    crwl_cmd_item_t *item;
    crwl_cmd_worker_stat_t *stat;
    crwl_conf_t *conf = ctx->conf;

    /* > 新建应答 */
    item = slab_alloc(man->slab, sizeof(crwl_cmd_item_t));
    if (NULL == item)
    {
        log_error(man->log, "Alloc from slab failed!");
        return CRWL_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置信息 */
    cmd = &item->cmd;
    stat = (crwl_cmd_worker_stat_t *)&cmd->data;

    cmd->type = htonl(CRWL_CMD_QUERY_WORKER_STAT_RESP);

    /* 1. 获取启动时间 */
    stat->stm = htonl(ctx->run_tm);
    stat->ctm = htonl(time(NULL));

    /* 2. 获取工作状态 */
    for (idx=0; idx<conf->worker.num && idx<CRWL_CMD_WORKER_MAX_NUM; ++idx)
    {
        worker = crwl_worker_get_by_idx(ctx, idx);

        stat->worker[idx].connections = htonl(worker->sock_list->num);
        stat->worker[idx].down_webpage_total = hton64(worker->down_webpage_total);
        stat->worker[idx].err_webpage_total = hton64(worker->err_webpage_total);
        ++stat->num;
    }

    stat->num = htonl(stat->num);

    /* > 放入队尾 */
    if (list_rpush(man->mesg_list, item))
    {
        log_error(man->log, "Push into list failed!");
        slab_dealloc(man->slab, item);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_man_query_workq_stat_req_hdl
 **功    能: 查询工作队列信息
 **输入参数: 
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.01 #
 ******************************************************************************/
static int crwl_man_query_workq_stat_req_hdl(crwl_cntx_t *ctx,
        crwl_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    int idx;
    queue_t *workq;
    crwl_cmd_t *cmd;
    crwl_cmd_item_t *item;
    crwl_cmd_workq_stat_t *stat;

    /* > 新建应答 */
    item = slab_alloc(man->slab, sizeof(crwl_cmd_item_t));
    if (NULL == item)
    {
        log_error(man->log, "Alloc from slab failed!");
        return CRWL_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置信息 */
    cmd = &item->cmd;
    stat = (crwl_cmd_workq_stat_t *)&cmd->data;

    cmd->type = htonl(CRWL_CMD_QUERY_WORKQ_STAT_RESP);

    for (idx=0; idx<ctx->conf->worker.num; ++idx)
    {
        workq = ctx->workq[idx];

        snprintf(stat->queue[idx].name, sizeof(stat->queue[idx].name), "WORKQ-%02d", idx+1);
        stat->queue[idx].num = htonl(workq->queue.num);
        stat->queue[idx].max = htonl(workq->queue.max);

        ++stat->num;
    }

    stat->num = htonl(stat->num);

    /* > 放入队尾 */
    if (list_rpush(man->mesg_list, item))
    {
        log_error(man->log, "Push into list failed!");
        slab_dealloc(man->slab, item);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_man_reg_cb
 **功    能: 设置命令处理函数
 **输入参数: 
 **     man: 管理对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.11 #
 ******************************************************************************/
static int crwl_man_reg_cb(crwl_man_t *man)
{
#define CRWL_REG(man, type, proc, args) \
    if (crwl_man_register(man, type, proc, args)) \
    { \
        return CRWL_ERR; \
    }

    /* 注册回调函数 */
    CRWL_REG(man, CRWL_CMD_QUERY_CONF_REQ, (crwl_man_reg_cb_t)crwl_man_query_conf_req_hdl, man);
    CRWL_REG(man, CRWL_CMD_QUERY_WORKER_STAT_REQ, (crwl_man_reg_cb_t)crwl_man_query_worker_stat_req_hdl, man);
    CRWL_REG(man, CRWL_CMD_QUERY_WORKQ_STAT_REQ, (crwl_man_reg_cb_t)crwl_man_query_workq_stat_req_hdl, man);

    return CRWL_OK;
}

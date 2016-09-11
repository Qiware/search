/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: flt_man.c
 ** 版本号: 1.0
 ** 描  述: 过滤模块
 **         负责对外提供查询、操作的接口
 ** 作  者: # Qifeng.zou # 2015.03.15 #
 ******************************************************************************/
#include "comm.h"
#include "redo.h"
#include "filter.h"
#include "flt_cmd.h"
#include "flt_man.h"
#include "flt_conf.h"
#include "flt_worker.h"

/* 命令应答信息 */
typedef struct
{
    flt_cmd_t cmd;
    struct sockaddr_un to;
} flt_cmd_item_t;

/* 静态函数 */
static flt_man_t *flt_man_init(flt_cntx_t *ctx);
static int flt_man_event_hdl(flt_cntx_t *ctx, flt_man_t *man);

static int flt_man_reg_cb(flt_man_t *man);

static int flt_man_recv_cmd(flt_cntx_t *ctx, flt_man_t *man);
static int flt_man_send_cmd(flt_cntx_t *ctx, flt_man_t *man);

/******************************************************************************
 **函数名称: flt_man_rwset
 **功    能: 设置读写集合
 **输入参数:
 **     man: 管理对象
 **输出参数:
 **返    回: VOID
 **实现描述:
 **     使用FD_ZERO() FD_SET()等接口
 **注意事项: 当链表为空时, 则不用加入可写集合
 **作    者: # Qifeng.zou # 2015.03.15 #
 ******************************************************************************/
#define flt_man_rwset(man) \
{ \
    FD_ZERO(&man->rdset); \
    FD_ZERO(&man->wrset); \
 \
    FD_SET(man->fd, &man->rdset); \
 \
    if (!list_empty(man->mesg_list)) { \
        FD_SET(man->fd, &man->wrset); \
    } \
}

/******************************************************************************
 **函数名称: flt_manager_routine
 **功    能: 代理运行的接口
 **输入参数:
 **     _ctx: 全局信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.15 #
 ******************************************************************************/
void *flt_manager_routine(void *_ctx)
{
    int ret;
    flt_man_t *man;
    struct timeval tmout;
    flt_cntx_t *ctx = (flt_cntx_t *)_ctx;

    /* 1. 初始化代理 */
    man = flt_man_init(ctx);
    if (NULL == man) {
        log_error(ctx->log, "Initialize agent failed!");
        return (void *)-1;
    }

    while (1) {
        /* 2. 等待事件通知 */
        flt_man_rwset(man);

        tmout.tv_sec = 30;
        tmout.tv_usec = 0;
        ret = select(man->fd+1, &man->rdset, &man->wrset, NULL, &tmout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return (void *)-1;
        }

        /* 3. 进行事件处理 */
        flt_man_event_hdl(ctx, man);
    }

    return (void *)0;
}

/******************************************************************************
 **函数名称: flt_man_reg_cmp_cb
 **功    能: 注册比较函数
 **输入参数:
 **     reg1: 注册项1
 **     reg2: 注册项2
 **输出参数:
 **返    回: 0:相等 <0:小于 >0:大于
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.02.12 #
 ******************************************************************************/
static int flt_man_reg_cmp_cb(const flt_man_reg_t *reg1, const flt_man_reg_t *reg2)
{
    return (reg1->type - reg2->type);
}

/******************************************************************************
 **函数名称: flt_man_init
 **功    能: 初始化代理
 **输入参数:
 **     ctx: 全局信息
 **输出参数:
 **返    回: 管理对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.02.12 #
 ******************************************************************************/
static flt_man_t *flt_man_init(flt_cntx_t *ctx)
{
    flt_man_t *man;

    /* > 创建对象 */
    man = (flt_man_t *)calloc(1, sizeof(flt_man_t));
    if (NULL == man) {
        return NULL;
    }

    memset(man, 0, sizeof(flt_man_t));

    man->log = ctx->log;
    man->fd = INVALID_FD;

    do {
        /* > 创建AVL树 */
        man->reg = avl_creat(NULL, (cmp_cb_t)flt_man_reg_cmp_cb);
        if (NULL == man->reg) {
            log_error(man->log, "Create AVL failed!");
            return NULL;
        }

        /* > 创建链表 */
        man->mesg_list = list_creat(NULL);
        if (NULL == man->mesg_list) {
            log_error(man->log, "Create list failed!");
            break;
        }

        /* > 注册回调函数 */
        if (flt_man_reg_cb(man)) {
            log_error(man->log, "Register callback failed!");
            break;
        }

        /* > 新建套接字 */
        man->fd = udp_listen(ctx->conf->man_port);
        if (man->fd < 0) {
            log_error(man->log, "Listen port [%d] failed!", ctx->conf->man_port);
            break;
        }
        return man;
    } while(0);

    /* > 释放空间 */
    if (NULL != man->reg) {
        avl_destroy(man->reg, mem_dummy_dealloc, NULL);
    }
    if (NULL != man->mesg_list) {
        list_destroy(man->mesg_list, (mem_dealloc_cb_t)mem_dealloc, NULL);
    }
    CLOSE(man->fd);
    FREE(man);

    return NULL;
}

/******************************************************************************
 **函数名称: flt_man_register
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
 **作    者: # Qifeng.zou # 2015.03.15 #
 ******************************************************************************/
static int flt_man_register(flt_man_t *man, int type, flt_man_reg_cb_t proc, void *args)
{
    int ret;
    flt_man_reg_t *reg;

    /* > 申请空间 */
    reg = calloc(1, sizeof(flt_man_reg_t));
    if (NULL == reg) {
        log_error(man->log, "Alloc memory failed!");
        return FLT_ERR;
    }

    /* > 设置数据 */
    reg->type = type;
    reg->proc = proc;
    reg->args = NULL;

    ret = avl_insert(man->reg, reg);
    if (0 != ret) {
        log_error(man->log, "Register failed! ret:%d", ret);
        return FLT_ERR;
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_man_event_hdl
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
static int flt_man_event_hdl(flt_cntx_t *ctx, flt_man_t *man)
{
    /* > 接收命令 */
    if (FD_ISSET(man->fd, &man->rdset)) {
        if (flt_man_recv_cmd(ctx, man)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return FLT_ERR;
        }
    }

    /* > 发送数据 */
    if (FD_ISSET(man->fd, &man->wrset)) {
        if (flt_man_send_cmd(ctx, man)) {
            log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
            return FLT_ERR;
        }
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_man_recv_cmd
 **功    能: 接收命令数据
 **输入参数:
 **     man: 管理对象
 **     buff: 缓存空间
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 接收命令，并对命令类型和数据做相应的处理.
 **注意事项:
 **作    者: # Qifeng.zou # 2015.02.13 #
 ******************************************************************************/
static int flt_man_recv_cmd(flt_cntx_t *ctx, flt_man_t *man)
{
    ssize_t n;
    flt_cmd_t cmd;
    socklen_t addrlen;
    flt_man_reg_t *reg, key;
    struct sockaddr_un from;

    /* > 接收命令 */
    memset(&from, 0, sizeof(from));

    from.sun_family = AF_INET;
    addrlen = sizeof(from);

    n = recvfrom(man->fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
    if (n < 0) {
        log_error(man->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return FLT_ERR;
    }

    cmd.type = ntohl(cmd.type);

    /* > 查找回调 */
    key.type = cmd.type;

    reg = (flt_man_reg_t *)avl_query(man->reg, (void *)&key);
    if (NULL == reg) {
        log_error(man->log, "Didn't register callback for type [%d]!", cmd.type);
        return FLT_ERR;
    }

    /* > 执行回调 */
    return reg->proc(ctx, man, cmd.type, (void *)&cmd.data, &from, reg->args);
}

/******************************************************************************
 **函数名称: flt_man_send_cmd
 **功    能: 发送应答数据
 **输入参数:
 **     man: 管理对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 记得释放链表数据的空间，否则存在内存泄露的问题
 **作    者: # Qifeng.zou # 2015.02.28 #
 ******************************************************************************/
static int flt_man_send_cmd(flt_cntx_t *ctx, flt_man_t *man)
{
    int n;
    flt_cmd_item_t *item;

    /* > 弹出数据 */
    item = list_lpop(man->mesg_list);
    if (NULL == item) {
        log_error(man->log, "Didn't pop data from list!");
        return FLT_ERR;
    }

    /* > 发送命令 */
    n = sendto(man->fd, &item->cmd, sizeof(flt_cmd_t), 0, (struct sockaddr *)&item->to, sizeof(item->to));
    if (n < 0) {
        log_error(man->log, "errmsg:[%d] %s!", errno, strerror(errno));
        FREE(item);
        return FLT_OK;
    }

    /* > 释放空间 */
    FREE(item);

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_man_add_seed_req_hdl
 **功    能: 添加爬虫种子
 **输入参数:
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     seed: 种子信息
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 已存在, 则结束处理
 **     2. 不存在, 则进行存储, 并加入到UNDO队列!(TODO: 待完善)
 **注意事项: 申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int flt_man_add_seed_req_hdl(flt_cntx_t *ctx,
        flt_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    flt_cmd_t *cmd;
    flt_cmd_item_t *item;
    flt_cmd_add_seed_rsp_t *rsp;
    flt_cmd_add_seed_req_t *req = (flt_cmd_add_seed_req_t *)buff;

    log_debug(man->log, "Url:%s", req->url);

    /* > 申请应答空间 */
    item = calloc(1, sizeof(flt_cmd_item_t));
    if (NULL == item) {
        log_error(man->log, "Alloc memory failed!");
        return FLT_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置应答信息 */
    cmd = &item->cmd;
    rsp = (flt_cmd_add_seed_rsp_t *)&cmd->data;

    cmd->type = htonl(FLT_CMD_ADD_SEED_RSP);

    rsp->stat = htonl(FLT_CMD_ADD_SEED_STAT_SUCC);
    snprintf(rsp->url, sizeof(rsp->url), "%s", req->url);

    /* > 加入应答列表 */
    if (list_rpush(man->mesg_list, item)) {
        log_error(man->log, "Insert list failed!");
        FREE(item);
        return FLT_ERR;
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_man_query_conf_req_hdl
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
 **注意事项: 申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int flt_man_query_conf_req_hdl(flt_cntx_t *ctx,
        flt_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    flt_cmd_t *cmd;
    flt_cmd_item_t *item;
    flt_cmd_conf_t *conf;

    /* > 申请应答空间 */
    item = calloc(1, sizeof(flt_cmd_item_t));
    if (NULL == item) {
        log_error(man->log, "Alloc memory failed!");
        return FLT_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置应答信息 */
    cmd = &item->cmd;
    conf = (flt_cmd_conf_t *)&cmd->data;

    cmd->type = htonl(FLT_CMD_QUERY_CONF_RSP);

    conf->log_level = htonl(ctx->log->level);      /* 日志级别 */

    /* > 加入应答列表 */
    if (list_rpush(man->mesg_list, item)) {
        log_error(man->log, "Insert list failed!");
        FREE(conf);
        return FLT_ERR;
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: crwl_man_query_table_stat_req_hdl
 **功    能: 查询各表信息
 **输入参数:
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.17 #
 ******************************************************************************/
static int flt_man_query_table_stat_req_hdl(flt_cntx_t *ctx,
        flt_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    flt_cmd_t *cmd;
    flt_cmd_item_t *item;
    flt_cmd_table_stat_t *stat;

    /* > 新建应答 */
    item = calloc(1, sizeof(flt_cmd_item_t));
    if (NULL == item) {
        log_error(man->log, "Alloc memory failed!");
        return FLT_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置信息 */
    cmd = &item->cmd;
    stat = (flt_cmd_table_stat_t *)&cmd->data;

    cmd->type = htonl(FLT_CMD_QUERY_TABLE_STAT_RSP);

    /* 1. 域名IP映射表 */
    snprintf(stat->table[stat->num].name, sizeof(stat->table[stat->num].name), "DOMAIN IP MAP");

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 设置信息 */
    cmd = &item->cmd;
    stat = (flt_cmd_table_stat_t *)&cmd->data;

    cmd->type = htonl(FLT_CMD_QUERY_TABLE_STAT_RSP);

    /* 1. 域名IP映射表 */
    snprintf(stat->table[stat->num].name, sizeof(stat->table[stat->num].name), "DOMAIN IP MAP");
    stat->table[stat->num].num = htonl(hash_tab_total(ctx->domain_ip_map));
    stat->table[stat->num].max = -1;
    ++stat->num;

    /* 2. 域名黑名单表 */
    snprintf(stat->table[stat->num].name, sizeof(stat->table[stat->num].name), "DOMAIN BLACKLIST");
    stat->table[stat->num].num = htonl(hash_tab_total(ctx->domain_blacklist));
    stat->table[stat->num].max = -1;
    ++stat->num;

    stat->num = htonl(stat->num);

    /* > 放入队尾 */
    if (list_rpush(man->mesg_list, item)) {
        log_error(man->log, "Push into list failed!");
        FREE(item);
        return FLT_ERR;
    }

    return FLT_OK;
}

typedef struct
{
    int idx;
    FILE *fp;
} flt_man_domain_ip_map_trav_t;

/******************************************************************************
 **函数名称: flt_man_store_domain_ip_map_hdl
 **功    能: 存储域名IP映射表
 **输入参数:
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int flt_man_store_domain_ip_map_hdl(flt_domain_ip_map_t *map, void *args)
{
    int idx;
    flt_man_domain_ip_map_trav_t *trav = (flt_man_domain_ip_map_trav_t *)args;

    fprintf(trav->fp, "%05d|%s|%d", ++trav->idx, map->host, map->ip_num);

    for (idx=0; idx<map->ip_num; ++idx) {
        fprintf(trav->fp, "|%s", map->ip[idx].addr);
    }

    fprintf(trav->fp, "\n");

    return 0;
}

/******************************************************************************
 **函数名称: flt_man_store_domain_ip_map_req_hdl
 **功    能: 存储域名IP映射表
 **输入参数:
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int flt_man_store_domain_ip_map_req_hdl(flt_cntx_t *ctx,
        flt_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    static int idx = 0;
    struct tm loctm;
    struct timeb ctm;
    flt_cmd_t *cmd;
    flt_cmd_item_t *item;
    char fname[FILE_PATH_MAX_LEN];
    flt_cmd_store_domain_ip_map_rsp_t *rsp;
    flt_man_domain_ip_map_trav_t trav;

    memset(&trav, 0, sizeof(trav));

    Mkdir(ctx->conf->work.man_path, 0777);

    /* > 新建应答 */
    item = calloc(1, sizeof(flt_cmd_item_t));
    if (NULL == item) {
        log_error(man->log, "Alloc memory failed!");
        return FLT_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 存储至文件 */
    ftime(&ctm);
    local_time(&ctm.time, &loctm);

    snprintf(fname, sizeof(fname),
            "%s/%04d%02d%02d%02d%02d%02d%03d-%d.dim",
            ctx->conf->work.man_path,
            loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
            loctm.tm_hour, loctm.tm_min, loctm.tm_sec, ctm.millitm, ++idx);

    trav.fp = fopen(fname, "w");
    if (NULL == trav.fp) {
        log_error(man->log, "errmsg:[%d] %s!", errno, strerror(errno));
        FREE(item);
        return FLT_ERR;
    }

    hash_tab_trav(ctx->domain_ip_map,
            (trav_cb_t)flt_man_store_domain_ip_map_hdl, &trav, RDLOCK);

    FCLOSE(trav.fp);

    /* > 设置信息 */
    cmd = &item->cmd;
    rsp = (flt_cmd_store_domain_ip_map_rsp_t *)&cmd->data;

    cmd->type = htonl(FLT_CMD_STORE_DOMAIN_IP_MAP_RSP);

    snprintf(rsp->path, sizeof(rsp->path), "%s", fname);

    /* > 放入队尾 */
    if (list_rpush(man->mesg_list, item)) {
        log_error(man->log, "Push into list failed!");
        FREE(item);
        return FLT_ERR;
    }

    return FLT_OK;
}

typedef struct
{
    int idx;
    FILE *fp;
} flt_man_domain_blacklist_trav_t;

/******************************************************************************
 **函数名称: flt_man_store_domain_blacklist_hdl
 **功    能: 存储域名黑名单
 **输入参数:
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int flt_man_store_domain_blacklist_hdl(flt_domain_blacklist_t *blacklist, void *args)
{
    flt_man_domain_blacklist_trav_t *trav = (flt_man_domain_blacklist_trav_t *)args;

    fprintf(trav->fp, "%05d|%s\n", ++trav->idx, blacklist->host);

    return 0;
}

/******************************************************************************
 **函数名称: flt_man_store_domain_blacklist_req_hdl
 **功    能: 存储域名IP映射表
 **输入参数:
 **     ctx: 全局信息
 **     man: 管理对象
 **     type: 命令类型
 **     buff: 命令数据
 **     args: 附加参数
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 申请的应答数据空间, 需要在应答之后, 进行释放!
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int flt_man_store_domain_blacklist_req_hdl(flt_cntx_t *ctx,
        flt_man_t *man, int type, void *buff, struct sockaddr_un *from, void *args)
{
    static int idx = 0;
    struct tm loctm;
    struct timeb ctm;
    flt_cmd_t *cmd;
    flt_cmd_item_t *item;
    char fname[FILE_PATH_MAX_LEN];
    flt_man_domain_blacklist_trav_t trav;
    flt_cmd_store_domain_blacklist_rsp_t *rsp;

    memset(&trav, 0, sizeof(trav));

    Mkdir(ctx->conf->work.man_path, 0777);

    /* > 新建应答 */
    item = calloc(1, sizeof(flt_cmd_item_t));
    if (NULL == item) {
        log_error(man->log, "Alloc memory failed!");
        return FLT_ERR;
    }

    memcpy(&item->to, from, sizeof(struct sockaddr_un));

    /* > 存储至文件 */
    ftime(&ctm);
    local_time(&ctm.time, &loctm);

    snprintf(fname, sizeof(fname),
            "%s/%04d%02d%02d%02d%02d%02d%03d-%d.bl",
            ctx->conf->work.man_path,
            loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
            loctm.tm_hour, loctm.tm_min, loctm.tm_sec, ctm.millitm, ++idx);

    trav.fp = fopen(fname, "w");
    if (NULL == trav.fp) {
        log_error(man->log, "errmsg:[%d] %s!", errno, strerror(errno));
        FREE(item);
        return FLT_ERR;
    }

    hash_tab_trav(ctx->domain_blacklist,
            (trav_cb_t)flt_man_store_domain_blacklist_hdl, &trav, RDLOCK);

    FCLOSE(trav.fp);

    /* > 设置信息 */
    cmd = &item->cmd;
    rsp = (flt_cmd_store_domain_blacklist_rsp_t *)&cmd->data;

    cmd->type = htonl(FLT_CMD_STORE_DOMAIN_BLACKLIST_RSP);

    snprintf(rsp->path, sizeof(rsp->path), "%s", fname);

    /* > 放入队尾 */
    if (list_rpush(man->mesg_list, item)) {
        log_error(man->log, "Push into list failed!");
        FREE(item);
        return FLT_ERR;
    }

    return FLT_OK;
}

/******************************************************************************
 **函数名称: flt_man_reg_cb
 **功    能: 设置命令处理函数
 **输入参数:
 **     man: 管理对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.15 #
 ******************************************************************************/
static int flt_man_reg_cb(flt_man_t *man)
{
#define FLT_REG(man, type, proc, args) \
    if (flt_man_register(man, type, proc, args)) { \
        return FLT_ERR; \
    }

    /* 注册回调函数 */
    FLT_REG(man, FLT_CMD_ADD_SEED_REQ, (flt_man_reg_cb_t)flt_man_add_seed_req_hdl, man);
    FLT_REG(man, FLT_CMD_QUERY_CONF_REQ, (flt_man_reg_cb_t)flt_man_query_conf_req_hdl, man);
    FLT_REG(man, FLT_CMD_QUERY_TABLE_STAT_REQ, (flt_man_reg_cb_t)flt_man_query_table_stat_req_hdl, man);
    FLT_REG(man, FLT_CMD_STORE_DOMAIN_IP_MAP_REQ, (flt_man_reg_cb_t)flt_man_store_domain_ip_map_req_hdl, man);
    FLT_REG(man, FLT_CMD_STORE_DOMAIN_BLACKLIST_REQ, (flt_man_reg_cb_t)flt_man_store_domain_blacklist_req_hdl, man);

    return FLT_OK;
}

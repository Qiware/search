/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mon_crwl.c
 ** 版本号: 1.0
 ** 描  述: 监控爬虫引擎
 **         测试或获取爬虫引擎的详细数据信息.
 ** 注  意: 请勿显示中文，否则将会出现对齐异常!
 ** 作  者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
#include "sck.h"
#include "syscall.h"
#include "monitor.h"
#include "flt_cmd.h"

#define MON_FLT_INTERVAL_SEC   (5)

typedef int (*mon_flt_setup_cb_t)(flt_cmd_t *cmd);
typedef int (*mon_flt_print_cb_t)(flt_cmd_t *cmd);

/* 静态函数 */
static int mon_flt_add_seed_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_flt_query_conf_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_flt_query_table_stat_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_flt_store_domain_ip_map_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_flt_store_domain_blacklist_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);

/******************************************************************************
 **函数名称: mon_flt_entry
 **功    能: 进入监控FILTER界面
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 输入目的IP和端口
 **     2. 创建套接口
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_entry(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    ctx->to.sin_family = AF_INET;
    ctx->to.sin_port = htons(ctx->conf->filter.port);
    inet_pton(AF_INET, ctx->conf->filter.ip, &ctx->to.sin_addr);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_menu
 **功    能: 爬虫引擎菜单
 **输入参数:
 **     ctx: 菜单对象
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 爬虫引擎菜单
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.28 #
 ******************************************************************************/
menu_item_t *mon_flt_menu(menu_cntx_t *ctx, void *args)
{
    menu_item_t *menu;

    menu = menu_creat(ctx, "Monitor Filter", mon_flt_entry, menu_display, NULL, args);
    if (NULL == menu)
    {
        return NULL;
    }

#define ADD_CHILD(ctx, menu, title, entry, func, exit, args) \
    if (!menu_child(ctx, menu, title, entry, func, exit, args)) \
    { \
        return menu; \
    }

    /* 添加子菜单 */
    ADD_CHILD(ctx, menu, "Add seed", NULL, mon_flt_add_seed_req, NULL, args);
    ADD_CHILD(ctx, menu, "Query configuration", NULL, mon_flt_query_conf_req, NULL, args);
    ADD_CHILD(ctx, menu, "Query table status", NULL, mon_flt_query_table_stat_req, NULL, args);
    ADD_CHILD(ctx, menu, "Store domain ip map", NULL, mon_flt_store_domain_ip_map_req, NULL, args);
    ADD_CHILD(ctx, menu, "Store domain blacklist", NULL, mon_flt_store_domain_blacklist_req, NULL, args);
    return menu;
}

/******************************************************************************
 **函数名称: mon_flt_frame
 **功    能: 过滤系统监控的框架
 **输入参数:
 **     setup: 设置参数
 **     print: 打印结果
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_frame(mon_flt_setup_cb_t setup, mon_flt_print_cb_t print, void *args)
{
    int ret, flag = 0;
    ssize_t n;
    flt_cmd_t cmd;
    socklen_t addrlen;
    fd_set rdset, wrset;
    struct timeval tmout;
    struct sockaddr_in from;
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    while (1)
    {
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);

        FD_SET(ctx->fd, &rdset);
        if (!flag)
        {
            FD_SET(ctx->fd, &wrset);
        }

        /* > 等待事件 */
        tmout.tv_sec = MON_FLT_INTERVAL_SEC;
        tmout.tv_usec = 0;

        ret = select(ctx->fd+1, &rdset, &wrset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno) { continue; }
            fprintf(stderr, "    errrmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            fprintf(stderr, "    Timeout!");
            break;
        }

        /* > 发送命令 */
        if (FD_ISSET(ctx->fd, &wrset))
        {
            memset(&cmd, 0, sizeof(cmd));

            setup(&cmd); /* 设置参数 */

            n = sendto(ctx->fd, &cmd, sizeof(cmd), 0, (const struct sockaddr *)&ctx->to, sizeof(ctx->to));
            if (n < 0)
            {
                fprintf(stderr, "    errrmsg:[%d] %s!", errno, strerror(errno));
                break;
            }
            flag = 1;
        }

        /* > 接收应答 */
        if (FD_ISSET(ctx->fd, &rdset))
        {
            memset(&from, 0, sizeof(from));

            from.sin_family = AF_INET;

            addrlen = sizeof(from);

            n = recvfrom(ctx->fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
            if (n < 0)
            {
                fprintf(stderr, "    errrmsg:[%d] %s!", errno, strerror(errno));
                break;
            }

            print(&cmd); /* 打印反馈 */

            break;
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: mon_flt_add_seed_setup
 **功    能: 设置添加种子的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_add_seed_setup(flt_cmd_t *cmd)
{
    char url[256];
    flt_cmd_add_seed_req_t *req;

    fprintf(stdout, "    SEED: ");
    scanf(" %s", url);

    cmd->type = htonl(FLT_CMD_ADD_SEED_REQ);

    req = (flt_cmd_add_seed_req_t *)&cmd->data;
    snprintf(req->url, sizeof(req->url), "%s", url);

    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_add_seed_print
 **功    能: 打印添加种子的结果反馈
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_add_seed_print(flt_cmd_t *cmd)
{
    flt_cmd_add_seed_rep_t *rep;

    cmd->type = ntohl(cmd->type);
    rep = (flt_cmd_add_seed_rep_t *)&cmd->data;

    /* 显示结果 */
    rep->stat = ntohl(rep->stat);
    switch (rep->stat)
    {
        case FLT_CMD_ADD_SEED_STAT_SUCC:
        {
            fprintf(stderr, "    Add seed success!\n");
            break;
        }
        case FLT_CMD_ADD_SEED_STAT_FAIL:
        {
            fprintf(stderr, "    Add seed failed!\n");
            break;
        }
        case FLT_CMD_ADD_SEED_STAT_EXIST:
        {
            fprintf(stderr, "    Seed [%s] is exist!\n", rep->url);
            break;
        }
        case FLT_CMD_ADD_SEED_STAT_UNKNOWN:
        default:
        {
            fprintf(stderr, "    Unknown status!\n");
            break;
        }
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_add_seed_req
 **功    能: 添加种子
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_add_seed_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    return mon_flt_frame(mon_flt_add_seed_setup, mon_flt_add_seed_print, args);
}

/******************************************************************************
 **函数名称: mon_flt_query_conf_setup
 **功    能: 设置查询爬虫配置的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_query_conf_setup(flt_cmd_t *cmd)
{
    cmd->type = htonl(FLT_CMD_QUERY_CONF_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_query_conf_print
 **功    能: 显示爬虫配置
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_query_conf_print(flt_cmd_t *cmd)
{
    flt_cmd_conf_t *conf;

    /* 字节序转换 */
    cmd->type = ntohl(cmd->type);
    conf = (flt_cmd_conf_t *)&cmd->data;

    conf->log.level = ntohl(conf->log.level);

    /* 显示结果 */
    fprintf(stderr, "    日志信息:\n");
    fprintf(stderr, "        LEVEL: %s\n", log_get_str(conf->log.level));
    fprintf(stderr, "        PATH: %s\n", conf->log.path);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_query_conf_req
 **功    能: 查询爬虫配置
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_query_conf_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    return mon_flt_frame(mon_flt_query_conf_setup, mon_flt_query_conf_print, args);
}

/******************************************************************************
 **函数名称: mon_flt_query_table_stat_setup
 **功    能: 设置查询各表状态的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_query_table_stat_setup(flt_cmd_t *cmd)
{
    cmd->type = htonl(FLT_CMD_QUERY_TABLE_STAT_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_query_table_stat_print
 **功    能: 打印查询各表状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_flt_query_table_stat_print(flt_cmd_t *cmd)
{
    int idx, num;
    flt_cmd_table_stat_t *stat;

    cmd->type = ntohl(cmd->type);
    stat = (flt_cmd_table_stat_t *)&cmd->data;

    /* 显示结果 */
    fprintf(stderr, "    %8s | %-16s| %-8s | %-8s\n", "IDX", "NAME", "NUMBER", "MAX");
    fprintf(stderr, "    ----------------------------------------------\n");

    num = ntohl(stat->num);
    for (idx=0; idx<num; ++idx)
    {
        fprintf(stderr, "    %8d | %-16s| %-8d | %-8u\n",
                idx+1,
                stat->table[idx].name,
                ntohl(stat->table[idx].num),
                ntohl(stat->table[idx].max));
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_query_table_stat_req
 **功    能: 查询各表状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.28 #
 ******************************************************************************/
static int mon_flt_query_table_stat_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    return mon_flt_frame(
            mon_flt_query_table_stat_setup,
            mon_flt_query_table_stat_print, args);
}

/******************************************************************************
 **函数名称: mon_flt_store_domain_ip_map_setup
 **功    能: 设置存储域名IP映射表的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int mon_flt_store_domain_ip_map_setup(flt_cmd_t *cmd)
{
    cmd->type = htonl(FLT_CMD_STORE_DOMAIN_IP_MAP_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_store_domain_ip_map_print
 **功    能: 显示存储域名IP映射表信息
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int mon_flt_store_domain_ip_map_print(flt_cmd_t *cmd)
{
    flt_cmd_store_domain_ip_map_rep_t *rep;

    /* 字节序转换 */
    cmd->type = ntohl(cmd->type);
    rep = (flt_cmd_store_domain_ip_map_rep_t *)&cmd->data;

    /* 显示结果 */
    fprintf(stderr, "        PATH: %s\n", rep->path);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_store_domain_ip_map_req
 **功    能: 存储域名IP映射表
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int mon_flt_store_domain_ip_map_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    return mon_flt_frame(mon_flt_store_domain_ip_map_setup, mon_flt_store_domain_ip_map_print, args);
}

/******************************************************************************
 **函数名称: mon_flt_store_domain_blacklist_setup
 **功    能: 设置存储域名黑名单的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int mon_flt_store_domain_blacklist_setup(flt_cmd_t *cmd)
{
    cmd->type = htonl(FLT_CMD_STORE_DOMAIN_BLACKLIST_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_store_domain_blacklist_print
 **功    能: 显示存储域名黑名单信息
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int mon_flt_store_domain_blacklist_print(flt_cmd_t *cmd)
{
    flt_cmd_store_domain_ip_map_rep_t *rep;

    /* 字节序转换 */
    cmd->type = ntohl(cmd->type);
    rep = (flt_cmd_store_domain_ip_map_rep_t *)&cmd->data;

    /* 显示结果 */
    fprintf(stderr, "        PATH: %s\n", rep->path);
    return 0;
}

/******************************************************************************
 **函数名称: mon_flt_store_domain_blacklist_req
 **功    能: 存储域名黑名单
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.07 #
 ******************************************************************************/
static int mon_flt_store_domain_blacklist_req(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    return mon_flt_frame(
            mon_flt_store_domain_blacklist_setup,
            mon_flt_store_domain_blacklist_print, args);
}

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
#include "crwl_cmd.h"

#define MON_CRWL_INTERVAL_SEC   (5)

typedef int (*mon_crwl_setup_cb_t)(crwl_cmd_t *cmd);
typedef int (*mon_crwl_print_cb_t)(crwl_cmd_t *cmd);

/* 静态函数 */
static int mon_crwl_query_conf_req(menu_cntx_t *ctx, menu_item_t *menu, void *args);
static int mon_crwl_query_workq_stat_req(menu_cntx_t *ctx, menu_item_t *menu, void *args);
static int mon_crwl_query_worker_stat_req(menu_cntx_t *ctx, menu_item_t *menu, void *args);
static int mon_crwl_switch_sched_req(menu_cntx_t *ctx, menu_item_t *menu, void *args);

/******************************************************************************
 **函数名称: mon_crwl_entry
 **功    能: 爬虫监控初始化
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
static int mon_crwl_entry(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    ctx->to.sin_family = AF_INET;
    ctx->to.sin_port = htons(ctx->conf->crwl.port);
    inet_pton(AF_INET, ctx->conf->crwl.ip, &ctx->to.sin_addr);
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_menu
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
menu_item_t *mon_crwl_menu(menu_cntx_t *ctx, void *args)
{
    menu_item_t *menu;

    menu = menu_creat(ctx, "Monitor Crawler", mon_crwl_entry, menu_display, NULL, args);
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
    ADD_CHILD(ctx, menu, "Query configuration", NULL, mon_crwl_query_conf_req, NULL, args);
    ADD_CHILD(ctx, menu, "Query workq status", NULL, mon_crwl_query_workq_stat_req, NULL, args);
    ADD_CHILD(ctx, menu, "Query worker status", NULL, mon_crwl_query_worker_stat_req, NULL, args);
    ADD_CHILD(ctx, menu, "Switch sched status", NULL, mon_crwl_switch_sched_req, NULL, args);
    return menu;
}

/******************************************************************************
 **函数名称: mon_crwl_frame
 **功    能: 爬虫监控的框架
 **输入参数:
 **     setup: 设置参数
 **     print: 打印结果
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_frame(mon_crwl_setup_cb_t setup, mon_crwl_print_cb_t print, void *args)
{
    int ret, flag = 0;
    ssize_t n;
    crwl_cmd_t cmd;
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
        tmout.tv_sec = MON_CRWL_INTERVAL_SEC;
        tmout.tv_usec = 0;

        ret = select(ctx->fd+1, &rdset, &wrset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

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
 **函数名称: mon_crwl_query_worker_stat_setup
 **功    能: 设置查询爬虫状态的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_worker_stat_setup(crwl_cmd_t *cmd)
{
    cmd->type = htonl(MSG_QUERY_WORKER_STAT_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_query_worker_stat_print
 **功    能: 打印查询爬虫状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_worker_stat_print(crwl_cmd_t *cmd)
{
    int idx;
    struct tm loctm;
    crwl_cmd_worker_stat_t *stat;
    uint64_t connections = 0, down_webpage_total = 0, err_webpage_total = 0;
    static uint64_t last_down_webpage_total = 0;
    static time_t last_query_tm = 0;
    time_t diff_tm;

    cmd->type = ntohl(cmd->type);
    stat = (crwl_cmd_worker_stat_t *)&cmd->data;

    /* > 显示结果 */
    connections = 0;
    down_webpage_total = 0;
    err_webpage_total = 0;

    /* 1. 线程列表 */
    fprintf(stderr, "\n    %8s | %-12s | %-8s | %-8s\n", "IDX", "CONNECTIONS", "DOWNLOAD", "ERROR");
    fprintf(stderr, "    ----------------------------------------------\n");

    stat->num = ntohl(stat->num);
    for (idx=0; idx<stat->num && idx<CRWL_CMD_WORKER_MAX_NUM; ++idx)
    {
        /* 字节序转换 */
        stat->worker[idx].connections = ntohl(stat->worker[idx].connections);
        stat->worker[idx].down_webpage_total = ntoh64(stat->worker[idx].down_webpage_total);
        stat->worker[idx].err_webpage_total = ntoh64(stat->worker[idx].err_webpage_total);

        /* 显示状态 */
        fprintf(stderr, "    %8d | %-12d | %-8llu | %-8llu\n",
                idx+1,
                stat->worker[idx].connections,
                stat->worker[idx].down_webpage_total,
                stat->worker[idx].err_webpage_total);

        /* 数量统计 */
        connections += stat->worker[idx].connections;
        down_webpage_total += stat->worker[idx].down_webpage_total;
        err_webpage_total += stat->worker[idx].err_webpage_total;
    }

    /* 显示统计 */
    fprintf(stderr, "    ----------------------------------------------\n");
    fprintf(stderr, "    %8d | %-12ld | %-8ld | %-8ld\n",
            stat->num, connections, down_webpage_total, err_webpage_total);

    /* 2. 其他信息 */
    stat->stm = ntohl(stat->stm);
    local_time(&stat->stm, &loctm);

    fprintf(stderr, "    启动时间: %04d-%02d-%02d %02d:%02d:%02d\n",
            loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
            loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

    stat->ctm = ntohl(stat->ctm);
    local_time(&stat->ctm, &loctm);

    diff_tm = stat->ctm - last_query_tm;
    if (0 == diff_tm)
    {
        diff_tm = 1;
    }

    fprintf(stderr, "    当前时间: %04d-%02d-%02d %02d:%02d:%02d\n"
            "    运行时长: %lud:%luh:%lum:%lus\n"
            "    平均速率: %lf\n"
            "    当前速率: %lf\n"
            "    异常比率: %lf%%\n",
            loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
            loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
            TM_DAY(stat->ctm - stat->stm), TM_HOUR(stat->ctm - stat->stm),
            TM_MIN(stat->ctm - stat->stm), TM_SEC(stat->ctm - stat->stm),
            (double)down_webpage_total / (stat->ctm - stat->stm),
            (double)(down_webpage_total - last_down_webpage_total) / diff_tm,
            (double)err_webpage_total / (down_webpage_total+1) * 100);

    last_query_tm = stat->ctm;
    last_down_webpage_total = down_webpage_total;

    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_query_worker_stat_req
 **功    能: 查询爬虫状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.27 #
 ******************************************************************************/
static int mon_crwl_query_worker_stat_req(menu_cntx_t *ctx, menu_item_t *menu, void *args)
{
    return mon_crwl_frame(
            mon_crwl_query_worker_stat_setup,
            mon_crwl_query_worker_stat_print, args);
}

/******************************************************************************
 **函数名称: mon_crwl_query_workq_stat_setup
 **功    能: 设置查询工作队列状态的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_workq_stat_setup(crwl_cmd_t *cmd)
{
    cmd->type = htonl(MSG_QUERY_WORKQ_STAT_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_query_workq_stat_print
 **功    能: 打印查询工作队列状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_workq_stat_print(crwl_cmd_t *cmd)
{
    int idx, num;
    crwl_cmd_workq_stat_t *stat;

    cmd->type = ntohl(cmd->type);
    stat = (crwl_cmd_workq_stat_t *)&cmd->data;

    /* 显示结果 */
    fprintf(stderr, "    %8s | %-16s| %-8s | %-8s\n", "IDX", "NAME", "NUMBER", "MAX");
    fprintf(stderr, "    ----------------------------------------------\n");

    num = ntohl(stat->num);
    for (idx=0; idx<num; ++idx)
    {
        stat->queue[idx].num = ntohl(stat->queue[idx].num);
        stat->queue[idx].max = ntohl(stat->queue[idx].max);

        fprintf(stderr, "    %8d | %-16s| %-8d | %-8u\n",
                idx+1,
                stat->queue[idx].name,
                stat->queue[idx].num,
                stat->queue[idx].max);
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_query_workq_stat_req
 **功    能: 查询工作队列状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_workq_stat_req(menu_cntx_t *ctx, menu_item_t *menu, void *args)
{
    return mon_crwl_frame(
            mon_crwl_query_workq_stat_setup,
            mon_crwl_query_workq_stat_print, args);
}

/******************************************************************************
 **函数名称: mon_crwl_query_conf_setup
 **功    能: 设置查询爬虫配置的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_conf_setup(crwl_cmd_t *cmd)
{
    cmd->type = htonl(MSG_QUERY_CONF_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_query_conf_print
 **功    能: 显示爬虫配置
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_conf_print(crwl_cmd_t *cmd)
{
    crwl_cmd_conf_t *conf;

    /* 字节序转换 */
    cmd->type = ntohl(cmd->type);
    conf = (crwl_cmd_conf_t *)&cmd->data;

    conf->log.level = ntohl(conf->log.level);
    conf->download.depth = ntohl(conf->download.depth);
    conf->worker.num = ntohl(conf->worker.num);
    conf->worker.conn_max_num = ntohl(conf->worker.conn_max_num);
    conf->worker.conn_tmout_sec = ntohl(conf->worker.conn_tmout_sec);

    /* 显示结果 */
    fprintf(stderr, "    日志信息:\n");
    fprintf(stderr, "        LEVEL: %s\n", log_get_str(conf->log.level));
    fprintf(stderr, "        PATH: %s\n", conf->log.path);
    fprintf(stderr, "    爬取配置:\n");
    fprintf(stderr, "        DEPTH: %d\n", conf->download.depth);
    fprintf(stderr, "        PATH: %s\n", conf->download.path);
    fprintf(stderr, "    爬虫配置:\n");
    fprintf(stderr, "        NUM: %d\n", conf->worker.num);
    fprintf(stderr, "        CONNECTIONS: %d\n", conf->worker.conn_max_num);
    fprintf(stderr, "        TIMEOUT: %d\n", conf->worker.conn_tmout_sec);
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_query_conf_req
 **功    能: 查询爬虫配置
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.02 #
 ******************************************************************************/
static int mon_crwl_query_conf_req(menu_cntx_t *ctx, menu_item_t *menu, void *args)
{
    return mon_crwl_frame(
            mon_crwl_query_conf_setup,
            mon_crwl_query_conf_print, args);
}

/******************************************************************************
 **函数名称: mon_crwl_switch_sched_setup
 **功    能: 设置切换调度的参数
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.26 #
 ******************************************************************************/
static int mon_crwl_switch_sched_setup(crwl_cmd_t *cmd)
{
    cmd->type = htonl(MSG_SWITCH_SCHED_REQ);
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_switch_sched_print
 **功    能: 显示切换调度的反馈
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.26 #
 ******************************************************************************/
static int mon_crwl_switch_sched_print(crwl_cmd_t *cmd)
{
    crwl_cmd_sched_stat_t *stat;

    /* 字节序转换 */
    cmd->type = ntohl(cmd->type);
    stat = (crwl_cmd_sched_stat_t *)&cmd->data;

    stat->sched_stat = ntohl(stat->sched_stat);

    /* 显示结果 */
    if (stat->sched_stat)
    {
        fprintf(stderr, "    调度状态: SCHED\n");
    }
    else
    {
        fprintf(stderr, "    调度状态: PAUSE\n");
    }
    return 0;
}

/******************************************************************************
 **函数名称: mon_crwl_switch_sched_req
 **功    能: 设置切换调度
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.26 #
 ******************************************************************************/
static int mon_crwl_switch_sched_req(menu_cntx_t *ctx, menu_item_t *menu, void *args)
{
    return mon_crwl_frame(
            mon_crwl_switch_sched_setup,
            mon_crwl_switch_sched_print, args);
}

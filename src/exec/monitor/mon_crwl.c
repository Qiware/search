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
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sck_api.h"
#include "syscall.h"
#include "monitor.h"
#include "crwl_cmd.h"

#define MON_CRWL_INTERVAL_SEC   (1)

static int mon_crwl_query_worker_stat_req(menu_item_t *menu);
static int mon_crwl_query_table_stat_req(menu_item_t *menu);
static int mon_crwl_query_workq_stat_req(menu_item_t *menu);

/******************************************************************************
 **函数名称: mon_crwl_menu
 **功    能: 爬虫引擎菜单
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 爬虫引擎菜单
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.28 #
 ******************************************************************************/
menu_item_t *mon_crwl_menu(menu_cntx_t *ctx)
{
    menu_item_t *menu, *child;

    menu = menu_creat(ctx, "Monitor Crawler", menu_display);
    if (NULL == menu)
    {
        return NULL;
    }

    /* 添加子菜单 */
    child = menu_creat(ctx, "Query worker status", mon_crwl_query_worker_stat_req);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    /* 添加子菜单 */
    child = menu_creat(ctx, "Query talbe status", mon_crwl_query_table_stat_req);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    /* 添加子菜单 */
    child = menu_creat(ctx, "Query work queue status", mon_crwl_query_workq_stat_req);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    return menu;
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
static int mon_crwl_query_worker_stat_req(menu_item_t *menu)
{
    int fd, ret, idx;
    ssize_t n;
    crwl_cmd_t cmd;
    socklen_t addrlen;
    fd_set rdset, wrset;
    struct timeval tmout;
    time_t ctm = 0, wtm = 0;
    struct tm loctm;
    struct sockaddr_in to, from;
    crwl_cmd_worker_stat_t *stat;
    uint64_t connections = 0, down_webpage_total = 0,
             err_webpage_total = 0, last_down_webpage_total = 0;

    memset(&to, 0, sizeof(to));

    to.sin_family = AF_INET;
    to.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);

    /* > 创建套接字 */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    while (1)
    {
        ctm = time(NULL);

        FD_ZERO(&rdset);
        FD_ZERO(&wrset);

        FD_SET(fd, &rdset);
        if (ctm - wtm >= MON_CRWL_INTERVAL_SEC)
        {
            FD_SET(fd, &wrset);
        }

        /* > 等待事件 */
        tmout.tv_sec = MON_CRWL_INTERVAL_SEC;
        tmout.tv_usec = 0;

        ret = select(fd+1, &rdset, &wrset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            continue;
        }

        /* > 发送命令 */
        if (FD_ISSET(fd, &wrset))
        {
            wtm = ctm;

            memset(&cmd, 0, sizeof(cmd));

            cmd.type = htonl(CRWL_CMD_QUERY_WORKER_STAT_REQ);

            n = sendto(fd, &cmd, sizeof(cmd), 0, (const struct sockaddr *)&to, sizeof(to));
            if (n < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
                break;
            }
        }

        /* > 接收应答 */
        if (FD_ISSET(fd, &rdset))
        {
            memset(&from, 0, sizeof(from));

            from.sin_family = AF_INET;

            addrlen = sizeof(from);

            n = recvfrom(fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
            if (n < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
                break;
            }

            cmd.type = ntohl(cmd.type);
            stat = (crwl_cmd_worker_stat_t *)&cmd.data;

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
                stat->worker[idx].connections = ntohl(stat->worker[idx].connections);
                stat->worker[idx].down_webpage_total = ntoh64(stat->worker[idx].down_webpage_total);
                stat->worker[idx].err_webpage_total = ntoh64(stat->worker[idx].err_webpage_total);

                fprintf(stderr, "    %8d | %-12d | %-8ld | %-8ld\n",
                        idx+1,
                        stat->worker[idx].connections,
                        stat->worker[idx].down_webpage_total,
                        stat->worker[idx].err_webpage_total);

                connections += stat->worker[idx].connections;
                down_webpage_total += stat->worker[idx].down_webpage_total;
                err_webpage_total += stat->worker[idx].err_webpage_total;
            }

            fprintf(stderr, "    ----------------------------------------------\n");
            fprintf(stderr, "    %8d | %-12ld | %-8ld | %-8ld\n",
                    stat->num, connections, down_webpage_total, err_webpage_total);

            /* 2. 启动/当前时间 */
            stat->stm = ntohl(stat->stm);
            localtime_r(&stat->stm, &loctm);

            fprintf(stderr, "    启动时间: %04d-%02d-%02d %02d:%02d:%02d\n",
                    loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

            stat->ctm = ntohl(stat->ctm);
            localtime_r(&stat->ctm, &loctm);

            fprintf(stderr, "    当前时间: %04d-%02d-%02d %02d:%02d:%02d\n"
                    "    运行时长: %lu\n"
                    "    平均速率: %ld\n"
                    "    当前速率: %ld\n",
                    loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    stat->ctm - stat->stm,
                    down_webpage_total / (stat->ctm - stat->stm),
                    down_webpage_total - last_down_webpage_total);

            last_down_webpage_total = down_webpage_total;
        }
    }

    Close(fd);

    return -1;
}

/******************************************************************************
 **函数名称: mon_crwl_query_table_stat_req
 **功    能: 查询TABLE状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.28 #
 ******************************************************************************/
static int mon_crwl_query_table_stat_req(menu_item_t *menu)
{
    int fd, ret, idx, num;
    ssize_t n;
    crwl_cmd_t cmd;
    socklen_t addrlen;
    fd_set rdset, wrset;
    struct timeval tmout;
    time_t ctm = 0, wtm = 0;
    struct sockaddr_in to, from;
    crwl_cmd_table_stat_t *stat;

    memset(&to, 0, sizeof(to));

    to.sin_family = AF_INET;
    to.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);

    /* > 创建套接字 */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    while (1)
    {
        ctm = time(NULL);

        FD_ZERO(&rdset);
        FD_ZERO(&wrset);

        FD_SET(fd, &rdset);
        if (ctm - wtm >= MON_CRWL_INTERVAL_SEC)
        {
            FD_SET(fd, &wrset);
        }

        /* > 等待事件 */
        tmout.tv_sec = MON_CRWL_INTERVAL_SEC;
        tmout.tv_usec = 0;

        ret = select(fd+1, &rdset, &wrset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            continue;
        }

        /* > 发送命令 */
        if (FD_ISSET(fd, &wrset))
        {
            wtm = ctm;

            memset(&cmd, 0, sizeof(cmd));

            cmd.type = htonl(CRWL_CMD_QUERY_TABLE_STAT_REQ);

            n = sendto(fd, &cmd, sizeof(cmd), 0, (const struct sockaddr *)&to, sizeof(to));
            if (n < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
                break;
            }
        }

        /* > 接收应答 */
        if (FD_ISSET(fd, &rdset))
        {
            memset(&from, 0, sizeof(from));

            from.sin_family = AF_INET;

            addrlen = sizeof(from);

            n = recvfrom(fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
            if (n < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
                break;
            }

            cmd.type = ntohl(cmd.type);
            stat = (crwl_cmd_table_stat_t *)&cmd.data;

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
        }
    }

    Close(fd);

    return -1;
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
 **作    者: # Qifeng.zou # 2015.03.01 #
 ******************************************************************************/
static int mon_crwl_query_workq_stat_req(menu_item_t *menu)
{
    int fd, ret, idx, num;
    ssize_t n;
    crwl_cmd_t cmd;
    socklen_t addrlen;
    fd_set rdset, wrset;
    struct timeval tmout;
    time_t ctm = 0, wtm = 0;
    struct sockaddr_in to, from;
    crwl_cmd_workq_stat_t *stat;

    memset(&to, 0, sizeof(to));

    to.sin_family = AF_INET;
    to.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);

    /* > 创建套接字 */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    while (1)
    {
        ctm = time(NULL);

        FD_ZERO(&rdset);
        FD_ZERO(&wrset);

        FD_SET(fd, &rdset);
        if (ctm - wtm >= MON_CRWL_INTERVAL_SEC)
        {
            FD_SET(fd, &wrset);
        }

        /* > 等待事件 */
        tmout.tv_sec = MON_CRWL_INTERVAL_SEC;
        tmout.tv_usec = 0;

        ret = select(fd+1, &rdset, &wrset, NULL, &tmout);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            continue;
        }

        /* > 发送命令 */
        if (FD_ISSET(fd, &wrset))
        {
            wtm = ctm;

            memset(&cmd, 0, sizeof(cmd));

            cmd.type = htonl(CRWL_CMD_QUERY_WORKQ_STAT_REQ);

            n = sendto(fd, &cmd, sizeof(cmd), 0, (const struct sockaddr *)&to, sizeof(to));
            if (n < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
                break;
            }
        }

        /* > 接收应答 */
        if (FD_ISSET(fd, &rdset))
        {
            memset(&from, 0, sizeof(from));

            from.sin_family = AF_INET;

            addrlen = sizeof(from);

            n = recvfrom(fd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&from, (socklen_t *)&addrlen);
            if (n < 0)
            {
                fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
                break;
            }

            cmd.type = ntohl(cmd.type);
            stat = (crwl_cmd_workq_stat_t *)&cmd.data;

            /* 显示结果 */
            fprintf(stderr, "    %8s | %-16s| %-8s | %-8s\n", "IDX", "NAME", "NUMBER", "MAX");
            fprintf(stderr, "    ----------------------------------------------\n");
            num = ntohl(stat->num);
            for (idx=0; idx<num; ++idx)
            {
                fprintf(stderr, "    %8d | %-16s| %-8d | %-8u\n",
                        idx+1,
                        stat->queue[idx].name,
                        ntohl(stat->queue[idx].num),
                        ntohl(stat->queue[idx].max));
            }
        }
    }

    Close(fd);

    return -1;
}

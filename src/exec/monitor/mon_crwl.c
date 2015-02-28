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
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sck_api.h"
#include "syscall.h"
#include "monitor.h"
#include "crwl_cmd.h"

static int mon_crwl_query_worker(menu_item_t *menu);

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
    child = menu_creat(ctx, "Query worker status", mon_crwl_query_worker);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    /* 添加子菜单 */
    child = menu_creat(ctx, "Connect to Search-Engine", NULL);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    /* 添加子菜单 */
    child = menu_creat(ctx, "Connect to Search-Engine", NULL);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    return menu;
}

/******************************************************************************
 **函数名称: mon_crwl_query_worker
 **功    能: 查询爬虫状态
 **输入参数:
 **     menu: 菜单对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.02.27 #
 ******************************************************************************/
static int mon_crwl_query_worker(menu_item_t *menu)
{
#define MON_CRWL_INTERVAL_SEC   (1)
    int fd, ret, idx;
    ssize_t n;
    crwl_cmd_t cmd;
    socklen_t addrlen;
    fd_set rdset, wrset;
    struct timeval tmout;
    time_t ctm = 0, wtm = 0;
    struct sockaddr_in to, from;
    crwl_cmd_worker_resp_t *resp;

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
        tmout.tv_sec = 1;
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

            cmd.type = CRWL_CMD_QUERY_WORKER_REQ;

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

            resp = (crwl_cmd_worker_resp_t *)&cmd.data;

            /* 显示结果 */
            fprintf(stderr, "\tnum:%d\n", resp->num);
            for (idx=0; idx<resp->num && idx<CRWL_CMD_WORKER_MAX_NUM; ++idx)
            {
                fprintf(stderr, "\tconnections:%d download:%ld error:%ld\n",
                        resp->worker[idx].connections,
                        resp->worker[idx].down_webpage_total,
                        resp->worker[idx].err_webpage_total);
            }
        }
    }

    Close(fd);

    return -1;
}

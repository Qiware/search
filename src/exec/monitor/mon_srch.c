/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mon_srch.c
 ** 版本号: 1.0
 ** 描  述: 监控搜索引擎
 **         测试或获取搜索引擎的详细数据信息.
 ** 注  意: 请勿显示中文，否则将会出现对齐异常!
 ** 作  者: # Qifeng.zou # 2014.12.27 #
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

#include "syscall.h"
#include "monitor.h"
#include "sck_api.h"
#include "srch_mesg.h"

#define SRCH_SVR_IP_ADDR    "127.0.0.1"
#define SRCH_SVR_PORT       (8888)

#define SRCH_CLIENT_NUM     (4000)

static int mon_srch_connect(menu_item_t *menu);

/******************************************************************************
 **函数名称: mon_srch_menu
 **功    能: 搜索引擎菜单
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 搜索引擎菜单
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_item_t *mon_srch_menu(menu_cntx_t *ctx)
{
    menu_item_t *menu, *child;

    menu = menu_creat(ctx, "Monitor Search Engine", menu_display);
    if (NULL == menu)
    {
        return NULL;
    }

    /* 添加子菜单 */
    child = menu_creat(ctx, "Get configuration", mon_srch_connect);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    /* 添加子菜单 */
    child = menu_creat(ctx, "Get current status", mon_srch_connect);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    /* 添加子菜单 */
    child = menu_creat(ctx, "Test connect", mon_srch_connect);
    if (NULL == child)
    {
        return menu;
    }

    menu_add(menu, child);

    return menu;
}

/******************************************************************************
 **函数名称: mon_srch_connect
 **功    能: 测试搜索引擎处理大并发连接的能力
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 连接搜索引擎
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static int mon_srch_connect(menu_item_t *menu)
{
    char ip[IP_ADDR_MAX_LEN], input[128];
    int port = SRCH_SVR_PORT, num = SRCH_CLIENT_NUM;
    int *fd, idx, n;
    srch_mesg_body_t body;
    srch_mesg_header_t header;

    fprintf(stdout, "Use default configuration? [Y/n]");
    scanf(" %s", input);
    if (0 == strcasecmp(input, "Y")
        || 0 == strcasecmp(input, "Yes"))
    {
        snprintf(ip, sizeof(ip), "%s", SRCH_SVR_IP_ADDR);
        port = SRCH_SVR_PORT;
        num = SRCH_CLIENT_NUM;
    }
    else
    {
        fprintf(stdout, "Input ip:");
        scanf(" %s", ip);

        fprintf(stdout, "Input port:");
        scanf(" %s", input);
        port = atoi(input);

        fprintf(stdout, "Input num:");
        scanf(" %s", input);
        num = atoi(input);
    }

    limit_file_num(4096); /* 设置进程打开文件的最大数目 */

    fd = (int *)malloc(num * sizeof(int));

    /* 连接搜索引擎 */
    for (idx=0; idx<num; ++idx)
    {
        fd[idx] = tcp_connect(AF_INET, ip, port);
        if (fd[idx] < 0)
        {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }
    }

    /* 发送搜索数据 */
    for (idx=0; idx<num; ++idx)
    {
        if (fd[idx] > 0)
        {
            header.type = idx%0xFF;
            header.flag = SRCH_MSG_FLAG_USR;
            header.mark = htonl(SRCH_MSG_MARK_KEY);
            header.length = htons(sizeof(body));

            snprintf(body.words, sizeof(body.words), "爱我中华");

            n = Writen(fd[idx], (void *)&header, sizeof(header));

            n = Writen(fd[idx], (void *)&body, sizeof(body));

            fprintf(stdout, "idx:%d n:%d!\n", idx, n);
        }
    }

    Sleep(5);

    /* 关闭网络连接 */
    for (idx=0; idx<num; ++idx)
    {
        Close(fd[idx]);
    }

    return 0;
}

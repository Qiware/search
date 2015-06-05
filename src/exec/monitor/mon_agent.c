/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mon_srch.c
 ** 版本号: 1.0
 ** 描  述: 监控代理服务
 **         测试或获取代理服务的详细数据信息.
 ** 注  意: 请勿显示中文，否则将会出现对齐异常!
 ** 作  者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
#include "sck.h"
#include "mesg.h"
#include "syscall.h"
#include "monitor.h"
#include "agent_mesg.h"

#define AGENT_CLIENT_NUM     (50000)

static int mon_agent_search_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_agent_connect(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);

/******************************************************************************
 **函数名称: mon_agent_menu
 **功    能: 代理服务菜单
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 代理服务菜单
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_item_t *mon_agent_menu(menu_cntx_t *ctx, void *args)
{
    menu_item_t *menu;

    menu = menu_creat(ctx, "Monitor Search Engine", NULL, menu_display, NULL, args);
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
    ADD_CHILD(ctx, menu, "Search word", NULL, mon_agent_search_word, NULL, args);
    ADD_CHILD(ctx, menu, "Get current status", NULL, mon_agent_connect, NULL, args);
    ADD_CHILD(ctx, menu, "Test connect", NULL, mon_agent_connect, NULL, args);
    return menu;
}

/******************************************************************************
 **函数名称: mon_agent_search_rep_hdl
 **功    能: 接收搜索应答信息
 **输入参数:
 **     fd: 文件描述符
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.05 17:01:04 #
 ******************************************************************************/
static int mon_agent_search_rep_hdl(int fd)
{
    int n, i;
    char *buff;
    ssize_t size;
    agent_header_t *head;
    mesg_search_rep_t *resp;

    size = sizeof(agent_header_t) + sizeof(mesg_search_rep_t);

    buff = (char *)calloc(1, size);
    if (NULL == buff)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    n = read(fd, buff, size);
    if (n < 0)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    head = (agent_header_t *)buff;
    resp = (mesg_search_rep_t *)(head + 1);

    for (i=0; i<resp->url_num; ++i)
    {
        fprintf(stderr, "    url: %s\n", resp->url[i]);
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_agent_search_word
 **功    能: 搜索单词
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.05 #
 ******************************************************************************/
static int mon_agent_search_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    int fd, ret;
    fd_set rdset;
    time_t ctm;
    char word[1024];
    agent_header_t head;
    srch_mesg_body_t body;
    struct timeval timeout;
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 连接代理服务 */
    fd = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
    if (fd < 0)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    /* 发送搜索请求 */
    fprintf(stderr, "    Word: ");
    scanf(" %s", word);

    head.type = htonl(MSG_SEARCH_REQ);
    head.flag = htonl(AGENT_MSG_FLAG_USR);
    head.mark = htonl(AGENT_MSG_MARK_KEY);
    head.length = htonl(sizeof(body));

    snprintf(body.words, sizeof(body.words), "%s", word);

    Writen(fd, (void *)&head, sizeof(head));
    Writen(fd, (void *)&body, sizeof(body));

    ctm = time(NULL);

    /* 等待应答数据 */
    while (1)
    {
        FD_ZERO(&rdset);

        FD_SET(fd, &rdset);

        timeout.tv_sec = 5 * 60;
        timeout.tv_usec = 0;
        ret = select(fd+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        if (FD_ISSET(fd, &rdset))
        {
            mon_agent_search_rep_hdl(fd);
            fprintf(stderr, "    Spend: %lu(s)\n", time(NULL) - ctm);
            break;
        }
    }

    close(fd);

    return 0;
}

/******************************************************************************
 **函数名称: mon_agent_connect
 **功    能: 测试代理服务处理大并发连接的能力
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 连接代理服务
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static int mon_agent_connect(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    char digit[256];
    int idx, num, max;
    int fd[AGENT_CLIENT_NUM];
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 输入最大连接数 */
    fprintf(stderr, "    Max connections: ");
    scanf(" %s", digit);

    max = atoi(digit);

    /* > 连接代理服务 */
    num = 0;
    for (idx=0; idx<max; ++idx)
    {
        fd[idx] = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
        if (fd[idx] < 0)
        {
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        fprintf(stdout, "    idx:%d fd:%d!\n", idx, fd[idx]);
        ++num;
    }

#if 1
    int n;
    srch_mesg_body_t body;
    agent_header_t header;

    /* 发送搜索数据 */
    for (idx=0; idx<num; ++idx)
    {
        header.type = htonl(MSG_SEARCH_REQ);
        header.flag = htonl(AGENT_MSG_FLAG_USR);
        header.mark = htonl(AGENT_MSG_MARK_KEY);
        header.length = htonl(sizeof(body));

        snprintf(body.words, sizeof(body.words), "爱我中华");

        n = Writen(fd[idx], (void *)&header, sizeof(header));
        if (n < 0)
        {
            break;
        }

        n = Writen(fd[idx], (void *)&body, sizeof(body));
    }
#endif

    if (num <= 0)
    {
        return 0;
    }

    Sleep(5);

    /* 关闭网络连接 */
    for (idx=0; idx<num; ++idx)
    {
        CLOSE(fd[idx]);
    }

    return 0;
}

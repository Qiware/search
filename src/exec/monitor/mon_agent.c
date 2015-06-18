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
static int mon_agent_insert_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_agent_multi_search_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);

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
    ADD_CHILD(ctx, menu, "Multi search word", NULL, mon_agent_multi_search_word, NULL, args);
    ADD_CHILD(ctx, menu, "Insert word", NULL, mon_agent_insert_word, NULL, args);
    ADD_CHILD(ctx, menu, "Test connect", NULL, mon_agent_connect, NULL, args);
    return menu;
}

/******************************************************************************
 **函数名称: mon_agent_search_rsp_hdl
 **功    能: 接收搜索应答信息
 **输入参数:
 **     fd: 文件描述符
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.05 17:01:04 #
 ******************************************************************************/
static int mon_agent_search_rsp_hdl(int fd)
{
    int n, i;
    char *buff;
    ssize_t size;
    agent_header_t *head;
    mesg_search_word_rsp_t *rsp;

    size = sizeof(agent_header_t) + sizeof(mesg_search_word_rsp_t);

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
    rsp = (mesg_search_word_rsp_t *)(head + 1);

    rsp->serial = ntoh64(rsp->serial);
    rsp->url_num = ntohl(rsp->url_num);

    fprintf(stderr, "    Serial: %ld\n", rsp->serial);
    fprintf(stderr, "    url num: %d\n", rsp->url_num);
    for (i=0; i<rsp->url_num; ++i)
    {
        fprintf(stderr, "        [%02d] url: %s\n", i+1, rsp->url[i]);
    }

    free(buff);

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
    int fd, ret, sec, msec;
    fd_set rdset;
    struct timeb ctm, old_tm;
    char word[1024];
    agent_header_t head;
    struct timeval timeout;
    mesg_search_word_req_t req;
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

    head.type = htonl(MSG_SEARCH_WORD_REQ);
    head.flag = htonl(AGENT_MSG_FLAG_USR);
    head.mark = htonl(AGENT_MSG_MARK_KEY);
    head.length = htonl(sizeof(req));

    snprintf(req.words, sizeof(req.words), "%s", word);

    Writen(fd, (void *)&head, sizeof(head));
    Writen(fd, (void *)&req, sizeof(req));

    ftime(&old_tm);

    /* 等待应答数据 */
    while (1)
    {
        FD_ZERO(&rdset);

        FD_SET(fd, &rdset);

        timeout.tv_sec = 10;
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
            mon_agent_search_rsp_hdl(fd);
            ftime(&ctm);
            sec = ctm.time - old_tm.time;
            msec = ctm.millitm - old_tm.millitm;
            if (msec < 0)
            {
                msec += 1000;
                sec -= 1;
            }
            fprintf(stderr, "    Spend: %d.%03d(s)\n", sec, msec);
            break;
        }
    }

    close(fd);

    return 0;
}

/******************************************************************************
 **函数名称: mon_agent_multi_search_word
 **功    能: 搜索单词
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.05 #
 ******************************************************************************/
static int mon_agent_multi_search_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    int sec, msec;
    int *fd, *wflg;
    struct timeb *wrtm, ctm;
    fd_set rdset, wrset;
    agent_header_t head;
    struct timeval timeout;
    mesg_search_word_req_t req;
    int ret, idx, max, num, left;
    char digit[256], word[1024];
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 发送搜索请求 */
    fprintf(stderr, "    Word: ");
    scanf(" %s", word);

    /* > 最大连接数 */
    fprintf(stderr, "    Connections: ");
    scanf(" %s", digit);

    num = atoi(digit);
    fd = (int *)calloc(num, sizeof(int));
    wflg = (int *)calloc(num, sizeof(int));
    wrtm = (struct timeb *)calloc(num, sizeof(struct timeb));

SRCH_AGAIN:
    num = atoi(digit);

    /* > 连接代理服务 */
    for (idx=0; idx<num; ++idx)
    {
        wflg[idx] = 0;
        fd[idx] = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
        if (fd[idx] < 0)
        {
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }

        fprintf(stdout, "    idx:%d fd:%d!\n", idx, fd[idx]);

    }

    num = idx;
    left = num;

    /* 等待应答数据 */
    while (1) {
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        if (0 == left) { break; }

        /* 加入读写集合 */
        max = -1;
        for (idx=0; idx<num; ++idx) {
            if (fd[idx] > 0) {
                FD_SET(fd[idx], &rdset);
                if (0 == wflg[idx]) {
                    FD_SET(fd[idx], &wrset);
                }
                max = (fd[idx] > max)? fd[idx] : max; 
            }
        }

        /* 等待事件通知 */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        ret = select(max+1, &rdset, &wrset, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        else if (0 == ret) {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        /* 进行读写处理 */
        for (idx=0; idx<num; ++idx) {
            if (fd[idx] <= 0) {
                continue;
            }

            if (FD_ISSET(fd[idx], &rdset)) {
                mon_agent_search_rsp_hdl(fd[idx]);
                ftime(&ctm);
                sec = ctm.time - wrtm[idx].time;
                msec = ctm.millitm - wrtm[idx].millitm;
                if (msec < 0) {
                    msec += 1000;
                    sec -= 1;
                }
                fprintf(stderr, "    fd:%d Spend: %d.%03d(s)\n", fd[idx], sec, msec);
                CLOSE(fd[idx]);
                --left;
            }

            if (fd[idx] > 0 && FD_ISSET(fd[idx], &wrset)) {
                head.type = htonl(MSG_SEARCH_WORD_REQ);
                head.flag = htonl(AGENT_MSG_FLAG_USR);
                head.mark = htonl(AGENT_MSG_MARK_KEY);
                head.length = htonl(sizeof(req));

                snprintf(req.words, sizeof(req.words), "%s", word);

                Writen(fd[idx], (void *)&head, sizeof(head));
                Writen(fd[idx], (void *)&req, sizeof(req));

                ftime(&wrtm[idx]);
                wflg[idx] = 1;
            }
        }
    }

    for (idx=0; idx<num; ++idx)
    {
        if (fd[idx] > 0)
        {
            CLOSE(fd[idx]);
        }
    }

    goto SRCH_AGAIN;

    free(fd);
    free(wrtm);

    return 0;
}

/* 接收插入关键字的应答 */
static int mon_agent_insert_word_rsp_hdl(int fd)
{
    int n;
    char *buff;
    ssize_t size;
    agent_header_t *head;
    mesg_insert_word_rsp_t *rsp;

    size = sizeof(agent_header_t) + sizeof(mesg_search_word_rsp_t);

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
    rsp = (mesg_insert_word_rsp_t *)(head + 1);

    rsp->serial = ntoh64(rsp->serial);
    rsp->code = ntoh64(rsp->code);

    fprintf(stderr, "    Serial: %ld\n", rsp->serial);
    fprintf(stderr, "    Code: %d\n", rsp->code);
    fprintf(stderr, "    Word: %s\n", rsp->word);
    free(buff);

    return 0;
}

/* 插入关键字 */
static int mon_agent_insert_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    fd_set rdset;
    char _freq[32];
    int fd, ret, sec, msec;
    struct timeb ctm, old_tm;
    agent_header_t head;
    mesg_insert_word_req_t req;
    struct timeval timeout;
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 输入发送数据 */
    fprintf(stderr, "    Word: ");
    scanf(" %s", req.word);

    fprintf(stderr, "    Url: ");
    scanf(" %s", req.url);

    fprintf(stderr, "    Freq: ");
    scanf(" %s", _freq);
    req.freq = htonl(atoi(_freq));

    /* > 设置发送数据 */
    head.type = htonl(MSG_INSERT_WORD_REQ);
    head.flag = htonl(AGENT_MSG_FLAG_USR);
    head.mark = htonl(AGENT_MSG_MARK_KEY);
    head.length = htonl(sizeof(req));

    /* > 连接代理服务 */
    fd = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
    if (fd < 0)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    Writen(fd, (void *)&head, sizeof(head));
    Writen(fd, (void *)&req, sizeof(req));

    ftime(&old_tm);

    /* 等待应答数据 */
    while (1)
    {
        FD_ZERO(&rdset);

        FD_SET(fd, &rdset);

        timeout.tv_sec = 10;
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
            mon_agent_insert_word_rsp_hdl(fd);
            ftime(&ctm);
            sec = ctm.time - old_tm.time;
            msec = ctm.millitm - old_tm.millitm;
            if (msec < 0)
            {
                msec += 1000;
                sec -= 1;
            }
            fprintf(stderr, "    Spend: %d.%03d(s)\n", sec, msec);
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
    int *fd;
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 输入最大连接数 */
    fprintf(stderr, "    Max connections: ");
    scanf(" %s", digit);

    max = atoi(digit);
    fd = (int *)calloc(max, sizeof(int));

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
    agent_header_t header;
    mesg_search_word_req_t req;

    /* 发送搜索数据 */
    for (idx=0; idx<num; ++idx)
    {
        header.type = htonl(MSG_SEARCH_WORD_REQ);
        header.flag = htonl(AGENT_MSG_FLAG_USR);
        header.mark = htonl(AGENT_MSG_MARK_KEY);
        header.length = htonl(sizeof(req));

        snprintf(req.words, sizeof(req.words), "爱我中华");

        n = Writen(fd[idx], (void *)&header, sizeof(header));
        if (n < 0)
        {
            break;
        }

        n = Writen(fd[idx], (void *)&req, sizeof(req));
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

    free(fd);

    return 0;
}

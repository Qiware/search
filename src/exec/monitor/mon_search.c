/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
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
#include "redo.h"
#include "monitor.h"
#include "agent_mesg.h"

/* 搜索连接 */
typedef struct
{
    int fd;             /* 文件描述符 */
    int flag;           /* 是否完成处理 */
    struct timeval wrtm;/* 请求发送时间 */
} mon_srch_conn_t;

static int mon_srch_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_srch_connect(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_insert_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);
static int mon_srch_word_loop(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);

/******************************************************************************
 **函数名称: mon_srch_menu
 **功    能: 代理服务菜单
 **输入参数:
 **     ctx: 菜单对象
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 代理服务菜单
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_item_t *mon_srch_menu(menu_cntx_t *ctx, void *args)
{
    menu_item_t *menu;

    menu = menu_creat(ctx, "Search Engine", NULL, menu_display, NULL, args);
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
    ADD_CHILD(ctx, menu, "Search word", NULL, mon_srch_word, NULL, args);
    ADD_CHILD(ctx, menu, "Search word - loop", NULL, mon_srch_word_loop, NULL, args);
    ADD_CHILD(ctx, menu, "Insert word", NULL, mon_insert_word, NULL, args);
    ADD_CHILD(ctx, menu, "Test connect", NULL, mon_srch_connect, NULL, args);
    return menu;
}

/* 发送搜索请求 */
static int mon_srch_send_rep(int fd, const char *word)
{
    agent_header_t *head;
    mesg_search_word_req_t *req;
    char addr[sizeof(agent_header_t) + sizeof(mesg_search_word_req_t)];

    head = (agent_header_t *)addr;
    req = (mesg_search_word_req_t *)(head + 1);
    head->type = htonl(MSG_SEARCH_WORD_REQ);
    head->flag = htonl(AGENT_MSG_FLAG_USR);
    head->mark = htonl(AGENT_MSG_MARK_KEY);
    head->length = htonl(sizeof(mesg_search_word_req_t));
    snprintf(req->words, sizeof(req->words), "%s", word);

    Writen(fd, (void *)addr, sizeof(addr));

    return 0;
}

/******************************************************************************
 **函数名称: mon_srch_recv_rsp
 **功    能: 接收搜索应答信息
 **输入参数:
 **     fd: 文件描述符
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.05 17:01:04 #
 ******************************************************************************/
static int mon_srch_recv_rsp(mon_cntx_t *ctx, mon_srch_conn_t *conn)
{
    serial_t serial;
    struct timeval ctm;
    int n, i, sec, msec, usec;
    agent_header_t *head;
    mesg_search_word_rsp_t *rsp;
    char addr[sizeof(agent_header_t) + sizeof(mesg_search_word_rsp_t)];

    /* > 接收应答数据 */
    n = read(conn->fd, addr, sizeof(addr));
    gettimeofday(&ctm, NULL);
    if (n <= 0)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }
    else if (0 == conn->wrtm.tv_sec)
    {
        fprintf(stderr, "    Didn't send search request but received response!\n");
    }

    /* > 显示查询结果 */
    fprintf(stderr, "    ============================================\n");
    head = (agent_header_t *)addr;
    rsp = (mesg_search_word_rsp_t *)(head + 1);

    rsp->serial = ntoh64(rsp->serial);
    rsp->url_num = ntohl(rsp->url_num);

    serial.serial = rsp->serial;
    fprintf(stderr, "    >Serial: %lu - gid(%u) sid(%u) seq(%u)\n",
            serial.serial, serial.nid, serial.sid, serial.seq);
    fprintf(stderr, "    >UrlNum: %d\n", rsp->url_num);
    for (i=0; i<rsp->url_num; ++i)
    {
        fprintf(stderr, "        -[%02d] url: %s\n", i+1, rsp->url[i]);
    }
    /* > 打印统计信息 */
    sec = ctm.tv_sec - conn->wrtm.tv_sec;
    msec = (ctm.tv_usec - conn->wrtm.tv_usec)/1000;
    if (msec < 0)
    {
        sec -= 1;
        msec += 1000;
    }
    usec = (ctm.tv_usec - conn->wrtm.tv_usec)%1000;
    if (usec < 0)
    {
        usec += 1000;
        msec -= 1;
        if (msec < 0)
        {
            sec -= 1;
            msec += 1000;
        }
    }
    if (msec < 0)
    {
        msec += 1000;
        sec -= 1;
    }
    fprintf(stderr, "    >Spend: %d(s).%03d(ms).%03d(us)\n", sec, msec, usec);
    fprintf(stderr, "    ============================================\n");

    return 0;
}

/******************************************************************************
 **函数名称: mon_srch_word
 **功    能: 搜索单词
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.05 #
 ******************************************************************************/
static int mon_srch_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    int ret;
    fd_set rdset;
    mon_srch_conn_t conn;
    char word[1024];
    struct timeval timeout;
    mon_cntx_t *ctx = (mon_cntx_t *)args;
    ip_port_t *conf = &ctx->conf->search;

    memset(&conn, 0, sizeof(conn));

    /* > 连接代理服务 */
    conn.fd = tcp_connect(AF_INET, conf->ip, conf->port);
    if (conn.fd < 0)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    /* 输入搜索内容 */
    fprintf(stderr, "    Word: ");
    scanf(" %s", word);

    /* 发送搜索请求 */
    mon_srch_send_rep(conn.fd, word);

    gettimeofday(&conn.wrtm, NULL);

    /* 等待应答数据 */
    while (1)
    {
        FD_ZERO(&rdset);

        FD_SET(conn.fd, &rdset);

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        ret = select(conn.fd+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno) { continue; }
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        else if (0 == ret)
        {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        if (FD_ISSET(conn.fd, &rdset))
        {
            mon_srch_recv_rsp(ctx, &conn);
            break;
        }
    }

    CLOSE(conn.fd);

    return 0;
}

/******************************************************************************
 **函数名称: mon_srch_word_loop
 **功    能: 循环搜索单词
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 此处使用的是select(), 因此最大文件描述符的"值"(而不是个数)不能超过FD_SETSIZE(1024)!
 **          否则，在设置rdset, wrset时，将会出现栈溢出, 导致不可预测的错误出现!
 **作    者: # Qifeng.zou # 2015.06.05 #
 ******************************************************************************/
static int mon_srch_word_loop(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
#define MON_FD_MAX  (900)
    int is_unrecv, unrecv_num;
    fd_set rdset, wrset;
    struct timeval timeout;
    mon_srch_conn_t *conn;
    int ret, idx, max, num, left;
    char digit[256], word[1024];
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 发送搜索请求 */
    fprintf(stderr, "    Word: ");
    scanf(" %s", word);

    /* > 最大连接数 */
    fprintf(stderr, "    Connections: ");
    scanf(" %s", digit);

    num = MIN(atoi(digit), MON_FD_MAX);
    conn = (mon_srch_conn_t *)slab_alloc(ctx->slab, num*sizeof(mon_srch_conn_t));
    if (NULL == conn)
    {
        fprintf(stderr, "    Alloc memory failed!\n");
        return -1;
    }

SRCH_AGAIN:
    is_unrecv = false;
    memset(conn, 0, num * sizeof(mon_srch_conn_t));

    num = MIN(atoi(digit), MON_FD_MAX);

    /* > 连接代理服务 */
    for (idx=0; idx<num; ++idx)
    {
        conn[idx].flag = 0;
        conn[idx].fd = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
        if (conn[idx].fd < 0)
        {
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }

        fprintf(stdout, "    Connect success! idx:%d fd:%d\n", idx, conn[idx].fd);
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
            if (conn[idx].fd > 0) {
                FD_SET(conn[idx].fd, &rdset);
                if (0 == conn[idx].flag) {
                    FD_SET(conn[idx].fd, &wrset);
                }
                max = (conn[idx].fd > max)? conn[idx].fd : max; 
            }
        }

        /* 等待事件通知 */
        timeout.tv_sec = 15;
        timeout.tv_usec = 0;
        ret = select(max+1, &rdset, &wrset, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            log_error(ctx->log, "errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        else if (0 == ret) {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        /* 进行读写处理 */
        for (idx=0; idx<num; ++idx) {
            if (conn[idx].fd <= 0) { continue; }

            if (FD_ISSET(conn[idx].fd, &rdset))
            {
                mon_srch_recv_rsp(ctx, &conn[idx]);
                CLOSE(conn[idx].fd);
                --left;
                continue;
            }

            if (FD_ISSET(conn[idx].fd, &wrset)) {
                mon_srch_send_rep(conn[idx].fd, word);
                gettimeofday(&conn[idx].wrtm, NULL);
                conn[idx].flag = 1;
            }
        }
    }

    unrecv_num = 0;
    for (idx=0; idx<num; ++idx)
    {
        if (conn[idx].fd > 0)
        {
            ++unrecv_num;
            is_unrecv = true;
            CLOSE(conn[idx].fd);
        }
    }

    if (is_unrecv)
    {
        log_error(ctx->log, "Didn't receive response! num:%d", unrecv_num);
    }

    goto SRCH_AGAIN;

    slab_dealloc(ctx->slab, conn); 

    return 0;
}

/* 接收插入关键字的应答 */
static int mon_insert_word_rsp_hdl(mon_cntx_t *ctx, int fd)
{
    int n;
    char *addr;
    ssize_t size;
    agent_header_t *head;
    mesg_insert_word_rsp_t *rsp;

    /* > 申请内存空间 */
    size = sizeof(agent_header_t) + sizeof(mesg_insert_word_rsp_t);

    addr = (char *)slab_alloc(ctx->slab, size);
    if (NULL == addr)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    /* > 接收应答数据 */
    n = read(fd, addr, size);
    if (n <= 0)
    {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        slab_dealloc(ctx->slab, addr);
        return -1;
    }

    /* > 显示插入结果 */
    head = (agent_header_t *)addr;
    rsp = (mesg_insert_word_rsp_t *)(head + 1);

    rsp->serial = ntoh64(rsp->serial);
    rsp->code = ntoh64(rsp->code);

    fprintf(stderr, "    Serial: %ld\n", rsp->serial);
    fprintf(stderr, "    Code: %d\n", rsp->code);
    fprintf(stderr, "    Word: %s\n", rsp->word);

    slab_dealloc(ctx->slab, addr);

    return 0;
}

/* 插入关键字 */
static int mon_insert_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
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
            if (EINTR == errno) { continue; }
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
            mon_insert_word_rsp_hdl(ctx, fd);
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
 **函数名称: mon_srch_connect
 **功    能: 测试代理服务处理大并发连接的能力
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 连接代理服务
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static int mon_srch_connect(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    char digit[256];
    int idx, num, max;
    int *fd;
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 输入最大连接数 */
    fprintf(stderr, "    Max connections: ");
    scanf(" %s", digit);

    max = atoi(digit);
    fd = (int *)slab_alloc(ctx->slab, max*sizeof(int));

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
        fprintf(stdout, "    Connect success! idx:%d fd:%d\n", idx, fd[idx]);
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

    slab_dealloc(ctx->slab, fd);

    return 0;
}

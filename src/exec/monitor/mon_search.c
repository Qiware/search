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
#include "xml_tree.h"

#define MON_SRCH_BODY_LEN   (1024)

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
static int mon_srch_set_body(const char *word, char *body, int size);

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
    if (NULL == menu) {
        return NULL;
    }

#define ADD_CHILD(ctx, menu, title, entry, func, exit, args) \
    if (!menu_child(ctx, menu, title, entry, func, exit, args)) { \
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
static int mon_srch_send_req(int fd, const char *word)
{
    int len;
    char *body;
    mesg_header_t *head;
    char addr[sizeof(mesg_header_t) + MON_SRCH_BODY_LEN];

    head = (mesg_header_t *)addr;
    body = (char *)(head + 1);

    len = mon_srch_set_body(word, body, MON_SRCH_BODY_LEN);
    if (len < 0) {
        fprintf(stderr, "    Get search body failed! word:%s\n", word);
        return -1;
    }

    MESG_HEAD_SET(head, MSG_SEARCH_REQ, 0, len);
    MESG_HEAD_HTON(head, head);

    Writen(fd, (void *)addr, sizeof(mesg_header_t)+len);

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
    int idx;
    ssize_t n;
    char addr[8192];
    serial_t serial;
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node, *attr;
    struct timeval ctm;
    int sec, msec, usec;
    mesg_header_t *head;

    memset(addr, 0, sizeof(addr));

    /* > 接收应答数据 */
    n = read(conn->fd, (void *)addr, sizeof(mesg_header_t));
    gettimeofday(&ctm, NULL);
    if (n <= 0) {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }
    else if (0 == conn->wrtm.tv_sec) {
        fprintf(stderr, "    Didn't send search request but received response!\n");
    }

    /* > 字节序转换 */
    head = (mesg_header_t *)addr;
    MESG_HEAD_NTOH(head, head);

    n = read(conn->fd, addr+sizeof(mesg_header_t), head->length);
    if (n != head->length) {
        fprintf(stderr, "    Get data failed! n:%ld/%d\n", n, head->length);
        return -1;
    }

    /* > 显示查询结果 */
    fprintf(stderr, "    ============================================\n");

    serial.serial = head->serial;
    fprintf(stderr, "    >Serial: %lu [nid(%u) svrid(%u) seq(%u)]\n",
            head->serial, serial.nid, serial.svrid, serial.seq);

    memset(&opt, 0, sizeof(opt));

    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_screat(head->body, head->length, &opt);
    if (NULL == xml) { 
        fprintf(stderr, "    Format isn't right! body:%s\n", head->body);
        return -1;
    }
    
    node = xml_query(xml, ".SEARCH-RSP.ITEM");
    for (idx=1; NULL != node; node = xml_brother(node), ++idx) {
        attr = xml_search(xml, node, "URL");
        fprintf(stderr, "        [%02d] URL:%s", idx, attr->value.str);

        attr = xml_search(xml, node, "FREQ");
        fprintf(stderr, "    FREQ:%s\n", attr->value.str);
    }

    xml_destroy(xml);

    /* > 打印统计信息 */
    sec = ctm.tv_sec - conn->wrtm.tv_sec;
    msec = (ctm.tv_usec - conn->wrtm.tv_usec)/1000;
    if (msec < 0) {
        sec -= 1;
        msec += 1000;
    }
    usec = (ctm.tv_usec - conn->wrtm.tv_usec)%1000;
    if (usec < 0) {
        usec += 1000;
        msec -= 1;
        if (msec < 0) {
            sec -= 1;
            msec += 1000;
        }
    }
    if (msec < 0) {
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
    if (conn.fd < 0) {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    /* 输入搜索内容 */
    fprintf(stderr, "    Word: ");
    scanf(" %s", word);

    /* 发送搜索请求 */
    mon_srch_send_req(conn.fd, word);

    gettimeofday(&conn.wrtm, NULL);

    /* 等待应答数据 */
    while (1) {
        FD_ZERO(&rdset);

        FD_SET(conn.fd, &rdset);

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        ret = select(conn.fd+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        else if (0 == ret) {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        if (FD_ISSET(conn.fd, &rdset)) {
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
    conn = (mon_srch_conn_t *)calloc(1, num*sizeof(mon_srch_conn_t));
    if (NULL == conn) {
        fprintf(stderr, "    Alloc memory failed!\n");
        return -1;
    }

SRCH_AGAIN:
    is_unrecv = false;
    memset(conn, 0, num * sizeof(mon_srch_conn_t));

    num = MIN(atoi(digit), MON_FD_MAX);

    /* > 连接代理服务 */
    for (idx=0; idx<num; ++idx) {
        conn[idx].flag = 0;
        conn[idx].fd = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
        if (conn[idx].fd < 0) {
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
            break;
        }
        else if (0 == ret) {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        /* 进行读写处理 */
        for (idx=0; idx<num; ++idx) {
            if (conn[idx].fd <= 0) { continue; }

            if (FD_ISSET(conn[idx].fd, &rdset)) {
                mon_srch_recv_rsp(ctx, &conn[idx]);
                CLOSE(conn[idx].fd);
                --left;
                continue;
            }

            if (FD_ISSET(conn[idx].fd, &wrset)) {
                mon_srch_send_req(conn[idx].fd, word);
                gettimeofday(&conn[idx].wrtm, NULL);
                conn[idx].flag = 1;
            }
        }
    }

    unrecv_num = 0;
    for (idx=0; idx<num; ++idx) {
        if (conn[idx].fd > 0) {
            ++unrecv_num;
            is_unrecv = true;
            CLOSE(conn[idx].fd);
        }
    }

    if (is_unrecv) {
        fprintf(stderr, "Didn't receive response! num:%d", unrecv_num);
    }

    goto SRCH_AGAIN;

    FREE(conn); 

    return 0;
}

/* 接收插入关键字的应答 */
static int mon_insert_word_rsp_hdl(mon_cntx_t *ctx, int fd)
{
    int n;
    serial_t s;
    mesg_header_t *head;
    mesg_insert_word_rsp_t *rsp;
    char addr[sizeof(mesg_header_t) + sizeof(mesg_insert_word_rsp_t)];

    /* > 接收应答数据 */
    n = read(fd, addr, sizeof(addr));
    if (n <= 0) {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    /* > 显示插入结果 */
    head = (mesg_header_t *)addr;
    rsp = (mesg_insert_word_rsp_t *)(head + 1);

    MESG_HEAD_NTOH(head, head);
    mesg_insert_word_resp_ntoh(rsp);

    s.serial = head->serial;
    fprintf(stderr, "    Serial: %ld(nid:%u svrid:%u seq:%u)\n",
            head->serial, s.nid, s.svrid, s.seq);
    fprintf(stderr, "    Code: %d\n", rsp->code);
    fprintf(stderr, "    Word: %s\n", rsp->word);

    return 0;
}

/* 插入关键字 */
static int mon_insert_word(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    fd_set rdset;
    char _freq[32];
    int fd, ret, sec, msec;
    struct timeb ctm, old_tm;
    mesg_header_t head;
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
    MESG_HEAD_SET(&head, MSG_INSERT_WORD_REQ, 0, sizeof(req));
    MESG_HEAD_HTON(&head, &head);

    /* > 连接代理服务 */
    fd = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
    if (fd < 0) {
        fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    Writen(fd, (void *)&head, sizeof(head));
    Writen(fd, (void *)&req, sizeof(req));

    ftime(&old_tm);

    /* 等待应答数据 */
    while (1) {
        FD_ZERO(&rdset);

        FD_SET(fd, &rdset);

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        ret = select(fd+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        else if (0 == ret) {
            fprintf(stderr, "    Timeout!\n");
            break;
        }

        if (FD_ISSET(fd, &rdset)) {
            mon_insert_word_rsp_hdl(ctx, fd);
            ftime(&ctm);
            sec = ctm.time - old_tm.time;
            msec = ctm.millitm - old_tm.millitm;
            if (msec < 0) {
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
    fd = (int *)calloc(1, max*sizeof(int));

    /* > 连接代理服务 */
    num = 0;
    for (idx=0; idx<max; ++idx) {
        fd[idx] = tcp_connect(AF_INET, ctx->conf->search.ip, ctx->conf->search.port);
        if (fd[idx] < 0) {
            fprintf(stderr, "    errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
        fprintf(stdout, "    Connect success! idx:%d fd:%d\n", idx, fd[idx]);
        ++num;
    }

#if 1
    /* 发送搜索数据 */
    for (idx=0; idx<num; ++idx) {
        mon_srch_send_req(fd[idx], "爱我中华");
    }
#endif

    if (num <= 0) {
        return 0;
    }

    Sleep(5);

    /* 关闭网络连接 */
    for (idx=0; idx<num; ++idx) {
        CLOSE(fd[idx]);
    }

    FREE(fd);

    return 0;
}

/******************************************************************************
 **函数名称: mon_srch_set_body
 **功    能: 设置搜索报体
 **输入参数:
 **     word: 搜索信息
 **     size: 搜索请求的最大报体
 **输出参数: NONE
 **     body: 搜索请求的报体
 **返    回: 报体实际长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.12.27 03:01:48 #
 ******************************************************************************/
static int mon_srch_set_body(const char *words, char *body, int size)
{
    int len;
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;

    memset(&opt, 0, sizeof(opt));

    /* > 创建XML树 */
    opt.log = NULL;
    opt.pool = NULL;
    opt.alloc = mem_alloc;
    opt.dealloc = mem_dealloc;

    xml = xml_empty(&opt);
    if (NULL == xml) {
        fprintf(stderr, "Create xml failed!");
        return -1;
    }

    node = xml_add_child(xml, xml->root, "SEARCH", NULL);
    xml_add_attr(xml, node, "WORDS", words);

    /* > 计算XML长度 */
    len = XML_PACK_LEN(xml);
    if (len >= size) {
        xml_destroy(xml);
        return -1;
    }

    /* > 输出XML至缓存 */
    len = xml_spack(xml, body);

    xml_destroy(xml);

    return len;
}

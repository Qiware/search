/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mon_srch.c
 ** 版本号: 1.0
 ** 描  述: 监控探针服务
 **         测试或获取探针服务的详细数据信息.
 ** 注  意: 请勿显示中文，否则将会出现对齐异常!
 ** 作  者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
#include "sck.h"
#include "mesg.h"
#include "syscall.h"
#include "monitor.h"
#include "gate_mesg.h"

#define GATE_CLIENT_NUM     (50000)

static int mon_gate_connect(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args);

/******************************************************************************
 **函数名称: mon_gate_menu
 **功    能: 探针服务菜单
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 探针服务菜单
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_item_t *mon_gate_menu(menu_cntx_t *ctx, void *args)
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
    ADD_CHILD(ctx, menu, "Get configuration", NULL, mon_gate_connect, NULL, args);
    ADD_CHILD(ctx, menu, "Get current status", NULL, mon_gate_connect, NULL, args);
    ADD_CHILD(ctx, menu, "Test connect", NULL, mon_gate_connect, NULL, args);
    return menu;
}

/******************************************************************************
 **函数名称: mon_gate_connect
 **功    能: 测试探针服务处理大并发连接的能力
 **输入参数:
 **     menu: 菜单
 **输出参数: NONE
 **返    回: 连接探针服务
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static int mon_gate_connect(menu_cntx_t *menu_ctx, menu_item_t *menu, void *args)
{
    char digit[256];
    int idx, num, max;
    int fd[GATE_CLIENT_NUM];
    mon_cntx_t *ctx = (mon_cntx_t *)args;

    /* > 输入最大连接数 */
    fprintf(stderr, "    Max connections: ");
    scanf(" %s", digit);

    max = atoi(digit);

    /* > 连接探针服务 */
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
    gate_mesg_header_t header;

    /* 发送搜索数据 */
    for (idx=0; idx<num; ++idx)
    {
        header.type = htonl(MSG_SEARCH_REQ);
        header.flag = htonl(GATE_MSG_FLAG_USR);
        header.mark = htonl(GATE_MSG_MARK_KEY);
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

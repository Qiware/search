/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: menu.c
 ** 版本号: 1.0
 ** 描  述: 菜单引擎
 **         用于管理界面显示和菜单功能的执行
 ** 作  者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
#include "str.h"
#include "menu.h"
#include "comm.h"

/******************************************************************************
 **函数名称: menu_init
 **功    能: 初始化上下文
 **输入参数:
 **     title: 主标题
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_cntx_t *menu_init(const char *title, menu_conf_t *conf)
{
    menu_cntx_t *ctx;

    /* > 创建全局对象 */
    ctx = (menu_cntx_t *)calloc(1, sizeof(menu_cntx_t));
    if (NULL == ctx) {
        free(ctx);
        return NULL;
    }

    memcpy(&ctx->conf, conf, sizeof(menu_conf_t));

    /* > 创建内存池 */
    ctx->pool = mem_pool_creat(1 * KB);
    if (NULL == ctx->pool) {
        free(ctx);
        return NULL;
    }

    ctx->alloc = (mem_alloc_cb_t)mem_pool_alloc;
    ctx->dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    /* > 创建主菜单 */
    ctx->menu = menu_creat(ctx, title, NULL, menu_display, NULL, NULL);
    if (NULL == ctx->menu) {
        mem_pool_destroy(ctx->pool);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: menu_creat
 **功    能: 新建菜单
 **输入参数:
 **     ctx: 全局对象
 **     name: 菜单名
 **     entry: 进入菜单的处理
 **     func: 菜单功能
 **     exit: 退出菜单的处理
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_item_t *menu_creat(menu_cntx_t *ctx, const char *name,
        menu_cb_t entry, menu_cb_t func, menu_cb_t exit, void *args)
{
    menu_item_t *menu;

    menu = (menu_item_t *)ctx->alloc(ctx->pool, sizeof(menu_item_t));
    if (NULL == menu) {
        return NULL;
    }

    menu->num = 0;
    menu->child = NULL;
    menu->parent = NULL;
    menu->next = NULL;

    menu_set_name(menu, name);
    menu_set_func(menu, entry, func, exit);
    menu_set_args(menu, args);

    return menu;
}

/******************************************************************************
 **函数名称: menu_add
 **功    能: 添加子菜单
 **输入参数:
 **     menu: 菜单
 **     child: 子菜单
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int menu_add(menu_item_t *menu, menu_item_t *child)
{
    menu_item_t *item = menu->child;

    if (NULL == item) {
        menu->child = child;
        child->parent = menu;
        child->next = NULL;
        ++menu->num;
        return 0;
    }

    while (NULL != item->next) {
        item = item->next;
    }

    item->next = child;
    child->parent = menu;
    child->next = NULL;
    ++menu->num;

    return 0;
}

/******************************************************************************
 **函数名称: menu_child
 **功    能: 创建并添加子菜单
 **输入参数:
 **     ctx: 全局对象
 **     parent: 父菜单
 **     name: 菜单名
 **     entry: 进入菜单的处理
 **     func: 菜单功能
 **     exit: 退出菜单的处理
 **     args: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.18 #
 ******************************************************************************/
menu_item_t *menu_child(menu_cntx_t *ctx, menu_item_t *parent,
        const char *name, menu_cb_t entry, menu_cb_t func, menu_cb_t exit, void *args)
{
    menu_item_t *child;

    /* > 创建子菜单 */
    child = menu_creat(ctx, name, entry, func, exit, args);
    if (NULL == child) {
        return NULL;
    }

    /* > 插入子菜单 */
    menu_add(parent, child);

    return child;
}

/******************************************************************************
 **函数名称: menu_display
 **功    能: 显示菜单
 **输入参数:
 **     ctx: 全局对象
 **     menu: 需要显示的菜单
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int menu_display(menu_cntx_t *ctx, menu_item_t *menu, void *args)
{
    int i, off, len, n;
    menu_item_t *child = menu->child;
    menu_conf_t *conf = &ctx->conf;

    /* 1. Display top line */
    fprintf(stderr, "\n╔");
    for (i=0; i<conf->width; ++i) {
        fprintf(stderr, "═");
    }
    fprintf(stderr, "╗\n");

    /* 2. Display title */
    len = strlen(menu->name);
    off = (conf->width - len)/2;
    fprintf(stderr, "║");
    for (i=0; i<off; ++i) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "%s", menu->name);
    for (i=off+len; i<conf->width; ++i) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "║\n");

    /* 3. Display sep line */
    fprintf(stderr, "║");
    for (i=0; i<conf->width; ++i) {
        fprintf(stderr, "═");
    }
    fprintf(stderr, "║\n");

    /* 4. Display child menu */
    for (n=0; NULL != child; child = child->next, ++n) {
        fprintf(stderr, "║");

        for (i=0; i<MENU_TAB_LEN; ++i) {
            fprintf(stderr, " ");
        }

        fprintf(stderr, "%2d: %s", n+1, child->name);

        len = strlen(child->name)+4;

        for (i=len+MENU_TAB_LEN; i<conf->width; ++i) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "║\n");
    }

    /* 5. Display low line */
    fprintf(stderr, "╚");
    for (i=0; i<conf->width; ++i) {
        fprintf(stderr, "═");
    }
    fprintf(stderr, "╝\n");

    return 0;
}

/******************************************************************************
 **函数名称: menu_exec
 **功    能: 执行菜单的功能
 **输入参数:
 **     ctx: 全局对象
 **     menu: 需要显示的菜单
 **     opt: 操作选项
 **输出参数: NONE
 **返    回: 当前显示的菜单项
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static menu_item_t *menu_exec(menu_cntx_t *ctx, menu_item_t *menu, const char *opt)
{
    char input[MENU_INPUT_LEN];
    int ret, i, num;
    menu_item_t *child = menu->child;

    if (!strcasecmp(opt, "q")
        || !strcasecmp(opt, "quit")
        || !strcasecmp(opt, "exit") )
    {
        if (!menu->parent) {
            fprintf(stderr, "    Are you sure exit?[N/y] ");
            if (scanf(" %s", input) < 0) {
                return NULL;
            }

            if (0 == strcasecmp(input, "y")
                || 0 == strcasecmp(input, "yes"))
            {
                if (menu->exit) {
                    menu->exit(ctx, menu, menu->args);
                }
                exit(0);
                return NULL;
            }

            menu->func(ctx, menu, menu->args);

            return menu;
        }

        if (menu->exit) {
            menu->exit(ctx, menu, menu->args);
        }

        menu->parent->func(ctx, menu->parent, menu->parent->args);

        return menu->parent;
    }

    num = atoi(opt);
    if ((num <= 0) || (num > menu->num)) {
        fprintf(stderr, "\n    Not right!\n");
        menu->func(ctx, menu, menu->args);
        return menu;
    }

    for (i=1; NULL != child; child = child->next, ++i) {
        if (i == num) {
            if (child->entry) {
                child->entry(ctx, child, child->args);
            }

            if (child->func) {
                ret = child->func(ctx, child, child->args);
                if (0 != ret                /* 失败或无子菜单时，均返回到上级菜单 */
                    || NULL == child->child)
                {
                    break;
                }
                return child;
            }
            fprintf(stderr, "    errmsg: Didn't register function!\n");
            break;
        }
    }

    menu->func(ctx, menu, menu->args);

    return menu;
}

/******************************************************************************
 **函数名称: menu_run
 **功    能: 启动菜单
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int menu_run(menu_cntx_t *ctx)
{
    menu_item_t *curr = ctx->menu;
    char opt[MENU_INPUT_LEN];

    if (curr->entry) {
        curr->entry(ctx, curr, curr->args);
    }

    curr->func(ctx, curr, curr->args);

    while (1) {
    BEGIN:
        fprintf(stderr, "Option: ");
        if (scanf(" %s", opt) < 0) {
            return 0;
        }

        if (!str_isdigit(opt)) {
            if (0 != strcasecmp(opt, "q")
                && 0 != strcasecmp(opt, "quit")
                && 0 != strcasecmp(opt, "exit"))
            {
                fprintf(stderr, "\n    Not right!\n");

                curr->func(ctx, curr, curr->args);
                goto BEGIN;
            }
            goto EXEC;
        }

    EXEC:
        curr = menu_exec(ctx, curr, opt);
    }

    return 0;
}

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: menu.c
 ** 版本号: 1.0
 ** 描  述: 菜单引擎
 **         用于管理界面显示和菜单功能的执行
 ** 作  者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
#include "menu.h"
#include "common.h"


/******************************************************************************
 **函数名称: menu_cntx_init
 **功    能: 初始化上下文
 **输入参数:
 **     menu: 菜单
 **     child: 子菜单
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_cntx_t *menu_cntx_init(const char *title)
{
    menu_cntx_t *ctx;
    mem_pool_t *pool;

    pool = mem_pool_creat(1 * KB);
    if (NULL == pool)
    {
        return NULL;
    }

    ctx = (menu_cntx_t *)mem_pool_alloc(pool, sizeof(menu_cntx_t));
    if (NULL == ctx)
    {
        free(pool);
        return NULL;
    }

    ctx->pool = pool;

    ctx->menu = menu_creat(ctx, title, menu_display);
    if (NULL == ctx->menu)
    {
        free(pool);
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: menu_creat
 **功    能: 新建菜单
 **输入参数:
 **     menu: 菜单
 **     child: 子菜单
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
menu_item_t *menu_creat(menu_cntx_t *ctx, const char *name, int (*func)(menu_item_t *))
{
    menu_item_t *menu;

    menu = (menu_item_t *)mem_pool_alloc(ctx->pool, sizeof(menu_item_t));
    if (NULL == menu)
    {
        return NULL;
    }

    menu_set_name(menu, name);
    menu_set_func(menu, func);

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

    if (NULL == item)
    {
        menu->child = child;
        child->parent = menu;
        child->next = NULL;
        ++menu->num;
        return 0;
    }

    while (NULL != item->next)
    {
        item = item->next;
    }

    item->next = child;
    child->parent = menu;
    child->next = NULL;
    ++menu->num;

    return 0;
}

/******************************************************************************
 **函数名称: menu_display
 **功    能: 显示菜单
 **输入参数:
 **     menu: 需要显示的菜单
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int menu_display(menu_item_t *menu)
{
    int i, off, len, n;
    menu_item_t *child = menu->child;

    /* 1. Display top line */ 
    fprintf(stdout, "\n╔");
    for (i=0; i<MENU_WIDTH; ++i)
    {
        fprintf(stdout, "═");
    }
    fprintf(stdout, "╗\n");

    /* 2. Display title */
    len = strlen(menu->name);
    off = (MENU_WIDTH - len)/2;
    fprintf(stdout, "║");
    for (i=0; i<off; ++i)
    {
        fprintf(stdout, " ");
    }
    fprintf(stdout, "%s", menu->name);
    for (i=off+len; i<MENU_WIDTH; ++i)
    {
        fprintf(stdout, " ");
    }
    fprintf(stdout, "║\n");

    /* 3. Display sep line */
    fprintf(stdout, "║");
    for (i=0; i<MENU_WIDTH; ++i)
    {
        fprintf(stdout, "═");
    }
    fprintf(stdout, "║\n");

    /* 4. Display child menu */
    for (n=0; NULL != child; child = child->next, ++n)
    {
        fprintf(stdout, "║");

        for (i=0; i<MENU_TAB_LEN; ++i)
        {
            fprintf(stdout, " ");
        }

        fprintf(stdout, "%2d: %s", n, child->name);

        len = strlen(child->name)+4;

        for (i=len+MENU_TAB_LEN; i<MENU_WIDTH; ++i)
        {
            fprintf(stdout, " ");
        }
        fprintf(stdout, "║\n");
    }

    /* 5. Display low line */ 
    fprintf(stdout, "╚");
    for (i=0; i<MENU_WIDTH; ++i)
    {
        fprintf(stdout, "═");
    }
    fprintf(stdout, "╝\n");

    return 0;
}

/******************************************************************************
 **函数名称: menu_exec
 **功    能: 执行菜单的功能
 **输入参数:
 **     menu: 需要显示的菜单
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static menu_item_t *menu_exec(menu_item_t *menu, const char *opt)
{
    int ret, i, num;
    menu_item_t *child = menu->child;

    if (!strcasecmp(opt, "q")
        || !strcasecmp(opt, "quit")
        || !strcasecmp(opt, "exit") )
    {
        if (!menu->parent)
        {
            fprintf(stderr, "\t退出系统!");
            exit(0);
            return NULL;
        }

        menu->parent->func(menu->parent);

        return menu->parent;
    }

    num = atoi(opt);

    for (i=0; NULL != child; child = child->next, ++i)
    {
        if (i == num)
        {
            ret = child->func(child);
            if (0 != ret                /* 失败或无子菜单时，均返回到上级菜单 */
                || NULL == child->child)
            {
                break;
            }
            return child;
        }
    }

    menu->func(menu);

    return menu;
}

/******************************************************************************
 **函数名称: menu_startup
 **功    能: 启动菜单
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int menu_startup(menu_cntx_t *ctx)
{
    int idx, len;
    menu_item_t *curr = ctx->menu;
    char opt[MENU_INPUT_LEN];

    curr->func(curr);

    while (1)
    {
    BEGIN:
        fprintf(stdout, "Option: ");
        scanf(" %s", opt);

        len = strlen(opt);
        for (idx=0; idx<len; ++idx)
        {
            if (!isdigit(opt[idx]))
            {
                if (0 != strcasecmp(opt, "q")
                    && 0 != strcasecmp(opt, "quit")
                    && 0 != strcasecmp(opt, "exit"))
                {
                    fprintf(stdout, "\n\tNot right!\n");

                    curr->func(curr);
                    goto BEGIN;
                }
                goto EXEC;
            }
        }

    EXEC:
        curr = menu_exec(curr, opt);
    }

    return 0;
}

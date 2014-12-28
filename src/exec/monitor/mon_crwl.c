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
#include "monitor.h"


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
    child = menu_creat(ctx, "Connect to Crawler", NULL);
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

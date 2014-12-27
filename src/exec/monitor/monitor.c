/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: monitor.c
 ** 版本号: 1.0
 ** 描  述: 监控模块
 **         获取平台运行的详细数据信息.(Note: Don't use display chinese!)
 ** 作  者: # Qifeng.zou # 2014.12.26 #
 ******************************************************************************/
#include <memory.h>
#include <strings.h>

#include "common.h"
#include "monitor.h"

static int mon_load_menu(menu_cntx_t *ctx);

/******************************************************************************
 **函数名称: main 
 **功    能: 监控主程序
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
int main(void)
{
    menu_cntx_t *ctx;

    /* 1. 初始化菜单 */
    ctx = menu_cntx_init("Monitor System");

    /* 2. 添加子菜单 */
    if (mon_load_menu(ctx))
    {
        fprintf(stderr, "Load menu failed!\n");
        return -1;
    }

    /* 3. 启动菜单 */
    if (menu_startup(ctx))
    {
        fprintf(stderr, "Startup menu failed!\n");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: mon_load_menu
 **功    能: 加载子菜单
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化菜单环境
 **     2. 加载子菜单
 **     3. 启动菜单功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.27 #
 ******************************************************************************/
static int mon_load_menu(menu_cntx_t *ctx)
{
    menu_item_t *child;

    /* 1. 加载搜索引擎菜单 */
    child = mon_srch_menu(ctx);
    menu_add(ctx->menu, child);

    /* 2. 加载爬虫系统菜单 */
//    child = mon_crwl_menu();
//    menu_add(ctx->menu, child);

    return 0;
}

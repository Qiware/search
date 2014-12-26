#if !defined(__MENU_H__)
#define __MENU_H__

#include "mem_pool.h"

#define MENU_NAME_STR_LEN   (128)
#define MENU_INPUT_LEN      (256)

/* 菜单 */
typedef struct _menu_item_t
{
    char name[MENU_NAME_STR_LEN];           /* 菜单名 */

    int (*func)(struct _menu_item_t *);     /* 菜单对应的功能 */

    struct _menu_item_t *child;             /* 子菜单列表 */
    struct _menu_item_t *parent;            /* 父菜单 */
    struct _menu_item_t *next;              /* 兄弟菜单 */
} menu_item_t;

#define menu_set_name(menu, nm) snprintf((menu)->name, sizeof((menu)->name), "%s", nm)
#define menu_set_func(menu, f) (menu)->func = (f)

/* 全局信息 */
typedef struct
{
    mem_pool_t *pool;                       /* 内存池 */
    menu_item_t *menu;                      /* 主菜单 */
} menu_cntx_t;

menu_cntx_t *menu_cntx_init(const char *title);
menu_item_t *menu_creat(menu_cntx_t *ctx, const char *name, int (*func)(menu_item_t *));
int menu_add(menu_item_t *menu, menu_item_t *child);
int menu_startup(menu_cntx_t *ctx);

#endif /*__MENU_H__*/

#if !defined(__MENU_H__)
#define __MENU_H__

#include "mem_pool.h"

#define MENU_TAB_LEN        (8)             /* TAB长度 */
#define MENU_NAME_STR_LEN   (128)
#define MENU_INPUT_LEN      (256)

typedef struct _menu_cntx_t menu_cntx_s;

/* 配置信息 */
typedef struct
{
    int width;                              /* 菜单宽度 */
} menu_conf_t;

/* 菜单项 */
typedef struct _menu_item_t
{
    char name[MENU_NAME_STR_LEN];           /* 菜单名 */

    int (*entry)(menu_cntx_s *ctx, struct _menu_item_t *menu, void *args);    /* 进入之前 */
    int (*func)(menu_cntx_s *ctx, struct _menu_item_t *menu, void *args);     /* 菜单对应的功能 */
    int (*exit)(menu_cntx_s *ctx, struct _menu_item_t *menu, void *args);     /* 退出菜单 */

    int num;                                /* 子菜单数 */
    struct _menu_item_t *child;             /* 子菜单列表 */
    struct _menu_item_t *parent;            /* 父菜单 */
    struct _menu_item_t *next;              /* 兄弟菜单 */

    void *args;                             /* 附加参数 */
} menu_item_t;

#define menu_set_name(menu, nm) snprintf((menu)->name, sizeof((menu)->name), "%s", nm)
#define menu_set_func(menu, _entry, _func, _exit) \
{ \
    (menu)->entry = (_entry); \
    (menu)->func = (_func); \
    (menu)->exit = (_exit); \
}
#define menu_set_args(menu, _args) (menu)->args = (_args)

typedef int (*menu_cb_t)(menu_cntx_s *ctx, menu_item_t *menu, void *args);

/* 全局信息 */
typedef struct _menu_cntx_t
{
    menu_conf_t conf;                       /* 配置 */
    menu_item_t *menu;                      /* 主菜单 */

    mem_pool_t *pool;                       /* 内存池 */
    mem_alloc_cb_t alloc;                   /* 申请内存 */
    mem_dealloc_cb_t dealloc;               /* 释放内存 */
} menu_cntx_t;

menu_cntx_t *menu_init(const char *title, menu_conf_t *conf);
menu_item_t *menu_creat(menu_cntx_t *ctx, const char *name, menu_cb_t entry, menu_cb_t func, menu_cb_t exit, void *args);
int menu_display(menu_cntx_t *ctx, menu_item_t *menu, void *args);
int menu_add(menu_item_t *menu, menu_item_t *child);
menu_item_t *menu_child(menu_cntx_t *ctx, menu_item_t *parent,
        const char *name, menu_cb_t entry, menu_cb_t func, menu_cb_t exit, void *args);
int menu_run(menu_cntx_t *ctx);

#endif /*__MENU_H__*/

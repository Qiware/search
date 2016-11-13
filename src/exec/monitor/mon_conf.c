#include "xml_tree.h"
#include "mon_conf.h"
#include "mem_pool.h"

#define MON_MENU_WIDTH          (58)            /* 菜单宽度 */

/******************************************************************************
 **函数名称: mon_conf_load
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.11 #
 ******************************************************************************/
static int mon_conf_load_menu(xml_tree_t *xml, mon_conf_t *conf)
{
    xml_node_t *node, *nail;

    nail = xml_query(xml, ".MONITOR.MENU");
    if (NULL == nail) {
        return -1;
    }

    node = xml_search(xml, nail, ".WIDTH");
    if (NULL == node) {
        conf->menu.width = MON_MENU_WIDTH;
    }
    else {
        conf->menu.width = atoi(node->value.str);
    }

    return 0;
}

/* 加载IP和端口 */
#define MON_LOAD_CONF(xml,  _path, conf) \
{\
    xml_node_t *node; \
    char node_path[FILE_PATH_MAX_LEN]; \
    \
    snprintf(node_path, sizeof(node_path), "%s.IP", _path); \
    \
    node = xml_query(xml, node_path); \
    if (NULL == node) { \
        break; \
    } \
    \
    snprintf((conf)->ipaddr, sizeof((conf)->ipaddr), "%s", (char *)node->value.str); \
    \
    snprintf(node_path, sizeof(node_path), "%s.PORT", _path); \
    \
    node = xml_query(xml, node_path); \
    if (NULL == node) { \
        break; \
    } \
    \
    (conf)->port = atoi(node->value.str); \
}

/******************************************************************************
 **函数名称: mon_conf_load
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置路径
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.15 #
 ******************************************************************************/
mon_conf_t *mon_conf_load(const char *path)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    mon_conf_t *conf;
    mem_pool_t *pool;

    /* > 创建配置对象 */
    conf = (mon_conf_t *)calloc(1, sizeof(mon_conf_t));
    if (NULL == conf) {
        return NULL;
    }

    /* > 构建XML树 */
    pool = mem_pool_creat(4 * KB);
    if (NULL == pool) {
        free(conf);
        return NULL;
    }

    do {
        /* > 加载XML树 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = pool;
        opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

        xml = xml_creat(path, &opt);
        if (NULL == xml) {
            break;
        }

        /* > 提取配置信息 */
        if (mon_conf_load_menu(xml, conf)) {
            break;
        }

        MON_LOAD_CONF(xml, ".MONITOR.CRAWLER", &conf->crwl);
        MON_LOAD_CONF(xml, ".MONITOR.FILTER", &conf->filter);
        MON_LOAD_CONF(xml, ".MONITOR.SEARCH", &conf->search);

        /* > 释放XML树 */
        xml_destroy(xml);
        mem_pool_destroy(pool);

        return conf;
    } while(0);

    /* > 异常处理 */
    free(conf);
    xml_destroy(xml);
    mem_pool_destroy(pool);

    return NULL;
}

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sys_conf.c
 ** 版本号: 1.0
 ** 描  述: 系统配置
 ** 作  者: # Qifeng.zou # Thu 09 Jul 2015 12:26:13 PM CST #
 ******************************************************************************/
#include "conf.h"
#include "xml_tree.h"

static int conf_load_listen(xml_tree_t *xml, sys_conf_t *conf);
static int conf_load_frwder(xml_tree_t *xml, sys_conf_t *conf);

/******************************************************************************
 **函数名称: conf_load_system
 **功    能: 加载系统配置
 **输入参数:
 **     fpath: 配置路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-07-09 12:29:04 #
 ******************************************************************************/
int conf_load_system(const char *fpath, sys_conf_t *conf)
{
    xml_opt_t opt;
    xml_tree_t *xml;

    memset(&opt, 0, sizeof(opt));

    /* > 加载配置 */
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(fpath, &opt);
    if (NULL == xml)
    {
        fprintf(stderr, "Load configuration failed! path:%s\n", fpath);
        return -1;
    }

    /* > 加载侦听配置 */
    if (conf_load_listen(xml, conf))
    {
        fprintf(stderr, "Load listen configuration failed! path:%s\n", fpath);
        xml_destroy(xml);
        return -1;
    }

    /* > 加载转发配置 */
    if (conf_load_frwder(xml, conf))
    {
        fprintf(stderr, "Load frwder configuration failed! path:%s\n", fpath);
        xml_destroy(xml);
        return -1;
    }

    xml_destroy(xml);
    return 0;
}

/******************************************************************************
 **函数名称: conf_load_listen
 **功    能: 加载侦听配置
 **输入参数:
 **     xml: XML树
 **输出参数:
 **     conf: 日志配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-07-09 12:51:56 #
 ******************************************************************************/
static int conf_load_listen(xml_tree_t *xml, sys_conf_t *conf)
{
    list_opt_t opt;
    conf_map_t map;
    conf_map_t *item;
    xml_node_t *node, *par, *attr;

    /* > 定位侦听配置 */
    par = xml_query(xml, ".SYSTEM.LISTEN");
    if (NULL == par)
    {
        fprintf(stderr, "Query listend configuration failed!\n");
        return -1;
    }

    /* > 创建链表对象 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    conf->listen = list_creat(&opt);
    if (NULL == conf->listen)
    {
        fprintf(stderr, "Create list failed!\n");
        return -1;
    }

    /* > 加载侦听配置 */
    while (NULL != (node = xml_search(xml, par, "ITEM")))
    {
        /* > 结点名 */
        attr = xml_search(xml, node, "NAME");
        if (NULL == attr
            || 0 == attr->value.len)
        {
            fprintf(stderr, "Query listend name failed!\n");
            list_destroy(conf->listen, NULL, mem_dealloc);
            return -1;
        }

        snprintf(map.name, sizeof(map.name), "%s", attr->value.str);

        /* > 对应配置路径 */
        attr = xml_search(xml, node, "PATH");
        if (NULL == attr
            || 0 == attr->value.len)
        {
            fprintf(stderr, "Query listend path failed!\n");
            list_destroy(conf->listen, NULL, mem_dealloc);
            return -1;
        }

        snprintf(map.path, sizeof(map.path), "%s", attr->value.str);

        item = (conf_map_t *)calloc(1, sizeof(conf_map_t));
        if (NULL == item)
        {
            fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
            list_destroy(conf->listen, NULL, mem_dealloc);
            return -1;
        }

        memcpy(item, &map, sizeof(map));

        if (list_rpush(conf->listen, (void *)item))
        {
            fprintf(stderr, "Push into list failed!\n");
            free(item);
            list_destroy(conf->listen, NULL, mem_dealloc);
            return -1;
        }
    }

    return 0;
}

/******************************************************************************
 **函数名称: conf_load_frwder
 **功    能: 加载转发配置
 **输入参数:
 **     xml: XML树
 **输出参数:
 **     conf: 日志配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-07-09 15:28:18 #
 ******************************************************************************/
static int conf_load_frwder(xml_tree_t *xml, sys_conf_t *conf)
{
    list_opt_t opt;
    conf_map_t map;
    conf_map_t *item;
    xml_node_t *node, *par, *attr;

    /* > 定位侦听配置 */
    par = xml_query(xml, ".SYSTEM.FRWDER");
    if (NULL == par)
    {
        fprintf(stderr, "Query frwder configuration failed!\n");
        return -1;
    }

    /* > 创建链表对象 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    conf->frwder = list_creat(&opt);
    if (NULL == conf->frwder)
    {
        fprintf(stderr, "Create list failed!\n");
        return -1;
    }

    /* > 加载侦听配置 */
    while (NULL != (node = xml_search(xml, par, "ITEM")))
    {
        /* > 结点名 */
        attr = xml_search(xml, node, "NAME");
        if (NULL == attr
            || 0 == attr->value.len)
        {
            fprintf(stderr, "Query frwder name failed!\n");
            list_destroy(conf->frwder, NULL, mem_dealloc);
            return -1;
        }

        snprintf(map.name, sizeof(map.name), "%s", attr->value.str);

        /* > 对应配置路径 */
        attr = xml_search(xml, node, "PATH");
        if (NULL == attr
            || 0 == attr->value.len)
        {
            fprintf(stderr, "Query frwder path failed!\n");
            list_destroy(conf->frwder, NULL, mem_dealloc);
            return -1;
        }

        snprintf(map.path, sizeof(map.path), "%s", attr->value.str);

        item = (conf_map_t *)calloc(1, sizeof(conf_map_t));
        if (NULL == item)
        {
            fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
            list_destroy(conf->frwder, NULL, mem_dealloc);
            return -1;
        }

        memcpy(item, &map, sizeof(map));

        if (list_rpush(conf->frwder, (void *)item))
        {
            fprintf(stderr, "Push into list failed!\n");
            free(item);
            list_destroy(conf->frwder, NULL, mem_dealloc);
            return -1;
        }
    }

    return 0;
}

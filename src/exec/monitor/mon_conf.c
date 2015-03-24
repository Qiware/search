#include "xml_tree.h"
#include "mon_conf.h"
#include "mem_pool.h"

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
    xml_tree_t *xml;
    xml_option_t opt;
    mon_conf_t *conf;
    mem_pool_t *pool;
    xml_node_t *node, *nail;

    /* > 创建配置对象 */
    conf = (mon_conf_t *)calloc(1, sizeof(mon_conf_t));
    if (NULL == conf)
    {
        return NULL;
    }

    /* > 构建XML树 */
    pool = mem_pool_creat(4 * KB);
    if (NULL == pool)
    {
        free(conf);
        return NULL;
    }

    do
    {
        /* > 加载XML树 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = pool;
        opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

        xml = xml_creat(path, &opt);
        if (NULL == xml)
        {
            break;
        }

        /* > 提取有效信息 */
        /* 1. 爬虫配置 */
        nail = xml_query(xml, ".MONITOR.CRAWLER");
        if (NULL == nail)
        {
            break;
        }

        node = xml_rquery(xml, nail, "IP");
        if (NULL == node)
        {
            break;
        }

        snprintf(conf->crwl.ip, sizeof(conf->crwl.ip), "%s", (char *)node->value);

        node = xml_rquery(xml, nail, "PORT");
        if (NULL == node)
        {
            break;
        }

        conf->crwl.port = atoi(node->value);

        /* 2. 过滤配置 */
        nail = xml_query(xml, ".MONITOR.FILTER");
        if (NULL == nail)
        {
            break;
        }

        node = xml_rquery(xml, nail, "IP");
        if (NULL == node)
        {
            break;
        }

        snprintf(conf->filter.ip, sizeof(conf->filter.ip), "%s", (char *)node->value);

        node = xml_rquery(xml, nail, "PORT");
        if (NULL == node)
        {
            break;
        }

        conf->filter.port = atoi(node->value);

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

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/

#include "crawler.h"
#include "xml_tree.h"
#include "crwl_comm.h"
#include "thread_pool.h"

static int crwl_parse_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log);
static void *crwl_worker_routine(void *args);

/******************************************************************************
 **函数名称: crwl_load_conf
 **功    能: 加载爬虫配置信息
 **输入参数:
 **     path: 配置路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载爬虫配置
 **     2. 提取配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log)
{
    int ret;
    xml_tree_t *xml;

    /* 1. 加载爬虫配置 */
    xml = xml_creat(path);
    if (NULL == xml)
    {
        log_error(log, "Create xml failed! path:%s", path);
        return CRWL_ERR;
    }

    /* 2. 提取爬虫配置 */
    ret = crwl_parse_conf(xml, conf, log);
    if (0 != ret)
    {
        xml_destroy(xml);
        log_error(log, "Crawler get configuration failed! path:%s", path);
        return CRWL_ERR;
    }

    xml_destroy(xml);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_start_work
 **功    能: 启动爬虫服务
 **输入参数: 
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_start_work(crwl_conf_t *conf, log_cycle_t *log)
{
    int idx;
    thread_pool_t *tpool;

    /* 1. 初始化线程池 */
    tpool = thread_pool_init(conf->thread_num);
    if (NULL == tpool)
    {
        log_error(log, "Initialize thread pool failed!");
        return CRWL_ERR;
    }

    /* 2. 初始化线程池 */
    for (idx=0; idx<conf->thread_num; ++idx)
    {
        thread_pool_add_worker(tpool, crwl_worker_routine, conf);
    }
    
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_parse_conf
 **功    能: 提取配置信息
 **输入参数: 
 **     xml: 配置文件
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static int crwl_parse_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *curr, *node, *node2;

    /* 1. 定位工作进程配置 */
    curr = xml_search(xml, ".SEARCH.CRWLSYS.CRAWLER");
    if (NULL != curr)
    {
        log_error(log, "Didn't configure worker process!");
        return CRWL_ERR;
    }

    /* 2. 爬虫线程数(相对查找) */
    node = xml_rsearch(xml, curr, "THD_NUM");
    if (NULL != node)
    {
        log_warn(log, "Didn't configure the number of worker process!");
        conf->thread_num = CRWL_DEF_THD_NUM;
    }
    else
    {
        conf->thread_num = atoi(node->value);
    }

    /* 3. 任务分配服务IP(相对查找) */
    node = xml_rsearch(xml, curr, "SVRIP");
    if (NULL != node)
    {
        log_error(log, "Didn't configure distribute server ip address!");
        return CRWL_ERR;
    }

    snprintf(conf->svrip, sizeof(conf->svrip), "%s", node->value);

    /* 4. 任务分配服务端口(相对查找) */
    node = xml_rsearch(xml, curr, "PORT");
    if (NULL != node)
    {
        log_error(log, "Didn't configure distribute server port!");
        return CRWL_ERR;
    }

    conf->port = atoi(node->value);

    /* 5. 日志级别:如果本级没有设置，则继承上一级的配置 */
    node2 = curr;
    while (NULL != node2)
    {
        node = xml_rsearch(xml, node2, ".LOG.LEVEL");
        if (NULL != node)
        {
            snprintf(conf->log_level_str,
                sizeof(conf->log_level_str), "%s", node->value);
            break;
        }

        node2 = node2->parent;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_routine
 **功    能: 启动爬虫线程
 **输入参数: 
 **     xml: 配置文件
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static void *crwl_worker_routine(void *args)
{
    return (void *)-1;
}

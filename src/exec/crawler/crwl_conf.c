/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_conf.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责爬虫配置信息的解析加载
 ** 作  者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/

#include "crawler.h"
#include "crwl_conf.h"

/******************************************************************************
 **函数名称: crwl_parse_comm_conf
 **功    能: 加载通用配置
 **输入参数: 
 **     xml: XML配置
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     通过XML查询接口查找对应的配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.16 #
 ******************************************************************************/
static int crwl_parse_comm_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* 1. 定位LOG标签
     *  获取日志级别信息
     * */
    fix = xml_search(xml, ".CRAWLER.COMMON.LOG");
    if (NULL != fix)
    {
        /* 1.1 日志级别 */
        node = xml_rsearch(xml, fix, "LOG_LEVEL");
        if (NULL != node)
        {
            conf->log_level = log_get_level(node->value);
        }

        /* 1.2 系统日志级别 */
        node = xml_rsearch(xml, fix, "LOG2_LEVEL");
        if (NULL != node)
        {
            conf->log2_level = log_get_level(node->value);
        }
    }
    else
    {
        log_warn(log, "Didn't configure log!");
    }

    /* 2. 定位Download标签
     *  获取网页抓取深度和存储路径
     * */
    fix = xml_search(xml, ".CRAWLER.COMMON.DOWNLOAD");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure download!");
        return CRWL_ERR;
    }

    /* 2.1 获取抓取深度 */
    node = xml_rsearch(xml, fix, "DEEP");
    if (NULL == node)
    {
        log_error(log, "Get download deep failed!");
        return CRWL_ERR;
    }

    conf->download.deep = atoi(node->value);

    /* 2.2 获取存储路径 */
    node = xml_rsearch(xml, fix, "PATH");
    if (NULL == node)
    {
        log_error(log, "Get download path failed!");
        return CRWL_ERR;
    }

    snprintf(conf->download.path, sizeof(conf->download.path), "%s", node->value);

    /* 3. 定位REDIS标签
     *  获取Redis的IP地址、端口号、队列等信息
     * */
    fix = xml_search(xml, ".CRAWLER.COMMON.REDIS");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure redis!");
        return CRWL_ERR;
    }

    /* 3.1 获取IP地址 */
    node = xml_rsearch(xml, fix, "IPADDR");
    if (NULL == node)
    {
        log_error(log, "Get redis ip address failed!");
        return CRWL_ERR;
    }

    snprintf(conf->redis.ipaddr, sizeof(conf->redis.ipaddr), "%s", node->value);

    /* 3.2 获取端口号 */
    node = xml_rsearch(xml, fix, "PORT");
    if (NULL == node)
    {
        log_error(log, "Get redis port failed!");
        return CRWL_ERR;
    }

    conf->redis.port = atoi(node->value);

    /* 3.3 获取队列名 */
    node = xml_rsearch(xml, fix, "QUEUE.UNDO_TASKQ");
    if (NULL == node)
    {
        log_error(log, "Get undo task queue failed!");
        return CRWL_ERR;
    }

    snprintf(conf->redis.undo_taskq,
            sizeof(conf->redis.undo_taskq), "%s", node->value);

    /* 3.4 获取哈希表名 */
    node = xml_rsearch(xml, fix, "HASH.DONE_TAB");
    if (NULL == node)
    {
        log_error(log, "Get done hash table failed!");
        return CRWL_ERR;
    }

    snprintf(conf->redis.done_tab,
            sizeof(conf->redis.done_tab), "%s", node->value);

    node = xml_rsearch(xml, fix, "HASH.PUSH_TAB");
    if (NULL == node)
    {
        log_error(log, "Get pushed hash table failed!");
        return CRWL_ERR;
    }

    snprintf(conf->redis.push_tab,
            sizeof(conf->redis.push_tab), "%s", node->value);

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_parse_seed_conf
 **功    能: 加载种子配置
 **输入参数: 
 **     xml: XML配置
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     通过XML查询接口查找对应的配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
static int crwl_parse_seed_conf(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
    list_node_t *node;
    crwl_seed_item_t *seed;
    xml_node_t *cf_node, *cf_item;

    /* 1. 定位SEED->ITEM标签 */
    cf_item = xml_search(xml, ".CRAWLER.SEED.ITEM");
    if (NULL == cf_item)
    {
        log_error(log, "Didn't configure seed item!");
        return CRWL_ERR;
    }

    /* 2. 提取种子信息 */
    while (NULL != cf_item)
    {
        if (0 != strcasecmp(cf_item->name, "ITEM"))
        {
            cf_item = cf_item->next;
            continue;
        }

        /* 申请配置空间 */
        node = (list_node_t *)calloc(1, sizeof(list_node_t));
        if (NULL == node)
        {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            return CRWL_ERR;
        }

        seed = (crwl_seed_item_t *)calloc(1, sizeof(crwl_seed_item_t));
        if (NULL == seed)
        {
            free(node);
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            return CRWL_ERR;
        }

        node->data = seed;

        /* 提取URI */
        cf_node = xml_rsearch(xml, cf_item, "URI");
        if (NULL == cf_node)
        {
            free(seed);
            free(node);
            log_error(log, "Get uri failed!");
            return CRWL_ERR;
        }

        snprintf(seed->uri, sizeof(seed->uri), "%s", cf_node->value);

        /* 获取DEEP */
        cf_node = xml_rsearch(xml, cf_item, "DEEP");
        if (NULL == cf_node)
        {
            seed->deep = 0;
            log_info(log, "Didn't set deepth of uri!");
        }
        else
        {
            seed->deep = atoi(cf_node->value);
        }

        /* 加入配置链表 */
        list_insert_tail(&conf->seed, node);

        cf_item = cf_item->next;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_parse_conf
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
static int crwl_worker_parse_conf(
        xml_tree_t *xml, crwl_worker_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *curr, *node;

    /* 1. 定位工作进程配置 */
    curr = xml_search(xml, ".CRAWLER.WORKER");
    if (NULL == curr)
    {
        log_error(log, "Didn't configure worker process!");
        return CRWL_ERR;
    }

    /* 2. 爬虫线程数(相对查找) */
    node = xml_rsearch(xml, curr, "NUM");
    if (NULL == node)
    {
        conf->num = CRWL_THD_DEF_NUM;
        log_warn(log, "Set thread number: %d!", conf->num);
    }
    else
    {
        conf->num = atoi(node->value);
    }

    if (conf->num <= 0)
    {
        conf->num = CRWL_THD_MIN_NUM;
        log_warn(log, "Set thread number: %d!", conf->num);
    }

    /* 3. 并发网页连接数(相对查找) */
    node = xml_rsearch(xml, curr, "CONNECTIONS.MAX");
    if (NULL == node)
    {
        log_error(log, "Didn't configure download webpage number!");
        return CRWL_ERR;
    }

    conf->connections = atoi(node->value);
    if (conf->connections <= 0)
    {
        conf->connections = CRWL_CONNECTIONS_MIN_NUM;
    }
    else if (conf->connections >= CRWL_CONNECTIONS_MAX_NUM)
    {
        conf->connections = CRWL_CONNECTIONS_MAX_NUM;
    }

    /* 4. Undo任务队列配置(相对查找) */
    node = xml_rsearch(xml, curr, "TASKQ.COUNT");
    if (NULL == node)
    {
        log_error(log, "Didn't configure count of undo task queue unit!");
        return CRWL_ERR;
    }

    conf->taskq_count = atoi(node->value);
    if (conf->taskq_count <= 0)
    {
        conf->taskq_count = CRWL_TASK_QUEUE_MAX_NUM;
    }

    return CRWL_OK;
}



/******************************************************************************
 **函数名称: crwl_load_conf
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
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

    /* 2. 加载通用配置 */
    ret = crwl_parse_comm_conf(xml, conf, log);
    if (CRWL_OK != ret)
    {
        log_error(log, "Load common conf failed! path:%s", path);
        return CRWL_ERR;
    }

    /* 3. 提取爬虫配置 */
    ret = crwl_worker_parse_conf(xml, &conf->worker, log);
    if (0 != ret)
    {
        log_error(log, "Parse worker configuration failed! path:%s", path);
        xml_destroy(xml);
        return CRWL_ERR;
    }

    /* 4. 加载种子配置 */
    ret = crwl_parse_seed_conf(xml, conf, log);
    if (CRWL_OK != ret)
    {
        log_error(log, "Load seed conf failed! path:%s", path);
        return CRWL_ERR;
    }

    xml_destroy(xml);
    return CRWL_OK;
}



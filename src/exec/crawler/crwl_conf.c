/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: crwl_conf.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫配置
 **         负责爬虫配置信息(crawler.xml)的解析加载
 ** 作  者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#include "syscall.h"
#include "crwl_conf.h"

#define CRWL_CONN_MAX_NUM           (1024)  /* 最大网络连接数 */
#define CRWL_CONN_DEF_NUM           (128)   /* 默认网络连接数 */
#define CRWL_CONN_MIN_NUM           (1)     /* 最小网络连接数 */
#define CRWL_CONN_TMOUT_SEC         (15)    /* 连接超时时间(秒) */
#define CRWL_WORKQ_MAX_NUM          (2048)  /* 工作队列单元数 */

static int _crwl_conf_load(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log);
static int crwl_conf_load_redis(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log);

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
int crwl_load_conf(const char *path, crwl_conf_t *conf, log_cycle_t *log)
{
    xml_opt_t opt;
    xml_tree_t *xml;

    /* > 构建XML树 */
    memset(&opt, 0, sizeof(opt));

    opt.log = log;
    opt.pool = NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml) {
        log_error(log, "Create xml failed! path:%s", path);
        return -1;
    }

    /* > 提取配置信息 */
    if (_crwl_conf_load(xml, conf, log)) {
        log_error(log, "Load conf failed! path:%s", path);
        xml_destroy(xml);
        return -1;
    }

    /* > 释放XML树 */
    xml_destroy(xml);

    return 0;
}

/******************************************************************************
 **函数名称: _crwl_conf_load
 **功    能: 加载通用配置
 **输入参数: 
 **     xml: XML配置
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 通过XML查询接口查找对应的配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.16 #
 ******************************************************************************/
static int _crwl_conf_load(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
#define CRWL_THD_DEF_NUM            (05)    /* 默认线程数 */
#define CRWL_THD_MIN_NUM            (01)    /* 最小线程数 */

    xml_node_t *node, *nail;
    crwl_worker_conf_t *worker = &conf->worker;

    conf->sched_stat = true;

    /* > 定位WORKER配置 */
    nail = xml_query(xml, ".CRAWLER.WORKER");
    if (NULL == nail) {
        log_error(log, "Didn't configure worker process!");
        return -1;
    }

    /* 1. 爬虫线程数(相对查找) */
    node = xml_search(xml, nail, "NUM");
    if (NULL == node) {
        worker->num = CRWL_THD_DEF_NUM;
        log_warn(log, "Set thread number: %d!", worker->num);
    }
    else
    {
        worker->num = atoi(node->value.str);
    }

    if (worker->num <= 0) {
        worker->num = CRWL_THD_MIN_NUM;
        log_warn(log, "Set thread number: %d!", worker->num);
    }

    /* 2. 并发网页连接数(相对查找) */
    node = xml_search(xml, nail, "CONNECTIONS.MAX");
    if (NULL == node) {
        log_error(log, "Didn't configure download webpage number!");
        return -1;
    }

    worker->conn_max_num = atoi(node->value.str);
    if (worker->conn_max_num <= 0) {
        worker->conn_max_num = CRWL_CONN_MIN_NUM;
    }
    else if (worker->conn_max_num >= CRWL_CONN_MAX_NUM) {
        worker->conn_max_num = CRWL_CONN_MAX_NUM;
    }

    /* 3. 连接超时时间 */
    node = xml_search(xml, nail, "CONNECTIONS.TIMEOUT");
    if (NULL == node) {
        log_error(log, "Didn't configure download webpage number!");
        return -1;
    }

    worker->conn_tmout_sec = atoi(node->value.str);
    if (worker->conn_tmout_sec <= 0) {
        worker->conn_tmout_sec = CRWL_CONN_TMOUT_SEC;
    }

    /* > 定位Download标签
     *  获取网页抓取深度和存储路径 */
    nail = xml_query(xml, ".CRAWLER.DOWNLOAD");
    if (NULL == nail) {
        log_error(log, "Didn't configure download!");
        return -1;
    }

    /* 1 获取抓取深度 */
    node = xml_search(xml, nail, "DEPTH");
    if (NULL == node) {
        log_error(log, "Get download depth failed!");
        return -1;
    }

    conf->download.depth = atoi(node->value.str);

    /* 2 获取存储路径 */
    node = xml_search(xml, nail, "PATH");
    if (NULL == node) {
        log_error(log, "Get download path failed!");
        return -1;
    }

    snprintf(conf->download.path, sizeof(conf->download.path), "%s", node->value.str);

    /* 3 任务队列配置(相对查找) */
    node = xml_query(xml, "CRAWLER.WORKQ.COUNT");
    if (NULL == node) {
        log_error(log, "Didn't configure count of work task queue unit!");
        return -1;
    }

    conf->workq_count = atoi(node->value.str);
    if (conf->workq_count <= 0) {
        conf->workq_count = CRWL_WORKQ_MAX_NUM;
    }

    /* > 获取管理配置 */
    node = xml_query(xml, "CRAWLER.MANAGER.PORT");
    if (NULL == node) {
        log_error(log, "Get manager port failed!");
        return -1;
    }

    conf->man_port = atoi(node->value.str);

    /* > 获取Redis配置 */
    if (crwl_conf_load_redis(xml, conf, log)) {
        log_error(log, "Get redis configuration failed!");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: crwl_conf_load_redis
 **功    能: 加载Redis配置
 **输入参数: 
 **     xml: XML配置
 **     log: 日志对象
 **输出参数:
 **     conf: Redis配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     通过XML查询接口查找对应的配置信息
 **注意事项: 1. 因为链表中的结点的空间是从mem_pool_t内存池中分配的, 因此, 如果
 **             处理过程中出现失败的情况时, 申请的内存空间不必进行释放的操作!
 **作    者: # Qifeng.zou # 2014.10.29 #
 ******************************************************************************/
static int crwl_conf_load_redis(xml_tree_t *xml, crwl_conf_t *conf, log_cycle_t *log)
{
    int idx;
    xml_node_t *nail, *node, *start, *item;
    crwl_redis_conf_t *redis = &conf->redis;

    /* > 定位REDIS标签
     *  获取Redis的IP地址、端口号、队列、副本等信息 */
    nail = xml_query(xml, ".CRAWLER.REDIS");
    if (NULL == nail) {
        log_error(log, "Didn't configure redis!");
        return -1;
    }

    /* > 计算REDIS网络配置项总数 */
    start = xml_search(xml, nail, "NETWORK.ITEM");
    if (NULL == start) {
        log_error(log, "Query item of network failed!");
        return -1;
    }
    
    redis->num = 0;
    item = start;
    while (NULL != item) {
        if (strcmp(item->name.str, "ITEM")) {
            log_error(log, "Mark name isn't right! mark:%s", item->name.str);
            return -1;
        }
        ++redis->num;
        item = item->next;
    }

    redis->conf = (redis_conf_t *)calloc(redis->num, sizeof(redis_conf_t));
    if (NULL == redis->conf) {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* 注: 出现异常情况时 内存在此不必释放 */
    idx = 0;
    item = start;
    while (NULL != item) {
        /* 获取IP地址 */
        node = xml_search(xml, item, "IP");
        if (NULL == node) {
            log_error(log, "Mark name isn't right! mark:%s", item->name);
            return -1;
        }

        snprintf(redis->conf[idx].ip, sizeof(redis->conf[idx].ip), "%s", node->value.str);

        /* 获取PORT地址 */
        node = xml_search(xml, item, "PORT");
        if (NULL == node) {
            log_error(log, "Mark name isn't right! mark:%s", item->name);
            return -1;
        }

        redis->conf[idx].port = atoi(node->value.str);

        /* 下一结点 */
        item = item->next;
        ++idx;
    }

    /* > 获取队列名 */
    node = xml_search(xml, nail, "TASKQ.NAME");
    if (NULL == node) {
        log_error(log, "Get undo task queue failed!");
        return -1;
    }

    snprintf(redis->taskq, sizeof(redis->taskq), "%s", node->value.str);

    /* > 获取哈希表名 */
    node = xml_search(xml, nail, "DONE_TAB.NAME");  /* DONE哈希表 */
    if (NULL == node) {
        log_error(log, "Get done hash table failed!");
        return -1;
    }

    snprintf(redis->done_tab, sizeof(redis->done_tab), "%s", node->value.str);

    node = xml_search(xml, nail, "PUSH_TAB.NAME");  /* PUSH哈希表 */
    if (NULL == node) {
        log_error(log, "Get pushed hash table failed!");
        return -1;
    }

    snprintf(redis->push_tab, sizeof(redis->push_tab), "%s", node->value.str);

    return 0;
}

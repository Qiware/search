/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: flt_conf.c
 ** 版本号: 1.0
 ** 描  述: 过滤器配置
 **         负责过滤器配置(filter.xml)的解析加载
 ** 作  者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/

#include "redo.h"
#include "flt_conf.h"

static int _flt_conf_load(xml_tree_t *xml, flt_conf_t *conf, log_cycle_t *log);
static int flt_conf_load_redis(xml_tree_t *xml, flt_conf_t *conf, log_cycle_t *log);
static int flt_conf_load_seed(xml_tree_t *xml, flt_conf_t *conf, log_cycle_t *log);

/******************************************************************************
 **函数名称: flt_conf_load
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置路径
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 配置对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
flt_conf_t *flt_conf_load(const char *path, log_cycle_t *log)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    flt_conf_t *conf;
    mem_pool_t *pool;

    /* > 创建配置对象 */
    conf = (flt_conf_t *)calloc(1, sizeof(flt_conf_t));
    if (NULL == conf) {
        log_error(log, "Alloc memory from pool failed!");
        return NULL;
    }

    /* > 构建XML树 */
    pool = mem_pool_creat(4 * KB);
    if (NULL == pool) {
        log_error(log, "Create memory pool failed!");
        free(conf);
        return NULL;
    }

    memset(&opt, 0, sizeof(opt));

    opt.log = log;
    opt.pool = pool;
    opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml) {
        log_error(log, "Create xml failed! path:%s", path);
        free(conf);
        mem_pool_destroy(pool);
        return NULL;
    }

    /* > 加载通用配置 */
    if (_flt_conf_load(xml, conf, log)) {
        log_error(log, "Load common conf failed! path:%s", path);
        free(conf);
        mem_pool_destroy(pool);
        return NULL;
    }

    /* 释放XML树 */
    xml_destroy(xml);
    mem_pool_destroy(pool);

    return conf;
}

/******************************************************************************
 **函数名称: _flt_conf_load
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
static int _flt_conf_load(xml_tree_t *xml, flt_conf_t *conf, log_cycle_t *log)
{
#define FLT_THD_DEF_NUM             (5)     /* 默认线程数 */
#define FLT_WORKQ_MAX_NUM           (2048)  /* 工作队列单元数 */
    xml_node_t *node, *nail;
    flt_work_conf_t *work = &conf->work;

    /* > 定位工作进程配置 */
    nail = xml_query(xml, ".FILTER.WORKER");
    if (NULL == nail) {
        log_error(log, "Didn't configure worker process!");
        return -1;
    }

    /* 1. 爬虫线程数(相对查找) */
    node = xml_search(xml, nail, "NUM");
    if (NULL == node) {
        work->num = FLT_THD_DEF_NUM;
        log_warn(log, "Set thread number: %d!", work->num);
    }
    else {
        work->num = atoi(node->value.str);
    }

    /* 2. 工作路径(相对查找) */
    node = xml_search(xml, nail, "PATH");
    if (NULL == node) {
        log_error(log, "Didn't configure workspace path!");
        return -1;
    }

    snprintf(work->path, sizeof(work->path), "%s", node->value.str);

    Mkdir(work->path, DIR_MODE);

    snprintf(work->err_path, sizeof(work->err_path), "%s/error", node->value.str);
    Mkdir(work->err_path, DIR_MODE);
    snprintf(work->man_path, sizeof(work->man_path), "%s/manage", node->value.str);
    Mkdir(work->man_path, DIR_MODE);
    snprintf(work->webpage_path, sizeof(work->webpage_path), "%s/webpage", node->value.str);
    Mkdir(work->webpage_path, DIR_MODE);

    /* > 定位Download标签
     *  获取网页抓取深度和存储路径
     * */
    nail = xml_query(xml, ".FILTER.DOWNLOAD");
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
    node = xml_query(xml, "FILTER.WORKQ.COUNT");
    if (NULL == node) {
        log_error(log, "Didn't configure count of work task queue unit!");
        return -1;
    }

    conf->workq_count = atoi(node->value.str);
    if (conf->workq_count <= 0) {
        conf->workq_count = FLT_WORKQ_MAX_NUM;
    }

    /* > 获取管理配置 */
    node = xml_query(xml, "FILTER.MANAGER.PORT");
    if (NULL == node) {
        log_error(log, "Get manager port failed!");
        return -1;
    }

    conf->man_port = atoi(node->value.str);

    /* > 获取Redis配置 */
    if (flt_conf_load_redis(xml, conf, log)) {
        log_error(log, "Get redis configuration failed!");
        return -1;
    }

    /* > 加载种子配置 */
    if (flt_conf_load_seed(xml, conf, log)) {
        log_error(log, "Load seed conf failed!");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: flt_conf_load_redis
 **功    能: 加载Redis配置
 **输入参数: 
 **     xml: XML配置
 **     log: 日志对象
 **输出参数:
 **     conf: Redis配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     通过XML查询接口查找对应的配置信息
 **注意事项: 
 **     1. 因为链表中的结点的空间是从mem_pool_t内存池中分配的, 因此, 如果处理过
 **     程中出现失败的情况时, 申请的内存空间不必进行释放的操作!
 **作    者: # Qifeng.zou # 2014.10.29 #
 ******************************************************************************/
static int flt_conf_load_redis(xml_tree_t *xml, flt_conf_t *conf, log_cycle_t *log)
{
    int idx;
    xml_node_t *nail, *node, *start, *item;
    flt_redis_conf_t *redis = &conf->redis;

    /* > 定位REDIS标签
     *  获取Redis的IP地址、端口号、队列、副本等信息 */
    nail = xml_query(xml, ".FILTER.REDIS");
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
            log_error(log, "Mark name isn't right! mark:%s", item->name);
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

    do {
        /* 注: 出现异常情况时 内存在此不必释放 */
        idx = 0;
        item = start;
        while (NULL != item) {
            /* 获取IP地址 */
            node = xml_search(xml, item, "IP");
            if (NULL == node) {
                log_error(log, "Mark name isn't right! mark:%s", item->name);
                break;
            }

            snprintf(redis->conf[idx].ip, sizeof(redis->conf[idx].ip), "%s", node->value.str);

            /* 获取PORT地址 */
            node = xml_search(xml, item, "PORT");
            if (NULL == node) {
                log_error(log, "Mark name isn't right! mark:%s", item->name);
                break;
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
            break;
        }

        snprintf(redis->taskq, sizeof(redis->taskq), "%s", node->value.str);

        /* > 获取哈希表名 */
        node = xml_search(xml, nail, "DONE_TAB.NAME");  /* DONE哈希表 */
        if (NULL == node) {
            log_error(log, "Get done hash table failed!");
            break;
        }

        snprintf(redis->done_tab, sizeof(redis->done_tab), "%s", node->value.str);

        node = xml_search(xml, nail, "PUSH_TAB.NAME");  /* PUSH哈希表 */
        if (NULL == node) {
            log_error(log, "Get pushed hash table failed!");
            break;
        }

        snprintf(redis->push_tab, sizeof(redis->push_tab), "%s", node->value.str);

        return 0;
    } while(0);

    free(redis->conf);

    return -1;
}


/******************************************************************************
 **函数名称: flt_conf_load_seed
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
static int flt_conf_load_seed(xml_tree_t *xml, flt_conf_t *conf, log_cycle_t *log)
{
    flt_seed_conf_t *seed;
    xml_node_t *node, *item;

    /* 1. 定位SEED->ITEM标签 */
    item = xml_query(xml, ".FILTER.SEED.ITEM");
    if (NULL == item) {
        log_error(log, "Didn't configure seed item!");
        return -1;
    }

    /* 2. 提取种子信息 */
    conf->seed_num = 0;
    while (NULL != item) {
        if (0 != strcasecmp(item->name.str, "ITEM")) {
            item = item->next;
            continue;
        }

        if (conf->seed_num >= FLT_SEED_MAX_NUM) {
            log_error(log, "Seed number is too many!");
            return -1;
        }

        seed = &conf->seed[conf->seed_num++];

        /* 提取URI */
        node = xml_search(xml, item, "URI");
        if (NULL == node) {
            log_error(log, "Get uri failed!");
            return -1;
        }

        snprintf(seed->uri, sizeof(seed->uri), "%s", node->value.str);

        /* 获取DEPTH */
        node = xml_search(xml, item, "DEPTH");
        if (NULL == node) {
            seed->depth = 0;
            log_info(log, "Didn't set depth of uri!");
        }
        else {
            seed->depth = atoi(node->value.str);
        }

        item = item->next;
    }

    return 0;
}

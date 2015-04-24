/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: srch_conf.c
 ** 版本号: 1.0
 ** 描  述: 搜索引擎配置
 **         负责从搜索引擎配置文件(search.xml)中提取有效信息
 ** 作  者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#include "search.h"
#include "syscall.h"
#include "srch_conf.h"

static int srch_conf_parse(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log);

/******************************************************************************
 **函数名称: srch_conf_load
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置路径
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 配置对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
srch_conf_t *srch_conf_load(const char *path, log_cycle_t *log)
{
    xml_option_t opt;
    srch_conf_t *conf;
    mem_pool_t *mem_pool;
    xml_tree_t *xml = NULL;

    /* 1. 创建配置内存池 */
    mem_pool = mem_pool_creat(4 * KB);
    if (NULL == mem_pool)
    {
        log_error(log, "Create memory pool failed!");
        return NULL;
    }

    do
    {
        /* 2. 创建配置对象 */
        conf = (srch_conf_t *)calloc(1, sizeof(srch_conf_t));
        if (NULL == conf)
        {
            log_error(log, "Alloc memory from pool failed!");
            break;
        }

       /* 2. 构建XML树 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = mem_pool;
        opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

        xml = xml_creat(path, &opt);
        if (NULL == xml)
        {
            log_error(log, "Create xml failed! path:%s", path);
            break;
        }

        /* 3. 加载通用配置 */
        if (srch_conf_parse(xml, conf, log))
        {
            log_error(log, "Load common conf failed! path:%s", path);
            break;
        }

        /* 4. 释放XML树 */
        xml_destroy(xml);
        return conf;
    } while(0);

    /* 异常处理 */
    if (NULL != xml)
    {
        xml_destroy(xml);
    }
    mem_pool_destroy(mem_pool);
    return NULL;
}

/* 解析日志级别配置 */
static int srch_conf_parse_log(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* > 定位日志标签 */
    fix = xml_query(xml, ".SEARCH-ENGINE.LOG");
    if (NULL == fix)
    {
        conf->log.level = log_get_level(LOG_DEF_LEVEL_STR);
        conf->log.syslevel = log_get_level(LOG_DEF_LEVEL_STR);
        return SRCH_OK;
    }

    /* > 日志级别 */
    node = xml_rquery(xml, fix, "LEVEL");
    if (NULL != node)
    {
        conf->log.level = log_get_level(node->value.str);
    }
    else
    {
        conf->log.level = log_get_level(LOG_DEF_LEVEL_STR);
    }

    /* > 系统日志级别 */
    node = xml_rquery(xml, fix, "SYS_LEVEL");
    if (NULL != node)
    {
        conf->log.syslevel = log_get_level(node->value.str);
    }
    else
    {
        conf->log.syslevel = log_get_level(LOG_DEF_LEVEL_STR);
    }

    return SRCH_OK;
}

/* 解析并发信息配置 */
static int srch_conf_parse_connections(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* > 定位并发配置 */
    fix = xml_query(xml, ".SEARCH-ENGINE.CONNECTIONS");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure connections!");
        return SRCH_ERR;
    }

    /* > 获取最大并发数 */
    node = xml_rquery(xml, fix, "MAX");
    if (NULL == node)
    {
        log_error(log, "Get max number of connections failed!");
        return SRCH_ERR;
    }

    conf->connections.max = atoi(node->value.str);

    /* > 获取连接超时时间 */
    node = xml_rquery(xml, fix, "TIMEOUT");
    if (NULL == node)
    {
        log_error(log, "Get timeout of connection failed!");
        return SRCH_ERR;
    }

    conf->connections.timeout = atoi(node->value.str);

    /* > 获取侦听端口 */
    node = xml_rquery(xml, fix, "PORT");
    if (NULL == node)
    {
        log_error(log, "Get port of connection failed!");
        return SRCH_ERR;
    }

    conf->connections.port = atoi(node->value.str);

    return SRCH_OK;
}

/* 解析队列配置 */
static int srch_conf_parse_queue(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* 加载队列信息 */
#define SRCH_LOAD_QUEUE(xml, fix,  _path, conf) \
    {\
        char node_path[FILE_PATH_MAX_LEN]; \
        \
        snprintf(node_path, sizeof(node_path), "%s.MAX", _path); \
        \
        node = xml_rquery(xml, fix, node_path); \
        if (NULL == node) \
        { \
            return SRCH_ERR; \
        } \
        \
        (conf)->max = atoi(node->value.str); \
        \
        snprintf(node_path, sizeof(node_path), "%s.SIZE", _path); \
        \
        node = xml_rquery(xml, fix, node_path); \
        if (NULL == node) \
        { \
            return SRCH_ERR; \
        } \
        \
        (conf)->size = atoi(node->value.str); \
    }

    /* > 定位队列标签 */
    fix = xml_query(xml, ".SEARCH-ENGINE.QUEUE");
    if (NULL == fix)
    {
        log_error(log, "Get queue configuration failed!");
        return SRCH_ERR;
    }

    /* > 获取队列配置 */
    SRCH_LOAD_QUEUE(xml, fix, ".CONNQ", &conf->connq);
    SRCH_LOAD_QUEUE(xml, fix, ".TASKQ", &conf->taskq);

    return SRCH_OK;
}

/* 解析配置信息 */
static int srch_conf_parse(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node;

    /* > 获取日志级别信息*/
    if (srch_conf_parse_log(xml, conf, log))
    {
        log_error(log, "Parse log configuration failed!");
        return SRCH_ERR;
    }

    /* > 获取连接配置 */
    if (srch_conf_parse_connections(xml, conf, log))
    {
        log_error(log, "Parse connections configuration failed!");
        return SRCH_ERR;
    }

    /* > 获取队列配置 */
    if (srch_conf_parse_queue(xml, conf, log))
    {
        log_error(log, "Parse queue configuration failed!");
        return SRCH_ERR;
    }

    /* 3. 获取WORKER.NUM标签 */
    node = xml_query(xml, ".SEARCH-ENGINE.WORKER.NUM");
    if (NULL == node)
    {
        log_error(log, "Didn't configure worker!");
        return SRCH_ERR;
    }

    conf->worker_num = atoi(node->value.str);

    /* 4. 获取AGENT.NUM标签 */
    node = xml_query(xml, ".SEARCH-ENGINE.AGENT.NUM");
    if (NULL == node)
    {
        log_error(log, "Didn't configure agent!");
        return SRCH_ERR;
    }

    conf->agent_num = atoi(node->value.str);

    return SRCH_OK;
}

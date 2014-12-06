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

static int srch_conf_load_comm(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log);

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
    xml_tree_t *xml;
    srch_conf_t *conf;
    mem_pool_t *mem_pool;

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
        conf = mem_pool_alloc(mem_pool, sizeof(srch_conf_t));
        if (NULL == conf)
        {
            log_error(log, "Alloc memory from pool failed!");
            break;
        }

        conf->mem_pool = mem_pool;

        /* 2. 构建XML树 */
        xml = xml_creat(path);
        if (NULL == xml)
        {
            log_error(log, "Create xml failed! path:%s", path);
            break;
        }

        /* 3. 加载通用配置 */
        if (srch_conf_load_comm(xml, conf, log))
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

/******************************************************************************
 **函数名称: srch_conf_load_comm
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
 **作    者: # Qifeng.zou # 2014.11.16 #
 ******************************************************************************/
static int srch_conf_load_comm(xml_tree_t *xml, srch_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* 1. 定位LOG标签
     *  获取日志级别信息
     * */
    fix = xml_query(xml, ".SEARCH-ENGINE.LOG");
    if (NULL != fix)
    {
        /* 1.1 日志级别 */
        node = xml_rquery(xml, fix, "LEVEL");
        if (NULL != node)
        {
            conf->log.level = log_get_level(node->value);
        }
        else
        {
            conf->log.level = log_get_level(LOG_DEF_LEVEL_STR);
        }

        /* 1.2 系统日志级别 */
        node = xml_rquery(xml, fix, "LEVEL2");
        if (NULL != node)
        {
            conf->log.level2 = log_get_level(node->value);
        }
        else
        {
            conf->log.level2 = log_get_level(LOG_DEF_LEVEL_STR);
        }
    }
    else
    {
        conf->log.level = log_get_level(LOG_DEF_LEVEL_STR);
        conf->log.level2 = log_get_level(LOG_DEF_LEVEL_STR);

        log_warn(log, "Didn't configure log!");
    }

    /* 2. 定位Connections标签
     *  获取网页抓取深度和存储路径
     * */
    fix = xml_query(xml, ".SEARCH-ENGINE.CONNECTIONS");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure connections!");
        return SRCH_ERR;
    }

    /* 2.1 获取最大并发数 */
    node = xml_rquery(xml, fix, "MAX");
    if (NULL == node)
    {
        log_error(log, "Get max number of connections failed!");
        return SRCH_ERR;
    }

    conf->connections.max = atoi(node->value);

    /* 2.2 获取连接超时时间 */
    node = xml_rquery(xml, fix, "TIMEOUT");
    if (NULL == node)
    {
        log_error(log, "Get timeout of connection failed!");
        return SRCH_ERR;
    }

    conf->connections.timeout = atoi(node->value);

    /* 3. 获取WORKER.NUM标签 */
    node = xml_query(xml, ".SEARCH-ENGINE.WORKER.NUM");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure worker!");
        return SRCH_ERR;
    }

    conf->worker_num = atoi(node->value);

    /* 4. 获取AGENT.NUM标签 */
    node = xml_query(xml, ".SEARCH-ENGINE.AGENT.NUM");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure receiver!");
        return SRCH_ERR;
    }

    conf->agent_num = atoi(node->value);

    return SRCH_OK;
}

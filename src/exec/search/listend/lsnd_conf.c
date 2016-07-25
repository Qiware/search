/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: lsnd_conf.c
 ** 版本号: 1.0
 ** 描  述: 代理服务配置
 **         负责从代理服务配置文件(lsnd.xml)中提取有效信息
 ** 作  者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#include "xml_tree.h" 
#include "lsnd_conf.h"

static int lsnd_conf_parse(xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log);

static int lsnd_conf_load_comm(xml_tree_t *xml, lsnd_conf_t *conf, log_cycle_t *log);
static int lsnd_conf_load_agent(xml_tree_t *xml, lsnd_conf_t *conf, log_cycle_t *log);
static int lsnd_conf_load_frwder(xml_tree_t *xml, lsnd_conf_t *lcf, log_cycle_t *log);

/******************************************************************************
 **函数名称: lsnd_load_conf
 **功    能: 加载配置信息
 **输入参数: 
 **     path: 配置文件路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 载入配置文件, 并提取其中的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-25 22:43:12 #
 ******************************************************************************/
int lsnd_load_conf(const char *path, lsnd_conf_t *conf, log_cycle_t *log)
{
    xml_opt_t opt;
    xml_tree_t *xml = NULL;

    memset(conf, 0, sizeof(lsnd_conf_t));

    do {
        /* > 构建XML树 */
        memset(&opt, 0, sizeof(opt));

        opt.log = log;
        opt.pool = (void *)NULL;
        opt.alloc = (mem_alloc_cb_t)mem_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

        xml = xml_creat(path, &opt);
        if (NULL == xml) {
            log_error(log, "Create xml failed! path:%s", path);
            break;
        }

        /* > 加载通用配置 */
        if (lsnd_conf_load_comm(xml, conf, log)) {
            log_error(log, "Load common configuration failed!");
            break;
        }

        /* > 加载AGENT配置 */
        if (lsnd_conf_load_agent(xml, conf, log)) {
            log_error(log, "Load AGENT conf failed! path:%s", path);
            break;
        }

        /* > 加载转发配置 */
        if (lsnd_conf_load_frwder(xml, conf, log)) {
            log_error(log, "Load rttp conf failed! path:%s", path);
            break;
        }

        /* > 释放XML树 */
        xml_destroy(xml);
        return 0;
    } while(0);

    /* 异常处理 */
    if (NULL != xml) { xml_destroy(xml); }
    return -1;
}

/******************************************************************************
 **函数名称: lsnd_conf_load_comm
 **功    能: 加载公共配置
 **输入参数: 
 **     path: 配置文件路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 提取配置文件中的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-25 22:43:12 #
 ******************************************************************************/
static int lsnd_conf_load_comm(xml_tree_t *xml, lsnd_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* > 加载结点ID */
    node = xml_query(xml, ".LISTEND.ID");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Get node id failed!");
        return -1;
    }

    conf->nid = atoi(node->value.str);

    /* > 加载工作路径 */
    node = xml_query(xml, ".LISTEND.WORKDIR");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Get work directory failed!");
        return -1;
    }

    snprintf(conf->wdir, sizeof(conf->wdir), "%s/%d", node->value.str, conf->nid);  /* 工作路径 */

    /* > 分发队列配置 */
    fix = xml_query(xml, ".LISTEND.DISTQ");
    if (NULL == fix) {
        log_error(log, "Get distribute queue failed!");
        return -1;
    }

    node = xml_search(xml, fix, "NUM");
    if (NULL == node) {
        log_error(log, "Get number of distribue queue failed!");
        return -1;
    }

    conf->distq.num = atoi(node->value.str);

    node = xml_search(xml, fix, "MAX");
    if (NULL == node) {
        log_error(log, "Get the max container of distribue queue failed!");
        return -1;
    }

    conf->distq.max = atoi(node->value.str);

    node = xml_search(xml, fix, "SIZE");
    if (NULL == node) {
        log_error(log, "Get the size of distribue queue failed!");
        return -1;
    }

    conf->distq.size = atoi(node->value.str);

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_conf_parse_agent_connections
 **功    能: 解析代理并发配置
 **输入参数: 
 **     path: 配置文件路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 提取配置文件中的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-25 22:43:12 #
 ******************************************************************************/
static int lsnd_conf_parse_agent_connections(
        xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *fix, *node;

    /* > 定位并发配置 */
    fix = xml_query(xml, ".LISTEND.AGENT.CONNECTIONS");
    if (NULL == fix) {
        log_error(log, "Didn't configure connections!");
        return -1;
    }

    node = xml_search(xml, fix, "MAX");         /* > 获取最大并发数 */
    if (NULL == node) {
        log_error(log, "Get max number of connections failed!");
        return -1;
    }

    conf->connections.max = atoi(node->value.str);

    node = xml_search(xml, fix, "TIMEOUT");     /* > 获取连接超时时间 */
    if (NULL == node) {
        log_error(log, "Get timeout of connection failed!");
        return -1;
    }

    conf->connections.timeout = atoi(node->value.str);

    /* > 获取侦听端口 */
    node = xml_search(xml, fix, "PORT");
    if (NULL == node) {
        log_error(log, "Get port of connection failed!");
        return -1;
    }

    conf->connections.port = atoi(node->value.str);

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_conf_parse_agent_queue
 **功    能: 解析代理各队列配置
 **输入参数: 
 **     path: 配置文件路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 提取配置文件中的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-25 22:43:12 #
 ******************************************************************************/
static int lsnd_conf_parse_agent_queue(xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* 加载队列信息 */
#define LSND_LOAD_QUEUE(xml, fix,  _path, conf) {\
        char node_path[FILE_PATH_MAX_LEN]; \
        \
        snprintf(node_path, sizeof(node_path), "%s.MAX", _path); \
        \
        node = xml_search(xml, fix, node_path); \
        if (NULL == node) { \
            return -1; \
        } \
        \
        (conf)->max = atoi(node->value.str); \
        \
        snprintf(node_path, sizeof(node_path), "%s.SIZE", _path); \
        \
        node = xml_search(xml, fix, node_path); \
        if (NULL == node) { \
            return -1; \
        } \
        \
        (conf)->size = atoi(node->value.str); \
    }

    /* > 定位队列标签 */
    fix = xml_query(xml, ".LISTEND.AGENT.QUEUE");
    if (NULL == fix) {
        log_error(log, "Get queue configuration failed!");
        return -1;
    }

    /* > 获取队列配置 */
    LSND_LOAD_QUEUE(xml, fix, ".CONNQ", &conf->connq);
    LSND_LOAD_QUEUE(xml, fix, ".RECVQ", &conf->recvq);
    LSND_LOAD_QUEUE(xml, fix, ".SENDQ", &conf->sendq);

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_conf_load_agent
 **功    能: 加载AGENT配置
 **输入参数: 
 **     path: 配置文件路径
 **     log: 日志对象
 **输出参数:
 **     lcf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 提取配置文件中的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-25 22:43:12 #
 ******************************************************************************/
static int lsnd_conf_load_agent(xml_tree_t *xml, lsnd_conf_t *lcf, log_cycle_t *log)
{
    xml_node_t *node;
    agent_conf_t *conf = &lcf->agent;

    snprintf(conf->path, sizeof(conf->path), "%s/agent/", lcf->wdir); /* 工作路径 */

    /* > 加载结点ID */
    conf->nid = lcf->nid;

    /* > 加载连接配置 */
    if (lsnd_conf_parse_agent_connections(xml, conf, log)) {
        log_error(log, "Parse connections of AGENTe configuration failed!");
        return -1;
    }

    /* > 加载队列配置 */
    if (lsnd_conf_parse_agent_queue(xml, conf, log)) {
        log_error(log, "Parse queue of AGENTe configuration failed!");
        return -1;
    }

    /* > 获取WORKER线程数 */
    node = xml_query(xml, ".LISTEND.AGENT.THREAD-POOL.WORKER");
    if (NULL == node) {
        log_error(log, "Didn't configure number of worker!");
        return -1;
    }

    conf->worker_num = atoi(node->value.str);

    /* > 获取AGENT线程数 */
    node = xml_query(xml, ".LISTEND.AGENT.THREAD-POOL.AGENT");
    if (NULL == node) {
        log_error(log, "Didn't configure number of agent!");
        return -1;
    }

    conf->agent_num = atoi(node->value.str);

    /* > 获取Listen线程数 */
    node = xml_query(xml, ".LISTEND.AGENT.THREAD-POOL.LSN");
    if (NULL == node) {
        log_error(log, "Didn't configure number of listen!");
        return -1;
    }

    conf->lsn_num = atoi(node->value.str);


    return 0;
}

/******************************************************************************
 **函数名称: lsnd_conf_load_frwder
 **功    能: 加载转发配置
 **输入参数: 
 **     path: 配置文件路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 提取配置文件中的数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-25 22:43:12 #
 ******************************************************************************/
static int lsnd_conf_load_frwder(xml_tree_t *xml, lsnd_conf_t *lcf, log_cycle_t *log)
{
    xml_node_t *parent, *node;
    rtmq_proxy_conf_t *conf = &lcf->frwder;

    parent = xml_query(xml, ".LISTEND.FRWDER");
    if (NULL == parent) {
        log_error(log, "Didn't find invertd configuation!");
        return -1;
    }

    /* > 设置结点ID */
    conf->nid = lcf->nid;
    snprintf(conf->path, sizeof(conf->path), "%s", lcf->wdir);

    /* > 服务端IP */
    node = xml_search(xml, parent, "SERVER.IP");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find SERVER.IP!");
        return -1;
    }

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "%s", node->value.str);

    node = xml_search(xml, parent, "SERVER.PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find SERVER.PORT!");
        return -1;
    }

    conf->port = atoi(node->value.str);

    /* > 鉴权信息 */
    node = xml_search(xml, parent, "AUTH.USR");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find AUTH.USR!");
        return -1;
    }

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

    node = xml_search(xml, parent, "AUTH.PASSWD");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find AUTH.PASSWD!");
        return -1;
    }

    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

    /* > 线程数目 */
    node = xml_search(xml, parent, "THREAD-POOL.SEND_THD_NUM");  /* 发送线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find THREAD-POOL.SEND_THD_NUM!");
        return -1;
    }

    conf->send_thd_num = atoi(node->value.str);
    if (0 == conf->send_thd_num) {
        log_error(log, "THREAD-POOL.SEND_THD_NUM is zero!");
        return -1;
    }

    node = xml_search(xml, parent, "THREAD-POOL.WORK_THD_NUM");  /* 工作线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find THREAD-POOL.WORK_THD_NUM!");
        return -1;
    }

    conf->work_thd_num = atoi(node->value.str);
    if (0 == conf->work_thd_num) {
        log_error(log, "THREAD-POOL.WORK_THD_NUM is zero!");
        return -1;
    }

    /* > 缓存大小配置 */
    node = xml_search(xml, parent, "BUFFER-POOL-SIZE.RECV");  /* 接收缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find BUFFER-POOL-SIZE.RECV!");
        return -1;
    }

    conf->recv_buff_size = atoi(node->value.str) * MB;
    if (0 == conf->recv_buff_size) {
        return -1;
    }

    /* > 接收队列 */
    node = xml_search(xml, parent, "RECVQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find RECVQ.MAX!");
        return -1;
    }

    conf->recvq.max = atoi(node->value.str);
    if (0 == conf->recvq.max) {
        log_error(log, "RECVQ.MAX is zero!");
        return -1;
    }

    node = xml_search(xml, parent, "RECVQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find RECVQ.SIZE!");
        return -1;
    }

    conf->recvq.size = atoi(node->value.str);
    if (0 == conf->recvq.size) {
        log_error(log, "RECVQ.SIZE is zero!");
        return -1;
    }

    /* > 发送队列 */
    node = xml_search(xml, parent, "SENDQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find SENDQ.MAX!");
        return -1;
    }

    conf->sendq.max = atoi(node->value.str);
    if (0 == conf->sendq.max) {
        log_error(log, "SENDQ.MAX is zero!");
        return -1;
    }

    node = xml_search(xml, parent, "SENDQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(log, "Didn't find SENDQ.SIZE!");
        return -1;
    }

    conf->sendq.size = atoi(node->value.str);
    if (0 == conf->sendq.size) {
        log_error(log, "SENDQ.SIZE is zero!");
        return -1;
    }

    return 0;
}

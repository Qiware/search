/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: lsnd_conf.c
 ** 版本号: 1.0
 ** 描  述: 代理服务配置
 **         负责从代理服务配置文件(lsnd.xml)中提取有效信息
 ** 作  者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#include "listend.h"
#include "syscall.h"
#include "xml_tree.h" 
#include "mem_pool.h"
#include "lsnd_conf.h"

static int lsnd_conf_parse(xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log);

static int lsnd_conf_load_comm(xml_tree_t *xml, lsnd_conf_t *conf, log_cycle_t *log);
static int lsnd_conf_load_agent(xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log);
static int lsnd_conf_load_frwd(xml_tree_t *xml, rtsd_conf_t *conf, log_cycle_t *log);

/* 加载配置信息 */
lsnd_conf_t *lsnd_load_conf(const char *path, log_cycle_t *log)
{
    xml_opt_t opt;
    lsnd_conf_t *conf;
    mem_pool_t *mem_pool;
    xml_tree_t *xml = NULL;

    /* > 创建配置内存池 */
    mem_pool = mem_pool_creat(4 * KB);
    if (NULL == mem_pool)
    {
        log_error(log, "Create memory pool failed!");
        return NULL;
    }

    do
    {
        /* > 创建配置对象 */
        conf = (lsnd_conf_t *)calloc(1, sizeof(lsnd_conf_t));
        if (NULL == conf)
        {
            log_error(log, "Alloc memory from pool failed!");
            break;
        }

        /* > 构建XML树 */
        memset(&opt, 0, sizeof(opt));

        opt.log = log;
        opt.pool = mem_pool;
        opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

        xml = xml_creat(path, &opt);
        if (NULL == xml)
        {
            log_error(log, "Create xml failed! path:%s", path);
            break;
        }

        /* > 加载通用配置 */
        if (lsnd_conf_load_comm(xml, conf, log))
        {
            log_error(log, "Load common configuration failed!");
            break;
        }

        /* > 加载AGENT配置 */
        if (lsnd_conf_load_agent(xml, &conf->agent, log))
        {
            log_error(log, "Load AGENT conf failed! path:%s", path);
            break;
        }

        /* > 加载转发配置 */
        if (lsnd_conf_load_frwd(xml, &conf->to_frwd, log))
        {
            log_error(log, "Load rttp conf failed! path:%s", path);
            break;
        }

        /* > 释放XML树 */
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

/* 加载公共配置 */
static int lsnd_conf_load_comm(xml_tree_t *xml, lsnd_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node;

    /* > 加载日志配置 */
    node = xml_query(xml, ".LISTEND.LOG.LEVEL");
    if (NULL == node
        || 0 == node->value.len)
    {
        conf->log_level = log_get_level(LOG_DEF_LEVEL_STR);
    }
    else
    {
        conf->log_level = log_get_level(node->value.str);
    }

    return LSND_OK;
}

/* 解析并发配置 */
static int lsnd_conf_parse_agent_connections(
        xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log)
{
    return LSND_OK;
}

/* 解析队列配置 */
static int lsnd_conf_parse_agent_queue(xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* 加载队列信息 */
#define LSND_LOAD_QUEUE(xml, fix,  _path, conf) \
    {\
        char node_path[FILE_PATH_MAX_LEN]; \
        \
        snprintf(node_path, sizeof(node_path), "%s.MAX", _path); \
        \
        node = xml_search(xml, fix, node_path); \
        if (NULL == node) \
        { \
            return LSND_ERR; \
        } \
        \
        (conf)->max = atoi(node->value.str); \
        \
        snprintf(node_path, sizeof(node_path), "%s.SIZE", _path); \
        \
        node = xml_search(xml, fix, node_path); \
        if (NULL == node) \
        { \
            return LSND_ERR; \
        } \
        \
        (conf)->size = atoi(node->value.str); \
    }

    /* > 定位队列标签 */
    fix = xml_query(xml, ".LISTEND.AGENT.QUEUE");
    if (NULL == fix)
    {
        log_error(log, "Get queue configuration failed!");
        return LSND_ERR;
    }

    /* > 获取队列配置 */
    LSND_LOAD_QUEUE(xml, fix, ".CONNQ", &conf->connq);
    LSND_LOAD_QUEUE(xml, fix, ".RECVQ", &conf->recvq);
    LSND_LOAD_QUEUE(xml, fix, ".SENDQ", &conf->sendq);

    return LSND_OK;
}

/* 加载AGENT配置 */
static int lsnd_conf_load_agent(xml_tree_t *xml, agent_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;

    /* > 定位并发配置 */
    fix = xml_query(xml, ".LISTEND.AGENT.CONNECTIONS");
    if (NULL == fix)
    {
        log_error(log, "Didn't configure connections!");
        return LSND_ERR;
    }

    node = xml_search(xml, fix, "MAX");         /* > 获取最大并发数 */
    if (NULL == node)
    {
        log_error(log, "Get max number of connections failed!");
        return LSND_ERR;
    }

    conf->connections.max = atoi(node->value.str);

    node = xml_search(xml, fix, "TIMEOUT");     /* > 获取连接超时时间 */
    if (NULL == node)
    {
        log_error(log, "Get timeout of connection failed!");
        return LSND_ERR;
    }

    conf->connections.timeout = atoi(node->value.str);

    /* > 获取侦听端口 */
    node = xml_search(xml, fix, "PORT");
    if (NULL == node)
    {
        log_error(log, "Get port of connection failed!");
        return LSND_ERR;
    }

    conf->connections.port = atoi(node->value.str);


    if (lsnd_conf_parse_agent_connections(xml, conf, log))
    {
        log_error(log, "Parse connections of AGENTe configuration failed!");
        return LSND_ERR;
    }

    /* > 加载连接配置 */
    if (lsnd_conf_parse_agent_queue(xml, conf, log))
    {
        log_error(log, "Parse queue of AGENTe configuration failed!");
        return LSND_ERR;
    }

    /* > 获取WORKER.NUM标签 */
    node = xml_query(xml, ".LISTEND.AGENT.WORKER.NUM");
    if (NULL == node)
    {
        log_error(log, "Didn't configure number of worker!");
        return LSND_ERR;
    }

    conf->worker_num = atoi(node->value.str);

    /* 4. 获取AGENT.NUM标签 */
    node = xml_query(xml, ".LISTEND.AGENT.AGENT.NUM");
    if (NULL == node)
    {
        log_error(log, "Didn't configure number of agent!");
        return LSND_ERR;
    }

    conf->agent_num = atoi(node->value.str);

    return LSND_OK;
}

static int _lsnd_conf_load_frwder(const char *path,
        const char *mark, rtsd_conf_t *conf, log_cycle_t *log)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *fix, *node;

    memset(&opt, 0, sizeof(opt));

    opt.log = log;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    /* > 创建XML树 */
    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        log_error(log, "Create XML failed! path:%s", path);
        return LSND_ERR;
    }

    do
    {
        fix = xml_query(xml, mark);
        if (NULL == fix)
        {
            log_error(log, "Didn't find mark [%d]! path:%s", mark, path);
            break;
        }

        /* > 结点ID */
        node = xml_search(xml, fix, "NDID");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find nodeid!");
            break;
        }

        conf->nodeid = atoi(node->value.str);

        /* > 设备名 */
        snprintf(conf->name, sizeof(conf->name), "%05d", conf->nodeid);

        /* > 服务端IP */
        node = xml_search(xml, fix, "SERVER.IP");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find server ip!");
            break;
        }

        snprintf(conf->ipaddr, sizeof(conf->ipaddr), "%s", node->value.str);

        node = xml_search(xml, fix, "SERVER.PORT");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find server port!");
            break;
        }

        conf->port = atoi(node->value.str);

        /* > 鉴权信息 */
        node = xml_search(xml, fix, "AUTH.USR");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find auth user!");
            break;
        }

        snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

        node = xml_search(xml, fix, "AUTH.PASSWD");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find auth passwd!");
            break;
        }

        snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

        /* > 线程数目 */
        node = xml_search(xml, fix, "THDNUM.SEND");  /* 发送线程数 */
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find send thread num!");
            break;
        }

        conf->send_thd_num = atoi(node->value.str);
        if (0 == conf->send_thd_num)
        {
            log_error(log, "Didn't find send thread num is zero!");
            break;
        }

        node = xml_search(xml, fix, "THDNUM.WORK");  /* 工作线程数 */
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find work thread num!");
            break;
        }

        conf->work_thd_num = atoi(node->value.str);
        if (0 == conf->work_thd_num)
        {
            log_error(log, "Didn't find work thread num is zero!");
            break;
        }

        /* > 缓存大小配置 */
        node = xml_search(xml, fix, "BUFF.SEND");    /* 发送缓存(MB) */
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find size of send buffer!");
            break;
        }

        conf->send_buff_size = atoi(node->value.str) * MB;
        if (0 == conf->send_buff_size)
        {
            log_error(log, "Didn't find size of send buffer is zero!");
            break;
        }

        node = xml_search(xml, fix, "BUFF.RECV");  /* 接收缓存(MB) */
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find size of recv buffer!");
            break;
        }

        conf->recv_buff_size = atoi(node->value.str) * MB;
        if (0 == conf->recv_buff_size)
        {
            log_error(log, "Didn't find size of recv buffer is zero!");
            break;
        }

        /* > 接收队列 */
        node = xml_search(xml, fix, "RECVQ.MAX");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find max of recvq!");
            break;
        }

        conf->recvq.max = atoi(node->value.str);
        if (0 == conf->recvq.max)
        {
            break;
        }

        node = xml_search(xml, fix, "SENDQ.SIZE");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find size of recvq!");
            break;
        }

        conf->recvq.size = atoi(node->value.str);
        if (0 == conf->recvq.size)
        {
            break;
        }

        /* > 发送队列 */
        node = xml_search(xml, fix, "SENDQ.MAX");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find max of sendq!");
            break;
        }

        conf->sendq.max = atoi(node->value.str);
        if (0 == conf->sendq.max)
        {
            break;
        }

        snprintf(conf->sendq.path, sizeof(conf->sendq.path),
                 "../temp/frwder/sendq-%05d.key", conf->nodeid);

        node = xml_search(xml, fix, "SENDQ.SIZE");
        if (NULL == node
            || 0 == node->value.len)
        {
            log_error(log, "Didn't find size of sendq!");
            break;
        }

        conf->sendq.size = atoi(node->value.str);
        if (0 == conf->sendq.size)
        {
            break;
        }

        xml_destroy(xml);
        return LSND_OK;
    } while(0);

    xml_destroy(xml);
    return LSND_ERR;
}

/* 加载RTTP配置 */
static int lsnd_conf_load_frwd(xml_tree_t *xml, rtsd_conf_t *conf, log_cycle_t *log)
{
    xml_node_t *node, *fix;
    char path[FILE_PATH_MAX_LEN], mark[FILE_PATH_MAX_LEN];

    /* > 获取配置路径和标签 */
    fix = xml_query(xml, ".LISTEND.FRWD");
    if (NULL == fix)
    {
        log_error(xml->log, "Didn't find frwd node!");
        return LSND_ERR;
    }

    node = xml_search(xml, fix, "PATH");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find frwd path!");
        return LSND_ERR;
    }

    snprintf(path, sizeof(path), "%s", node->value.str);
    
    node = xml_search(xml, fix, "MARK");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find frwd mark!");
        return LSND_ERR;
    }

    snprintf(mark, sizeof(mark), "%s", node->value.str);

    /* > 加载FRWD配置 */
    return _lsnd_conf_load_frwder(path, mark, conf, log);
}

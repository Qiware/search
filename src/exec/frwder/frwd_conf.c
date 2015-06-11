/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: frwd_conf.c
 ** 版本号: 1.0
 ** 描  述: 转发配置
 **         负责转发器配置(frwder.xml)的解析加载
 ** 作  者: # Qifeng.zou # 2015.06.09 #
 ******************************************************************************/

#include "frwd.h"
#include "xml_tree.h"

static int frwd_conf_load_comm(xml_tree_t *xml, frwd_conf_t *conf);
static int frwd_conf_load_frwder(xml_tree_t *xml, const char *path, rtsd_conf_t *conf);

/******************************************************************************
 **函数名称: frwd_load_conf
 **功    能: 加载配置信息
 **输入参数: 
 **     path: 配置路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 载入配置文件，再依次解析各标签内容
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_load_conf(const char *path, frwd_conf_t *conf)
{
    xml_opt_t opt;
    xml_tree_t *xml;

    memset(&opt, 0, sizeof(opt));
    memset(conf, 0, sizeof(frwd_conf_t));

    /* > 加载配置 */
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        return FRWD_ERR;
    }

    /* > 提取通用配置 */
    if (frwd_conf_load_comm(xml, conf))
    {
        xml_destroy(xml);
        return FRWD_ERR;
    }
    /* > 提取发送配置 */
    if (frwd_conf_load_frwder(xml, ".FRWDER.CONN-INVTD", &conf->conn_invtd))
    {
        xml_destroy(xml);
        return FRWD_ERR;
    }

    xml_destroy(xml);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_conf_load_comm
 **功    能: 加载通用配置
 **输入参数: 
 **     xml: XML树
 **输出参数:
 **     conf: 发送配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 11:38:57 #
 ******************************************************************************/
static int frwd_conf_load_comm(xml_tree_t *xml, frwd_conf_t *conf)
{
    xml_node_t *node;

    /* > 日志级别 */
    node = xml_query(xml, ".FRWDER.LOG.LEVEL");
    if (NULL == node
        || 0 == node->value.len)
    {
        conf->log_level = LOG_LEVEL_TRACE;
    }
    else
    {
        conf->log_level = atoi(node->value.str);
    }

    /* > 发送至Agentd */
    node = xml_query(xml, ".FRWDER.TO_LSND.PATH");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->to_listend, sizeof(conf->to_listend), "%s", node->value.str);

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_conf_load_frwder
 **功    能: 加载转发配置
 **输入参数: 
 **     xml: XML树
 **     path: 结点路径
 **输出参数:
 **     conf: 发送配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.09 #
 ******************************************************************************/
static int frwd_conf_load_frwder(xml_tree_t *xml, const char *path, rtsd_conf_t *conf)
{
    xml_node_t *parent, *node;

    parent = xml_query(xml, path);
    if (NULL == parent)
    {
        return FRWD_ERR;
    }

    /* > 结点ID */
    node = xml_search(xml, parent, "NODE");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->nodeid = atoi(node->value.str);

    /* > 工作路径 */
    node = xml_search(xml, parent, "PATH");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->path, sizeof(conf->path), "%s", node->value.str);

    /* > 服务端IP */
    node = xml_search(xml, parent, "SERVER.IP");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "%s", node->value.str);

    node = xml_search(xml, parent, "SERVER.PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->port = atoi(node->value.str);

    /* > 鉴权信息 */
    node = xml_search(xml, parent, "AUTH.USR");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

    node = xml_search(xml, parent, "AUTH.PASSWD");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

    /* > 线程数目 */
    node = xml_search(xml, parent, "THDNUM.SEND");  /* 发送线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->send_thd_num = atoi(node->value.str);
    if (0 == conf->send_thd_num)
    {
        return FRWD_ERR;
    }

    node = xml_search(xml, parent, "THDNUM.WORK");  /* 工作线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->work_thd_num = atoi(node->value.str);
    if (0 == conf->work_thd_num)
    {
        return FRWD_ERR;
    }

    /* > 缓存大小配置 */
    node = xml_search(xml, parent, "BUFF.SEND");    /* 发送缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->send_buff_size = atoi(node->value.str) * MB;
    if (0 == conf->send_buff_size)
    {
        return FRWD_ERR;
    }

    node = xml_search(xml, parent, "BUFF.RECV");  /* 接收缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->recv_buff_size = atoi(node->value.str) * MB;
    if (0 == conf->recv_buff_size)
    {
        return FRWD_ERR;
    }

    /* > 接收队列 */
    node = xml_search(xml, parent, "RECVQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->recvq.max = atoi(node->value.str);
    if (0 == conf->recvq.max)
    {
        return FRWD_ERR;
    }

    node = xml_search(xml, parent, "SENDQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->recvq.size = atoi(node->value.str);
    if (0 == conf->recvq.size)
    {
        return FRWD_ERR;
    }

    /* > 发送队列 */
    node = xml_search(xml, parent, "SENDQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->sendq.max = atoi(node->value.str);
    if (0 == conf->sendq.max)
    {
        return FRWD_ERR;
    }

    snprintf(conf->sendq.path,
         sizeof(conf->sendq.path), "%s/%05d.sq", conf->path, conf->nodeid);

    node = xml_search(xml, parent, "SENDQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->sendq.size = atoi(node->value.str);
    if (0 == conf->sendq.size)
    {
        return FRWD_ERR;
    }

    return FRWD_OK;
}

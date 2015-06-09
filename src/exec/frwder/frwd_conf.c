#include "frwd_conf.h"

#if defined(__RTTP_SUPPORT__)
static int frwd_conf_load_rttp(const char *path, rtsd_conf_t *conf);
#else /*__RTTP_SUPPORT__*/
static int frwd_conf_load_sdtp(const char *path, sdsd_conf_t *conf);
#endif /*__RTTP_SUPPORT__*/

/* 加载配置信息 */
int frwd_load_conf(const char *path, frwd_conf_t *conf)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;

    memset(&opt, 0, sizeof(opt));

    /* > 加载配置 */
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        return FRWD_ERR;
    }

    /* > 提取信息 */
#if defined(__RTTP_SUPPORT__)
    if (frwd_conf_load_rttp(".FRWD.CONN-INVTD", &conf->conn_invtd))
#else /*__RTTP_SUPPORT__*/
    if (frwd_conf_load_sdtp(".FRWD.CONN-INVTD", &conf->conn_invtd))
#endif /*__RTTP_SUPPORT__*/
    {
        return FRWD_ERR;
    }

    xml_destroy(xml);

    return FRWD_OK;
}

#if defined(__RTTP_SUPPORT__)
static int frwd_conf_load_rttp(const char *path, rtsd_conf_t *conf)
#else /*__RTTP_SUPPORT__*/
static int frwd_conf_load_rttp(const char *path, sdsd_conf_t *conf)
#endif /*__RTTP_SUPPORT__*/
{
    xml_node_t *parent, *node;

    parent = xml_query(xml, path);
    if (NULL == parent)
    {
        return FRWD_ERR;
    }

    /* > 设备ID */
    node = xml_rquery(xml, parent, "DEVID");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->devid = atoi(node->devid);

    /* > 设备名 */
    node = xml_rquery(xml, parent, "NAME");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->name, sizeof(conf->name), "%s", node->value.str);

    /* > 服务端IP */
    node = xml_rquery(xml, parent, "SERVER.IP");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "%s", node->value.str);

    node = xml_rquery(xml, parent, "SERVER.PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->port = atoi(node->value.str);

    /* > 鉴权信息 */
    conf->auth.devid = conf->devid;

    node = xml_rquery(xml, parent, "AUTH.USER");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

    node = xml_rquery(xml, parent, "AUTH.PASSWD");
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

    /* > 线程数目 */
    node = xml_rquery(xml, parent, "THDNUM.SEND");  /* 发送线程数 */
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

    node = xml_rquery(xml, parent, "THDNUM.WORK");  /* 工作线程数 */
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
    node = xml_rquery(xml, parent, "BUFF.SEND");    /* 发送缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->send_buff_size = atoi(node->value.str);
    if (0 == conf->send_buff_size)
    {
        return FRWD_ERR;
    }

    node = xml_rquery(xml, parent, "BUFF.RECV");  /* 接收缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        return FRWD_ERR;
    }

    conf->recv_buff_size = atoi(node->value.str);
    if (0 == conf->recv_buff_size)
    {
        return FRWD_ERR;
    }

    /* > 接收队列 */
    node = xml_rquery(xml, parent, "RECVQ.MAX");
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

    node = xml_rquery(xml, parent, "SENDQ.SIZE");
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
    node = xml_rquery(xml, parent, "SENDQ.MAX");
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

    node = xml_rquery(xml, parent, "SENDQ.SIZE");
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

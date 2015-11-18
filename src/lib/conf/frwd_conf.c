/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: frwd_conf.c
 ** 版本号: 1.0
 ** 描  述: 转发配置
 **         负责转发器配置(frwder.xml)的解析加载
 ** 作  者: # Qifeng.zou # 2015.06.09 #
 ******************************************************************************/

#include "xml_tree.h"
#include "frwd_conf.h"

static int frwd_conf_load_comm(xml_tree_t *xml, frwd_conf_t *conf);

/******************************************************************************
 **函数名称: frwd_load_conf
 **功    能: 加载配置信息
 **输入参数: 
 **     name: 结点名
 **     path: 配置路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 载入配置文件，再依次解析各标签内容
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_load_conf(const char *name, const char *path, frwd_conf_t *conf, log_cycle_t *log)
{
    xml_opt_t opt;
    xml_tree_t *xml;

    memset(&opt, 0, sizeof(opt));
    memset(conf, 0, sizeof(frwd_conf_t));

    /* > 加载配置 */
    opt.log = log;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        return -1;
    }

    /* > 提取通用配置 */
    if (frwd_conf_load_comm(xml, conf))
    {
        xml_destroy(xml);
        return -1;
    }

    if (strcasecmp(name, conf->name)) /* 校验结点名 */
    {
        xml_destroy(xml);
        return -1;
    }

    /* > 提取发送配置 */
    if (frwd_conf_load_frwder(xml, ".FRWDER.CONN-INVTD", &conf->conn_invtd))
    {
        xml_destroy(xml);
        return -1;
    }

    xml_destroy(xml);

    return 0;
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

    /* > 结点名 */
    node = xml_query(xml, ".FRWDER.NAME");
    if (NULL == node
        || 0 == node->value.len)
    {
        return -1;
    }

    snprintf(conf->name, sizeof(conf->name), "%s", node->value.str);

    /* > 发送至Agentd */
    node = xml_query(xml, ".FRWDER.LISTEND.NAME");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find .FRWDER.LISTEND.NAME!\n");
        return -1;
    }

    snprintf(conf->lsnd_name, sizeof(conf->lsnd_name), "%s", node->value.str);

    return 0;
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
int frwd_conf_load_frwder(xml_tree_t *xml, const char *path, rtsd_conf_t *conf)
{
    xml_node_t *parent, *node;

    parent = xml_query(xml, path);
    if (NULL == parent)
    {
        fprintf(stderr, "Didn't find %s!\n", path);
        return -1;
    }

    /* > 结点ID */
    node = xml_search(xml, parent, "NODE");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.NODE!\n", path);
        return -1;
    }

    conf->nodeid = atoi(node->value.str);

    /* > 工作路径 */
    node = xml_search(xml, parent, "PATH");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.PATH!\n", path);
        return -1;
    }

    snprintf(conf->path, sizeof(conf->path), "%s", node->value.str);

    /* > 服务端IP */
    node = xml_search(xml, parent, "SERVER.IP");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SERVER.IP!\n", path);
        return -1;
    }

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "%s", node->value.str);

    node = xml_search(xml, parent, "SERVER.PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SERVER.PORT!\n", path);
        return -1;
    }

    conf->port = atoi(node->value.str);

    /* > 鉴权信息 */
    node = xml_search(xml, parent, "AUTH.USR");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.AUTH.USR!\n", path);
        return -1;
    }

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

    node = xml_search(xml, parent, "AUTH.PASSWD");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.AUTH.PASSWD!\n", path);
        return -1;
    }

    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

    /* > 线程数目 */
    node = xml_search(xml, parent, "THREAD-POOL.SEND_THD_NUM");  /* 发送线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.THREAD-POOL.SEND_THD_NUM!\n", path);
        return -1;
    }

    conf->send_thd_num = atoi(node->value.str);
    if (0 == conf->send_thd_num)
    {
        fprintf(stderr, "%s.THREAD-POOL.SEND_THD_NUM is zero!\n", path);
        return -1;
    }

    node = xml_search(xml, parent, "THREAD-POOL.WORK_THD_NUM");  /* 工作线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.THREAD-POOL.WORK_THD_NUM!\n", path);
        return -1;
    }

    conf->work_thd_num = atoi(node->value.str);
    if (0 == conf->work_thd_num)
    {
        fprintf(stderr, "%s.THREAD-POOL.WORK_THD_NUM is zero!\n", path);
        return -1;
    }

    /* > 缓存大小配置 */
    node = xml_search(xml, parent, "BUFFER-POOL-SIZE.SEND");    /* 发送缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.BUFFER-POOL-SIZE.SEND!\n", path);
        return -1;
    }

    conf->send_buff_size = atoi(node->value.str) * MB;
    if (0 == conf->send_buff_size)
    {
        return -1;
    }

    node = xml_search(xml, parent, "BUFFER-POOL-SIZE.RECV");  /* 接收缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.BUFFER-POOL-SIZE.RECV!\n", path);
        return -1;
    }

    conf->recv_buff_size = atoi(node->value.str) * MB;
    if (0 == conf->recv_buff_size)
    {
        return -1;
    }

    /* > 接收队列 */
    node = xml_search(xml, parent, "RECVQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.RECVQ.MAX!\n", path);
        return -1;
    }

    conf->recvq.max = atoi(node->value.str);
    if (0 == conf->recvq.max)
    {
        fprintf(stderr, "%s.RECVQ.MAX is zero!\n", path);
        return -1;
    }

    node = xml_search(xml, parent, "RECVQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.RECVQ.SIZE!\n", path);
        return -1;
    }

    conf->recvq.size = atoi(node->value.str);
    if (0 == conf->recvq.size)
    {
        fprintf(stderr, "%s.RECVQ.SIZE is zero!\n", path);
        return -1;
    }

    /* > 发送队列 */
    node = xml_search(xml, parent, "SENDQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SENDQ.MAX!\n", path);
        return -1;
    }

    conf->sendq.max = atoi(node->value.str);
    if (0 == conf->sendq.max)
    {
        fprintf(stderr, "%s.SENDQ.MAX is zero!\n", path);
        return -1;
    }

    node = xml_search(xml, parent, "SENDQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SENDQ.SIZE!\n", path);
        return -1;
    }

    conf->sendq.size = atoi(node->value.str);
    if (0 == conf->sendq.size)
    {
        fprintf(stderr, "%s.SENDQ.SIZE is zero!\n", path);
        return -1;
    }

    return 0;
}

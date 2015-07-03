/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invtd_conf.c
 ** 版本号: 1.0
 ** 描  述: 加载倒排服务的配置信息
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 08:27:51 AM CST #
 ******************************************************************************/

#include "invtab.h"
#include "xml_tree.h"
#include "invtd_conf.h"

static int invtd_conf_load_sdtp(xml_tree_t *xml, rtrd_conf_t *conf);
static int invtd_conf_load_comm(xml_tree_t *xml, invtd_conf_t *conf);

/******************************************************************************
 **函数名称: invtd_conf_load
 **功    能: 加载配置信息
 **输入参数:
 **     path: 配置文件路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 从配置文件中依次取出配置数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_conf_load(const char *path, invtd_conf_t *conf)
{
    xml_opt_t opt;
    xml_tree_t *xml;

    /* > 创建XML树 */
    memset(&opt, 0, sizeof(opt));

    opt.log = NULL;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        xml_destroy(xml);
        return INVT_ERR_CONF;
    }

    /* > 加载SDTP配置 */
    if (invtd_conf_load_sdtp(xml, &conf->rtrd))
    {
        xml_destroy(xml);
        return INVT_ERR_CONF;
    }

    /* > 加载其他配置 */
    if (invtd_conf_load_comm(xml, conf))
    {
        xml_destroy(xml);
        return INVT_ERR_CONF;
    }

    xml_destroy(xml);

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_conf_load_sdtp
 **功    能: 加载SDTP配置信息
 **输入参数:
 **     path: 配置文件路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 从配置文件取出SDTP配置数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_conf_load_sdtp(xml_tree_t *xml, rtrd_conf_t *conf)
{
    xml_node_t *nail, *node;

    nail = xml_query(xml, ".INVTERD.SDTP"); 
    if (NULL == nail)
    {
        return INVT_ERR_CONF;
    }

    /* > 节点ID */
    node = xml_search(xml, nail, "NODE");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->nodeid = atoi(node->value.str);

    /* > 工作路径 */
    node = xml_search(xml, nail, "PATH");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    snprintf(conf->path, sizeof(conf->path), "%s", node->value.str);

    /* > 端口号 */
    node = xml_search(xml, nail, "PORT");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->port = atoi(node->value.str);

    /* > 鉴权配置信息 */
    node = xml_search(xml, nail, "AUTH.USR");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

    node = xml_search(xml, nail, "AUTH.PASSWD");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

    /* > 线程数配置 */
    node = xml_search(xml, nail, "THDNUM.RECV_THD_NUM");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->recv_thd_num = atoi(node->value.str);

    node = xml_search(xml, nail, "THDNUM.WORK_THD_NUM");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->work_thd_num = atoi(node->value.str);

    /* > 接收队列配置 */
    node = xml_search(xml, nail, "RECVQ.NUM");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->recvq_num = atoi(node->value.str);

    node = xml_search(xml, nail, "RECVQ.MAX");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->recvq.max = atoi(node->value.str);

    node = xml_search(xml, nail, "RECVQ.SIZE");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->recvq.size = atoi(node->value.str);

    /* > 发送队列配置 */
    conf->sendq.max = conf->recvq.max;
    conf->sendq.size = conf->recvq.size;

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_conf_load_comm
 **功    能: 加载其他配置信息
 **输入参数:
 **     path: 配置文件路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 从配置文件取出通用配置数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_conf_load_comm(xml_tree_t *xml, invtd_conf_t *conf)
{
    xml_node_t *node;

    /* > 加载日志配置 */
    node = xml_query(xml, ".INVTERD.LOG.LEVEL");
    if (NULL == node
        || 0 == node->value.len)
    {
        conf->log_level = log_get_level(LOG_DEF_LEVEL_STR);
    }
    else
    {
        conf->log_level = log_get_level(node->value.str);
    }

    /* > 倒排表长度 */
    node = xml_query(xml, ".INVTERD.INVT_TAB.MAX");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->invt_tab_max = atoi(node->value.str);

    return INVT_OK;
}

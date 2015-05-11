/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invtd_conf.c
 ** 版本号: 1.0
 ** 描  述: 加载倒排服务的配置信息
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 08:27:51 AM CST #
 ******************************************************************************/

#include "xml_tree.h"
#include "invert_tab.h"
#include "invtd_conf.h"

static int invtd_conf_load_sdtp(xml_tree_t *xml, invtd_conf_t *conf);
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
    if (invtd_conf_load_sdtp(xml, conf))
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
static int invtd_conf_load_sdtp(xml_tree_t *xml, invtd_conf_t *conf)
{
    xml_node_t *nail, *node;

    nail = xml_query(xml, ".INVTERD.SDTP"); 
    if (NULL == nail)
    {
        return INVT_ERR_CONF;
    }

    /* > SDTP名称 */
    node = xml_rquery(xml, nail, "NAME");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    snprintf(conf->sdtp.name, sizeof(conf->sdtp.name), "%s", node->value.str);

    /* > 端口号 */
    node = xml_rquery(xml, nail, "PORT");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->sdtp.port = atoi(node->value.str);

    /* > 线程数配置 */
    node = xml_rquery(xml, nail, "THDNUM.RECV_THD_NUM");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->sdtp.recv_thd_num = atoi(node->value.str);

    node = xml_rquery(xml, nail, "THDNUM.WORK_THD_NUM");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->sdtp.work_thd_num = atoi(node->value.str);

    /* > 接收队列配置 */
    node = xml_rquery(xml, nail, "RECVQ.NUM");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->sdtp.rqnum = atoi(node->value.str);

    node = xml_rquery(xml, nail, "RECVQ.MAX");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->sdtp.recvq.max = atoi(node->value.str);

    node = xml_rquery(xml, nail, "RECVQ.SIZE");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->sdtp.recvq.size = atoi(node->value.str);

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

    /* > 倒排表长度 */
    node = xml_query(xml, ".INVTERD.INVT_TAB.MAX");
    if (NULL == node)
    {
        return INVT_ERR_CONF;
    }

    conf->invt_tab_max = atoi(node->value.str);

    return INVT_OK;
}

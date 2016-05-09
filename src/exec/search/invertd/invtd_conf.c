/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invtd_conf.c
 ** 版本号: 1.0
 ** 描  述: 加载倒排服务的配置信息
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 08:27:51 AM CST #
 ******************************************************************************/

#include "invtab.h"
#include "xml_tree.h"
#include "invtd_conf.h"

static int invtd_conf_load_comm(xml_tree_t *xml, invtd_conf_t *conf);
static int invtd_conf_load_frwder(xml_tree_t *xml, rtmq_proxy_conf_t *conf, int nid);

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
int invtd_conf_load(const char *path, invtd_conf_t *conf, log_cycle_t *log)
{
    xml_opt_t opt;
    xml_tree_t *xml;

    /* > 创建XML树 */
    memset(&opt, 0, sizeof(opt));

    opt.log = log;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml) {
        return INVT_ERR_CONF;
    }

    /* > 加载其他配置 */
    if (invtd_conf_load_comm(xml, conf)) {
        xml_destroy(xml);
        return INVT_ERR_CONF;
    }

    /* > 加载DownStream配置 */
    if (invtd_conf_load_frwder(xml, &conf->frwder, conf->nid)) {
        xml_destroy(xml);
        return INVT_ERR_CONF;
    }

    xml_destroy(xml);

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_conf_load_frwder
 **功    能: 加载下行配置信息
 **输入参数:
 **     path: 配置文件路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 从配置文件取出SDTP配置数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_conf_load_frwder(xml_tree_t *xml, rtmq_proxy_conf_t *conf, int nid)
{
    xml_node_t *parent, *node;

    parent = xml_query(xml, ".INVERTD.FRWDER");
    if (NULL == parent) {
        log_error(xml->log, "Didn't find invertd configuation!");
        return -1;
    }

    /* > 设置结点ID */
    conf->nid = nid;

    /* > 服务端IP */
    node = xml_search(xml, parent, "SERVER.IP");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find SERVER.IP!");
        return -1;
    }

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "%s", node->value.str);

    node = xml_search(xml, parent, "SERVER.PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find SERVER.PORT!");
        return -1;
    }

    conf->port = atoi(node->value.str);

    /* > 鉴权信息 */
    node = xml_search(xml, parent, "AUTH.USR");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find AUTH.USR!");
        return -1;
    }

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "%s", node->value.str);

    node = xml_search(xml, parent, "AUTH.PASSWD");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find AUTH.PASSWD!");
        return -1;
    }

    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "%s", node->value.str);

    /* > 线程数目 */
    node = xml_search(xml, parent, "THREAD-POOL.SEND_THD_NUM");  /* 发送线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find THREAD-POOL.SEND_THD_NUM!");
        return -1;
    }

    conf->send_thd_num = atoi(node->value.str);
    if (0 == conf->send_thd_num) {
        log_error(xml->log, "THREAD-POOL.SEND_THD_NUM is zero!");
        return -1;
    }

    node = xml_search(xml, parent, "THREAD-POOL.WORK_THD_NUM");  /* 工作线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find THREAD-POOL.WORK_THD_NUM!");
        return -1;
    }

    conf->work_thd_num = atoi(node->value.str);
    if (0 == conf->work_thd_num) {
        log_error(xml->log, "THREAD-POOL.WORK_THD_NUM is zero!");
        return -1;
    }

    /* > 缓存大小配置 */
    node = xml_search(xml, parent, "BUFFER-POOL-SIZE.RECV");  /* 接收缓存(MB) */
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find BUFFER-POOL-SIZE.RECV!");
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
        log_error(xml->log, "Didn't find RECVQ.MAX!");
        return -1;
    }

    conf->recvq.max = atoi(node->value.str);
    if (0 == conf->recvq.max) {
        log_error(xml->log, "RECVQ.MAX is zero!");
        return -1;
    }

    node = xml_search(xml, parent, "RECVQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find RECVQ.SIZE!");
        return -1;
    }

    conf->recvq.size = atoi(node->value.str);
    if (0 == conf->recvq.size) {
        log_error(xml->log, "RECVQ.SIZE is zero!");
        return -1;
    }

    /* > 发送队列 */
    node = xml_search(xml, parent, "SENDQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find SENDQ.MAX!");
        return -1;
    }

    conf->sendq.max = atoi(node->value.str);
    if (0 == conf->sendq.max) {
        log_error(xml->log, "SENDQ.MAX is zero!");
        return -1;
    }

    node = xml_search(xml, parent, "SENDQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        log_error(xml->log, "Didn't find SENDQ.SIZE!");
        return -1;
    }

    conf->sendq.size = atoi(node->value.str);
    if (0 == conf->sendq.size) {
        log_error(xml->log, "SENDQ.SIZE is zero!");
        return -1;
    }

    return 0;
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

    /* > 结点ID */
    node = xml_query(xml, ".INVERTD.ID");
    if (NULL == node) {
        return INVT_ERR_CONF;
    }

    conf->nid = atoi(node->value.str);

    /* > 倒排表长度 */
    node = xml_query(xml, ".INVERTD.INVT_TAB.MAX");
    if (NULL == node) {
        return INVT_ERR_CONF;
    }

    conf->invt_tab_max = atoi(node->value.str);

    return INVT_OK;
}

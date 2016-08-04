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
#include "rtmq_recv.h"

static int frwd_conf_parse_comm(xml_tree_t *xml, frwd_conf_t *conf);
static int frwd_conf_parse_backend(xml_tree_t *xml, const char *path, frwd_conf_t *fcf);
static int frwd_conf_parse_forward(xml_tree_t *xml, const char *path, frwd_conf_t *fcf);

/******************************************************************************
 **函数名称: frwd_load_conf
 **功    能: 加载配置信息
 **输入参数: 
 **     path: 配置路径
 **     log: 日志对象
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 载入配置文件，再依次解析各标签内容
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_load_conf(const char *path, frwd_conf_t *conf, log_cycle_t *log)
{
    int ret = -1;
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
    if (NULL == xml) {
        return -1;
    }

    do {
        /* > 提取通用配置 */
        if (frwd_conf_parse_comm(xml, conf)) {
            break;
        }

        /* > 提取上游配置 */
        if (frwd_conf_parse_backend(xml, ".FRWDER.BACKEND", conf)) {
            break;
        }

        /* > 提取下游配置 */
        if (frwd_conf_parse_forward(xml, ".FRWDER.FORWARD", conf)) {
            break;
        }

        ret = 0;
    } while(0);

    xml_destroy(xml);

    return ret;
}

/******************************************************************************
 **函数名称: frwd_conf_parse_comm
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
static int frwd_conf_parse_comm(xml_tree_t *xml, frwd_conf_t *conf)
{
    xml_node_t *node;

    /* > 结点名ID */
    node = xml_query(xml, ".FRWDER.ID");
    if (NULL == node
        || 0 == node->value.len)
    {
        return -1;
    }

    conf->nid = str_to_num(node->value.str);

    /* > 结点名 */
    node = xml_query(xml, ".FRWDER.NAME");
    if (NULL == node
        || 0 == node->value.len)
    {
        return -1;
    }

    snprintf(conf->name, sizeof(conf->name), "%s", node->value.str);

    return 0;
}

/******************************************************************************
 **函数名称: frwd_conf_parse_backend
 **功    能: 加载backend配置
 **输入参数: 
 **     xml: XML树
 **     path: 结点路径
 **输出参数:
 **     fcf: 转发配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.09 #
 ******************************************************************************/
static int frwd_conf_parse_backend(xml_tree_t *xml, const char *path, frwd_conf_t *fcf)
{
    rtmq_auth_t *auth;
    xml_node_t *parent, *node, *usr, *passwd;
    rtmq_conf_t *conf = &fcf->backend;

    parent = xml_query(xml, path);
    if (NULL == parent) {
        fprintf(stderr, "Didn't find %s!\n", path);
        return -1;
    }

    /* > 结点ID */
    conf->nid = fcf->nid;

    /* > 帧听端口 */
    node = xml_search(xml, parent, "PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SERVER.PORT!\n", path);
        return -1;
    }

    conf->port = str_to_num(node->value.str);

    /* > 工作路径 */
    node = xml_search(xml, parent, "PATH");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.PATH!\n", path);
        return -1;
    }

    snprintf(conf->path, sizeof(conf->path), "%s", node->value.str);

    /* > 鉴权信息 */
    node = xml_search(xml, parent, "AUTH.ITEM");
    if (NULL == node) {
        fprintf(stderr, "Didn't find %s.AUTH.ITEM!\n", path);
        return -1;
    }

    conf->auth = list_creat(NULL);
    if (NULL == conf) {
        fprintf(stderr, "errmsg:[%d] %s\n", errno, strerror(errno));
        return -1;
    }

    for (; NULL != node; node = xml_brother(node)) {
        if (strcmp(node->name.str, "ITEM")) {
            continue;
        }

        usr = xml_search(xml, node, "USR");
        if ((NULL == usr) || (0 == usr->value.len)) {
            continue;
        }

        passwd = xml_search(xml, node, "PASSWD");
        if ((NULL == passwd) || (0 == passwd->value.len)) {
            continue;
        }

        auth = (rtmq_auth_t *)calloc(1, sizeof(rtmq_auth_t));
        if (NULL == auth) {
            fprintf(stderr, "errmsg:[%d] %s\n", errno, strerror(errno));
            return -1;
        }

        snprintf(auth->usr, sizeof(auth->usr), "%s", usr->value.str);
        snprintf(auth->passwd, sizeof(auth->passwd), "%s", passwd->value.str);

        list_lpush(conf->auth, auth);
    }

    /* > 线程数目 */
    node = xml_search(xml, parent, "THREAD-POOL.RECV_THD_NUM");  /* 发送线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.THREAD-POOL.SEND_THD_NUM!\n", path);
        return -1;
    }

    conf->recv_thd_num = str_to_num(node->value.str);
    if (0 == conf->recv_thd_num) {
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

    conf->work_thd_num = str_to_num(node->value.str);
    if (0 == conf->work_thd_num) {
        fprintf(stderr, "%s.THREAD-POOL.WORK_THD_NUM is zero!\n", path);
        return -1;
    }

    /* > 接收队列 */
    node = xml_search(xml, parent, "RECVQ.NUM");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.RECVQ.NUM!\n", path);
        return -1;
    }

    conf->recvq_num = str_to_num(node->value.str);
    if (0 == conf->recvq_num) {
        conf->recvq_num = 1;
    }

    node = xml_search(xml, parent, "RECVQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.RECVQ.MAX!\n", path);
        return -1;
    }

    conf->recvq.max = str_to_num(node->value.str);
    if (0 == conf->recvq.max) {
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

    conf->recvq.size = str_to_num(node->value.str);
    if (0 == conf->recvq.size) {
        fprintf(stderr, "%s.RECVQ.SIZE is zero!\n", path);
        return -1;
    }

    /* > 发送队列配置 */
    conf->sendq.max = conf->recvq.max;
    conf->sendq.size = conf->recvq.size;

    /* > 分发队列 */
    node = xml_search(xml, parent, "DISTQ.NUM");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.DISTQ.NUM!\n", path);
        return -1;
    }

    conf->distq_num = str_to_num(node->value.str);
    if (0 == conf->distq_num) {
        conf->distq_num = 1;
    }

    node = xml_search(xml, parent, "DISTQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SENDQ.MAX!\n", path);
        return -1;
    }

    conf->distq.max = str_to_num(node->value.str);
    if (0 == conf->distq.max) {
        fprintf(stderr, "%s.SENDQ.MAX is zero!\n", path);
        return -1;
    }

    node = xml_search(xml, parent, "DISTQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SENDQ.SIZE!\n", path);
        return -1;
    }

    conf->distq.size = str_to_num(node->value.str);
    if (0 == conf->distq.size) {
        fprintf(stderr, "%s.SENDQ.SIZE is zero!\n", path);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: frwd_conf_parse_forward
 **功    能: 加载Download配置
 **输入参数: 
 **     xml: XML树
 **     path: 结点路径
 **输出参数:
 **     fcf: 转发配置
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.09 #
 ******************************************************************************/
static int frwd_conf_parse_forward(xml_tree_t *xml, const char *path, frwd_conf_t *fcf)
{
    rtmq_auth_t *auth;
    rtmq_conf_t *conf = &fcf->forward;
    xml_node_t *parent, *node, *usr, *passwd;

    parent = xml_query(xml, path);
    if (NULL == parent) {
        fprintf(stderr, "Didn't find %s!\n", path);
        return -1;
    }

    /* > 结点ID */
    conf->nid = fcf->nid;

    /* > 帧听端口 */
    node = xml_search(xml, parent, "PORT");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SERVER.PORT!\n", path);
        return -1;
    }

    conf->port = str_to_num(node->value.str);

    /* > 工作路径 */
    node = xml_search(xml, parent, "PATH");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.PATH!\n", path);
        return -1;
    }

    snprintf(conf->path, sizeof(conf->path), "%s", node->value.str);

    /* > 鉴权信息 */
    node = xml_search(xml, parent, "AUTH.ITEM");
    if (NULL == node) {
        fprintf(stderr, "Didn't find %s.AUTH.ITEM!\n", path);
        return -1;
    }

    conf->auth = list_creat(NULL);
    if (NULL == conf) {
        fprintf(stderr, "errmsg:[%d] %s\n", errno, strerror(errno));
        return -1;
    }

    for (; NULL != node; node = xml_brother(node)) {
        if (strcmp(node->name.str, "ITEM")) {
            continue;
        }

        usr = xml_search(xml, node, "USR");
        if ((NULL == usr) || (0 == usr->value.len)) {
            continue;
        }

        passwd = xml_search(xml, node, "PASSWD");
        if ((NULL == passwd) || (0 == passwd->value.len)) {
            continue;
        }

        auth = (rtmq_auth_t *)calloc(1, sizeof(rtmq_auth_t));
        if (NULL == auth) {
            fprintf(stderr, "errmsg:[%d] %s\n", errno, strerror(errno));
            return -1;
        }

        snprintf(auth->usr, sizeof(auth->usr), "%s", usr->value.str);
        snprintf(auth->passwd, sizeof(auth->passwd), "%s", passwd->value.str);

        list_lpush(conf->auth, auth);
    }

    /* > 线程数目 */
    node = xml_search(xml, parent, "THREAD-POOL.RECV_THD_NUM");  /* 发送线程数 */
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.THREAD-POOL.SEND_THD_NUM!\n", path);
        return -1;
    }

    conf->recv_thd_num = str_to_num(node->value.str);
    if (0 == conf->recv_thd_num) {
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

    conf->work_thd_num = str_to_num(node->value.str);
    if (0 == conf->work_thd_num) {
        fprintf(stderr, "%s.THREAD-POOL.WORK_THD_NUM is zero!\n", path);
        return -1;
    }

    /* > 接收队列 */
    node = xml_search(xml, parent, "RECVQ.NUM");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.RECVQ.NUM!\n", path);
        return -1;
    }

    conf->recvq_num = str_to_num(node->value.str);
    if (0 == conf->recvq_num) {
        conf->recvq_num = 1;
    }

    node = xml_search(xml, parent, "RECVQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.RECVQ.MAX!\n", path);
        return -1;
    }

    conf->recvq.max = str_to_num(node->value.str);
    if (0 == conf->recvq.max) {
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

    conf->recvq.size = str_to_num(node->value.str);
    if (0 == conf->recvq.size) {
        fprintf(stderr, "%s.RECVQ.SIZE is zero!\n", path);
        return -1;
    }

    /* > 发送队列配置 */
    conf->sendq.max = conf->recvq.max;
    conf->sendq.size = conf->recvq.size;

    /* > 分发队列 */
    node = xml_search(xml, parent, "DISTQ.NUM");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.DISTQ.NUM!\n", path);
        return -1;
    }

    conf->distq_num = str_to_num(node->value.str);
    if (0 == conf->distq_num) {
        conf->distq_num = 1;
    }

    node = xml_search(xml, parent, "DISTQ.MAX");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SENDQ.MAX!\n", path);
        return -1;
    }

    conf->distq.max = str_to_num(node->value.str);
    if (0 == conf->distq.max) {
        fprintf(stderr, "%s.SENDQ.MAX is zero!\n", path);
        return -1;
    }

    node = xml_search(xml, parent, "DISTQ.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        fprintf(stderr, "Didn't find %s.SENDQ.SIZE!\n", path);
        return -1;
    }

    conf->distq.size = str_to_num(node->value.str);
    if (0 == conf->distq.size) {
        fprintf(stderr, "%s.SENDQ.SIZE is zero!\n", path);
        return -1;
    }

    return 0;
}

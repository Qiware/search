/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.c
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#include "crawler.h"
#include "xml_tree.h"
#include "crwl_comm.h"

static int crwl_get_conf(xml_tree_t *xml, crawler_conf_t *conf);

/******************************************************************************
 **函数名称: crwl_load_conf
 **功    能: 加载爬虫配置信息
 **输入参数:
 **     path: 配置路径
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载爬虫配置
 **     2. 提取配置信息
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_load_conf(crawler_conf_t *conf, const char *path)
{
    int ret;
    xml_tree_t *xml;

    /* 1. 加载爬虫配置 */
    xml = xml_creat(NULL, path);
    if (NULL == xml)
    {
        return CRWL_OK;
    }

    /* 2. 提取爬虫配置 */
    ret = crwl_get_conf(xml, conf);
    if (0 != ret)
    {
        xml_destroy(xml);
        return CRWL_FAIL;
    }

    xml_destroy(xml);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_start_work
 **功    能: 启动爬虫服务
 **输入参数: 
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
int crwl_start_work(crawler_conf_t *conf)
{
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_get_conf
 **功    能: 提取配置信息
 **输入参数: 
 **     xml: 配置文件
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.05 #
 ******************************************************************************/
static int crwl_get_conf(xml_tree_t *xml, crawler_conf_t *conf)
{
    return CRWL_OK;
}

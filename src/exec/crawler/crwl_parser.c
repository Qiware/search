/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_parser.c
 ** 版本号: 1.0
 ** 描  述: 超链接的提取程序
 **         从爬取的网页中提取超链接
 ** 作  者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "list.h"
#include "http.h"
#include "redis.h"
#include "xd_str.h"
#include "common.h"
#include "syscall.h"
#include "crawler.h"
#include "xml_tree.h"
#include "crwl_conf.h"
#include "crwl_parser.h"

#define CRWL_PARSER_LOG_NAME    "parser"

static int crwl_parser_webpage_info(crwl_webpage_info_t *info);
static int crwl_parser_work_flow(crwl_parser_t *parser);
static int crwl_parser_deep_hdl(crwl_parser_t *parser, gumbo_result_t *result);

bool crwl_set_uri_exists(redis_cluster_t *cluster, const char *hash, const char *uri);

/* 判断uri是否已下载 */
#define crwl_is_uri_down(cluster, hash, uri) crwl_set_uri_exists(cluster, hash, uri)

/* 判断uri是否已推送 */
#define crwl_is_uri_push(cluster, hash, uri) crwl_set_uri_exists(cluster, hash, uri)

/******************************************************************************
 **函数名称: crwl_parser_init
 **功    能: 初始化Parser对象
 **输入参数: 
 **     conf: 配置信息
 **     log: 日志信息
 **输出参数:
 **返    回: Parser对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
crwl_parser_t *crwl_parser_init(crwl_conf_t *conf, log_cycle_t *log)
{
    crwl_parser_t *parser;

    /* 1. 申请对象空间 */
    parser = (crwl_parser_t *)calloc(1, sizeof(crwl_parser_t));
    if (NULL == parser)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    parser->log = log;
    parser->conf = conf;

    log_set_level(log, conf->log.level);
    log2_set_level(conf->log.level2);

    /* 2. 连接Redis集群 */
    parser->redis = redis_cluster_init(&conf->redis.master, &conf->redis.slave_list);
    if (NULL == parser->redis)
    {
        log_error(parser->log, "Initialize redis context failed!");
        free(parser);
        return NULL;
    }

    return parser;
}

/******************************************************************************
 **函数名称: crwl_parser_destroy
 **功    能: 销毁Parser对象
 **输入参数: 
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
void crwl_parser_destroy(crwl_parser_t *parser)
{
    if (parser->log)
    {
        log_destroy(&parser->log);
        parser->log = NULL;
    }
    log2_destroy();
    if (parser->redis)
    {
        redis_cluster_destroy(parser->redis);
        parser->redis = NULL;
    }
    if (parser->conf)
    {
        crwl_conf_destroy(parser->conf);
        parser->conf = NULL;
    }
    free(parser);
}

/******************************************************************************
 **函数名称: crwl_parser_work
 **功    能: 网页解析处理
 **输入参数: 
 **     p: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
int crwl_parser_work(crwl_parser_t *parser)
{
    DIR *dir;
    struct stat st;
    struct dirent *item;
    char path[PATH_NAME_MAX_LEN],
         new_path[FILE_PATH_MAX_LEN],
         html_path[FILE_PATH_MAX_LEN];
    crwl_conf_t *conf = parser->conf;

    while (1)
    {
        snprintf(path, sizeof(path), "%s/wpi", conf->download.path);

        /* 1. 打开目录 */
        dir = opendir(path);
        if (NULL == dir)
        {
            Mkdir(path, 0777);
            continue;
        }

        /* 2. 遍历文件 */
        while (NULL != (item = readdir(dir)))
        {
            snprintf(parser->info.fname,
                    sizeof(parser->info.fname), "%s/%s", path, item->d_name); 

            /* 判断文件类型 */
            stat(parser->info.fname, &st);
            if (!S_ISREG(st.st_mode))
            {
                continue;
            }

            /* 获取网页信息 */
            if (crwl_parser_webpage_info(&parser->info))
            {
                snprintf(new_path, sizeof(new_path),
                        "%s/%s", conf->parser.store.err_path, item->d_name);
                rename(parser->info.fname, new_path);

                snprintf(html_path, sizeof(html_path),
                        "%s/%s", conf->download.path, parser->info.html);
                remove(html_path);

                log_error(parser->log, "Get webpage information failed! fname:%s",
                        parser->info.fname);
                continue;
            }

            /* 主处理流程 */
            crwl_parser_work_flow(parser);

            snprintf(new_path, sizeof(new_path),
                    "%s/%s", conf->parser.store.path, item->d_name);

            rename(parser->info.fname, new_path);


            snprintf(html_path, sizeof(html_path),
                    "%s/%s", conf->download.path, parser->info.html);
            remove(html_path);
        }

        /* 3. 关闭目录 */
        closedir(dir);

        Mkdir(conf->parser.store.path, 0777);

        Sleep(5);
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_parser_webpage_info
 **功    能: 获取网页信息
 **输入参数:
 **     info: 网页信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
static int crwl_parser_webpage_info(crwl_webpage_info_t *info)
{
    xml_tree_t *xml;
    xml_node_t *node, *fix;

    /* 1. 新建XML树 */
    xml = xml_creat(info->fname);
    if (NULL == xml)
    {
        return CRWL_ERR;
    }

    /* 2. 提取网页信息 */
    do
    {
        fix = xml_search(xml, ".INFO");
        if (NULL == fix)
        {
            break;
        }

        /* 获取URI字段 */
        node = xml_rsearch(xml, fix, "URI");
        if (NULL == fix)
        {
            break;
        }

        snprintf(info->uri, sizeof(info->uri), "%s", node->value);

        /* 获取DEPTH字段 */
        node = xml_rsearch(xml, fix, "URI.DEPTH");
        if (NULL == fix)
        {
            break;
        }

        info->depth = atoi(node->value);

        /* 获取IP字段 */
        node = xml_rsearch(xml, fix, "URI.IP");
        if (NULL == fix)
        {
            break;
        }

        snprintf(info->ip, sizeof(info->ip), "%s", node->value);

        /* 获取PORT字段 */
        node = xml_rsearch(xml, fix, "URI.PORT");
        if (NULL == fix)
        {
            break;
        }

        info->port = atoi(node->value);

        /* 获取HTML字段 */
        node = xml_rsearch(xml, fix, "HTML");
        if (NULL == fix)
        {
            break;
        }

        snprintf(info->html, sizeof(info->html), "%s", node->value);

        xml_destroy(xml);
        return CRWL_OK;
    } while(0);

    /* 3. 释放XML树 */
    xml_destroy(xml);
    return CRWL_ERR;
}

/******************************************************************************
 **函数名称: crwl_parser_work_flow
 **功    能: 解析器处理流程
 **输入参数: 
 **     p: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_parser_work_flow(crwl_parser_t *parser)
{
    gumbo_html_t *html;             /* HTML对象 */
    gumbo_result_t *result;         /* 结果集合 */
    char fpath[FILE_PATH_MAX_LEN];  /* HTML文件名 */
    crwl_conf_t *conf = parser->conf;
    crwl_webpage_info_t *info = &parser->info;

    /* 1. 判断网页深度 */
    if (info->depth > conf->download.depth)
    {
        log_info(parser->log, "Drop handle webpage! uri:%s depth:%d",
                info->uri, info->depth);
        return CRWL_OK;
    }

    /* 判断网页(URI)是否已下载
     *  判断的同时设置网页的下载标志
     *  如果已下载，则不做提取该网页中的超链接
     * */
    if (crwl_is_uri_down(parser->redis, conf->redis.done_tab, info->uri))
    {
        log_info(parser->log, "Uri [%s] was downloaded!", info->uri);
        return CRWL_OK;
    }

    snprintf(fpath, sizeof(fpath), "%s/%s", conf->download.path, info->html);

    /* 2. 解析HTML文件 */
    html = gumbo_html_parse(fpath);
    if (NULL == html)
    {
        log_error(parser->log, "Parse html failed! fpath:%s", fpath);
        return CRWL_ERR;
    }

    /* 3. 提取超链接 */
    result = gumbo_parse_href(html);
    if (NULL == result)
    {
        log_error(parser->log, "Parse href failed! fpath:%s", fpath);

        gumbo_result_destroy(result);
        gumbo_html_destroy(html);
        return CRWL_ERR;
    }

    /* 4. 深入处理超链接
     *  1. 判断超链接深度
     *  2. 判断超链接是否已被爬取
     *  3. 将超链接插入任务队列
     * */
    if (crwl_parser_deep_hdl(parser, result))
    {
        log_error(parser->log, "Deep handler failed! fpath:%s", fpath);

        gumbo_result_destroy(result);
        gumbo_html_destroy(html);
        return CRWL_ERR;
    }

    /* 5. 内存释放 */
    gumbo_result_destroy(result);
    gumbo_html_destroy(html);
    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_parser_deep_hdl
 **功    能: 超链接的深入分析和处理
 **输入参数: 
 **     p: 解析器对象
 **     result: URI集合
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断超链接深度
 **     2. 判断超链接是否已被爬取
 **     3. 将超链接插入任务队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int crwl_parser_deep_hdl(crwl_parser_t *parser, gumbo_result_t *result)
{
    int len;
    redisReply *r; 
    uri_field_t field;
    char task_str[CRWL_TASK_STR_LEN];
    crwl_conf_t *conf = parser->conf;
    list_node_t *node = result->list.head;
    crwl_webpage_info_t *info = &parser->info;

    /* 遍历URL集合 */
    for (; NULL != node; node = node->next)
    {
        /* 1. 将href转至uri */
        if (0 != href_to_uri((const char *)node->data, info->uri, &field))
        {
            log_info(parser->log, "Uri [%s] is invalid!", (char *)node->data);
            continue;
        }

        /* 2. 判断URI是否已经被推送到队列中 */
        if (crwl_is_uri_push(parser->redis, conf->redis.push_tab, field.uri))
        {
            log_info(parser->log, "Uri [%s] was pushed!", (char *)node->data);
            continue;
        }

        /* 3. 组装任务格式 */
        len = crwl_get_task_str(task_str, sizeof(task_str), field.uri, info->depth+1);
        if (len >= sizeof(task_str))
        {
            log_info(parser->log, "Task string is too long! [%s]", task_str);
            continue;
        }

        /* 4. 插入Undo任务队列 */
        r = redis_rpush(parser->redis->master, parser->conf->redis.undo_taskq, task_str);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            log_error(parser->log, "Push into undo task queue failed!");
            return CRWL_ERR;
        }

        freeReplyObject(r);
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_set_uri_exists
 **功    能: 设置uri是否已存在
 **输入参数: 
 **     cluster: Redis集群
 **     hash: 哈希表名
 **     uri: 判断对象-URI
 **输出参数:
 **返    回: true:已下载 false:未下载
 **实现描述: 
 **     1) 当URI已存在时, 返回true;
 **     2) 当URI不存在时, 返回false, 并设置uri的值为1.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
bool crwl_set_uri_exists(redis_cluster_t *cluster, const char *hash, const char *uri)
{
    redisReply *r;

    if (0 == cluster->slave_num)
    {
        return !redis_hsetnx(cluster->master, hash, uri, "1");
    }

    do
    {
        r = redisCommand(
                cluster->slave[random() % cluster->slave_num],
                "HEXISTS %s %s", hash, uri);
        if (REDIS_REPLY_INTEGER != r->type)
        {
            break;
        }

        if (0 == r->integer)
        {
            break;
        }

        freeReplyObject(r);
        return true; /* 已存在 */
    } while(0);

    freeReplyObject(r);

    return !redis_hsetnx(cluster->master, hash, uri, "1");
}

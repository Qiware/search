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

static crwl_parser_t *crwl_parser_init(const crwl_conf_t *conf, log_cycle_t *log);
static int crwl_parser_loop(crwl_parser_t *parser);
static int crwl_parser_webpage_info(crwl_parser_t *parser);
static int crwl_parser_work_flow(crwl_parser_t *parser);
static int crwl_parser_deep_hdl(crwl_parser_t *parser, gumbo_result_t *result);

bool uri_reslove_ex(const char *_uri, const char *parent, uri_field_t *field);

/* 判断URI是否已下载 */
#define crwl_is_uri_down(ctx, hash, uri) (!redis_hsetnx(ctx, hash, uri, "1"))
/* 判断URI是否已推送至队列 */
#define crwl_is_uri_push(ctx, hash, uri) (!redis_hsetnx(ctx, hash, uri, "1"))

/******************************************************************************
 **函数名称: crwl_parser_exec
 **功    能: 解析器主接口
 **输入参数: 
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
int crwl_parser_exec(const crwl_conf_t *conf)
{
    int ret;
    crwl_parser_t *parser;
    log_cycle_t *log;
    char log_path[FILE_PATH_MAX_LEN];

    /* 1. 初始化日志模块 */
    log2_get_path(log_path, sizeof(log_path), CRWL_PARSER_LOG_NAME);

    ret = log2_init(conf->log2_level, log_path);
    if (0 != ret)
    {
        fprintf(stderr, "Init log2 failed! level:%d path:%s\n",
                conf->log2_level, log_path);
        return CRWL_ERR;
    }

    log_get_path(log_path, sizeof(log_path), CRWL_PARSER_LOG_NAME);

    log = log_init(conf->log_level, log_path);
    if (NULL == log)
    {
        fprintf(stderr, "Init log2 failed! level:%d path:%s\n",
                conf->log_level, log_path);
        return CRWL_ERR;
    }

    /* 2. 初始化Parser对象 */
    parser = crwl_parser_init(conf, log);
    if (NULL == parser)
    {
        log_error(log, "Init parser failed!");
        return CRWL_ERR;
    }

    /* 遍历网页信息 */
    crwl_parser_loop(parser);

    /* 3. 释放GUMBO对象 */
    gumbo_destroy(&parser->gumbo_ctx);
    return CRWL_OK;
}

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
static crwl_parser_t *crwl_parser_init(const crwl_conf_t *conf, log_cycle_t *log)
{
    int ret;
    struct timeval tv;
    crwl_parser_t *parser;

    parser = (crwl_parser_t *)calloc(1, sizeof(crwl_parser_t));
    if (NULL == parser)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 初始化GUMBO对象 */
    ret = gumbo_init(&parser->gumbo_ctx);
    if (0 != ret)
    {
        log_error(log, "Init gumbo failed!");
        free(parser);
        return NULL;
    }

    /* 3. 连接Redis服务 */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    parser->redis_ctx = redisConnectWithTimeout("127.0.0.1", 6379, tv);
    if (NULL == parser->redis_ctx)
    {
        log_error(parser->log, "Connect redis failed!");
        gumbo_destroy(&parser->gumbo_ctx);
        free(parser);
        return NULL;
    }

    parser->log = log;
    memcpy(&parser->conf, conf, sizeof(crwl_conf_t));

    return parser;
}

/******************************************************************************
 **函数名称: crwl_parser_loop
 **功    能: 遍历网页信息
 **输入参数: 
 **     p: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
static int crwl_parser_loop(crwl_parser_t *parser)
{
    int ret;
    DIR *dir;
    struct stat st;
    struct dirent *item;
    char path[PATH_NAME_MAX_LEN];

    while (1)
    {
        snprintf(path, sizeof(path), "%s/info", parser->conf.download.path);

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
            ret = crwl_parser_webpage_info(parser);
            if (CRWL_OK != ret)
            {
                remove(parser->info.fname);

                log_error(parser->log, "Get webpage information failed! fname:%s",
                        parser->info.fname);
                continue;
            }

            /* 主处理流程 */
            crwl_parser_work_flow(parser);

            remove(parser->info.fname);
        }

        /* 3. 关闭目录 */
        closedir(dir);

        Sleep(5);
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_parser_webpage_info
 **功    能: 获取网页信息
 **输入参数: 
 **     p: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
static int crwl_parser_webpage_info(crwl_parser_t *parser)
{
    xml_tree_t *xml;
    xml_node_t *node, *fix;
    crwl_webpage_info_t *info = &parser->info;

    /* 1. 新建XML树 */
    xml = xml_creat(parser->info.fname);
    if (NULL == xml)
    {
        log_error(parser->log, "Create xml tree failed!");
        return CRWL_ERR;
    }

    /* 2. 提取网页信息 */
    do
    {
        fix = xml_search(xml, ".INFO");
        if (NULL == fix)
        {
            log_error(parser->log, "Didn't find INFO mark!");
            break;
        }

        /* 获取URI字段 */
        node = xml_rsearch(xml, fix, "URI");
        if (NULL == fix)
        {
            log_error(parser->log, "Didn't find INFO mark!");
            break;
        }

        snprintf(info->uri, sizeof(info->uri), "%s", node->value);

        /* 获取DEPTH字段 */
        node = xml_rsearch(xml, fix, "URI.DEPTH");
        if (NULL == fix)
        {
            log_error(parser->log, "Didn't find INFO mark!");
            break;
        }

        info->depth = atoi(node->value);

        /* 获取IP字段 */
        node = xml_rsearch(xml, fix, "URI.IP");
        if (NULL == fix)
        {
            log_error(parser->log, "Didn't find IP mark!");
            break;
        }

        snprintf(info->ip, sizeof(info->ip), "%s", node->value);

        /* 获取PORT字段 */
        node = xml_rsearch(xml, fix, "URI.PORT");
        if (NULL == fix)
        {
            log_error(parser->log, "Didn't find PORT mark!");
            break;
        }

        info->port = atoi(node->value);

        /* 获取HTML字段 */
        node = xml_rsearch(xml, fix, "HTML");
        if (NULL == fix)
        {
            log_error(parser->log, "Didn't find HTML mark!");
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
    int ret;
    gumbo_html_t *html;             /* HTML对象 */
    gumbo_result_t *result;         /* 结果集合 */
    char fname[FILE_PATH_MAX_LEN];  /* HTML文件名 */
    crwl_conf_t *conf = &parser->conf;
    crwl_webpage_info_t *info = &parser->info;

    /* 1. 判断网页深度 */
    if (info->depth > conf->download.depth)
    {
        log_info(parser->log, "Drop handle webpage! uri:%s depth:%d",
                info->uri, info->depth);
        return CRWL_OK;
    }

    /* 判断网页(URI)是否已下载
     * 判断的同时设置网页的下载标志
     * 如果已下载，则不做提取该网页中的超链接
     * */
    if (crwl_is_uri_down(parser->redis_ctx, conf->redis.done_tab, info->uri))
    {
        log_info(parser->log, "Uri [%s] was downloaded!", info->uri);
        return CRWL_OK;
    }


    snprintf(fname, sizeof(fname), "%s/%s", conf->download.path, info->html);

    /* 2. 解析HTML文件 */
    html = gumbo_html_parse(&parser->gumbo_ctx, fname);
    if (NULL == html)
    {
        log_error(parser->log, "Parse html failed! fname:%s", fname);
        return CRWL_ERR;
    }

    /* 3. 提取超链接 */
    result = gumbo_parse_href(&parser->gumbo_ctx, html);
    if (NULL == result)
    {
        gumbo_html_destroy(&parser->gumbo_ctx, html);

        log_error(parser->log, "Parse href failed! fname:%s", fname);
        return CRWL_ERR;
    }

    /* 4. 深入处理超链接
     *  1. 判断超链接深度
     *  2. 判断超链接是否已被爬取
     *  3. 将超链接插入任务队列 */
    ret = crwl_parser_deep_hdl(parser, result);
    if (CRWL_OK != ret)
    {
        log_error(parser->log, "Deep handler failed! fname:%s", fname);
        return CRWL_ERR;
    }

    /* 5. 内存释放 */
    gumbo_result_destroy(&parser->gumbo_ctx, result);
    gumbo_html_destroy(&parser->gumbo_ctx, html);
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
    crwl_conf_t *conf = &parser->conf;
    list_node_t *node = result->list.head;
    crwl_webpage_info_t *info = &parser->info;

    /* 遍历URL集合 */
    for (; NULL != node; node = node->next)
    {
        /* 1. 分析href字段 */
        if (0 != href_to_uri((const char *)node->data, info->uri, &field))
        {
            log_info(parser->log, "Uri [%s] is invalid!", (char *)node->data);
            continue;
        }

        /* 2. 判断URI是否已经被推送到队列中 */
        if (crwl_is_uri_push(parser->redis_ctx, conf->redis.push_tab, field.uri))
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
        r = redis_rpush(parser->redis_ctx, parser->conf.redis.undo_taskq, task_str);
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

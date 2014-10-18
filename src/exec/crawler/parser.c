/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: parse.c
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
#include "common.h"
#include "parser.h"
#include "syscall.h"
#include "xml_tree.h"

static int parser_trav_webpage_info(parser_t *parser);
static int parser_get_webpage_info(parser_t *parser);
static int parser_work_flow(parser_t *parser);
static int parser_deep_handler(parser_t *parser);

int main(int argc, char *argv[])
{
    int ret;
    parser_t parser;
    log_cycle_t *log;
    struct timeval tv;
    char log_path[FILE_PATH_MAX_LEN];

    memset(&parser, 0, sizeof(parser));

    /* 1. 初始化日志模块 */
    log2_get_path(log_path, sizeof(log_path), basename(argv[0]));

    ret = log2_init(LOG_LEVEL_DEBUG, log_path);
    if (0 != ret)
    {
        fprintf(stderr, "Init log2 failed! level:%s path:%s\n",
                PARSER_LOG2_LEVEL, log_path);
        return -1;
    }

    log_get_path(log_path, sizeof(log_path), basename(argv[0]));

    log = log_init(LOG_LEVEL_DEBUG, log_path);
    if (NULL == log)
    {
        log2_error("Init log failed!");
        return -1;
    }

    parser.log = log;

    /* 2. 初始化GUMBO对象 */
    ret = gumbo_init(&parser.gumbo_ctx);
    if (0 != ret)
    {
        log_error(parser.log, "Init gumbo failed!");
        return -1;
    }

    tv.tv_sec = 30;
    tv.tv_usec = 0;
    parser.redis_ctx = redisConnectWithTimeout("127.0.0.1", 6379, tv);
    if (NULL == parser.redis_ctx)
    {
        fprintf(stderr, "Connect redis failed!");
        return -1;
    }

    while (1)
    {
        parser_trav_webpage_info(&parser);

        Sleep(5);
    }

    /* 3. 释放GUMBO对象 */
    gumbo_destroy(&parser.gumbo_ctx);
    return 0;
}

/******************************************************************************
 **函数名称: parser_trav_webpage_info
 **功    能: 遍历网页信息
 **输入参数: 
 **     parser: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
static int parser_trav_webpage_info(parser_t *parser)
{
    DIR *dir;
    struct stat st;
    struct dirent *item;
    char path[PATH_NAME_MAX_LEN];

    snprintf(path, sizeof(path), "../temp/webpage/info");

    /* 1. 打开目录 */
    dir = opendir(path);
    if (NULL == dir)
    {
        Mkdir(path, 0777);
        return 0;
    }

    /* 2. 遍历文件 */
    while (NULL != (item = readdir(dir)))
    {
        snprintf(parser->info.finfo,
                sizeof(parser->info.finfo), "%s/%s", path, item->d_name); 

        /* 判断文件类型 */
        stat(parser->info.finfo, &st);
        if (!S_ISREG(st.st_mode))
        {
            continue;
        }

        parser_get_webpage_info(parser);

        /* 调用解析处理流程 */
        parser_work_flow(parser);
    }

    closedir(dir);
    return 0;
}

/******************************************************************************
 **函数名称: parser_get_webpage_info
 **功    能: 解析网页信息
 **输入参数: 
 **     parser: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
static int parser_get_webpage_info(parser_t *parser)
{
    xml_tree_t *xml;
    xml_node_t *node, *fix;
    parser_webpage_info_t *info = &parser->info;

    xml = xml_creat(parser->info.finfo);
    if (NULL == xml)
    {
        log2_error("Create xml tree failed!");
        return -1;
    }

    fix = xml_search(xml, ".INFO");
    if (NULL == fix)
    {
        log2_error("Didn't find INFO mark!");
        xml_destroy(xml);
        return -1;
    }

    /* 获取URI字段 */
    node = xml_rsearch(xml, fix, "URI");
    if (NULL == fix)
    {
        log2_error("Didn't find INFO mark!");
        xml_destroy(xml);
        return -1;
    }

    snprintf(info->uri, sizeof(info->uri), "%s", node->value);

    /* 获取DEEP字段 */
    node = xml_rsearch(xml, fix, "URI.DEEP");
    if (NULL == fix)
    {
        log2_error("Didn't find INFO mark!");
        xml_destroy(xml);
        return -1;
    }

    info->deep = atoi(node->value);

    /* 获取IPADDR字段 */
    node = xml_rsearch(xml, fix, "URI.IPADDR");
    if (NULL == fix)
    {
        log2_error("Didn't find IPADDR mark!");
        xml_destroy(xml);
        return -1;
    }

    snprintf(info->ipaddr, sizeof(info->ipaddr), "%s", node->value);

    /* 获取PORT字段 */
    node = xml_rsearch(xml, fix, "URI.PORT");
    if (NULL == fix)
    {
        log2_error("Didn't find PORT mark!");
        xml_destroy(xml);
        return -1;
    }

    info->port = atoi(node->value);

    /* 获取HTML字段 */
    node = xml_rsearch(xml, fix, "HTML");
    if (NULL == fix)
    {
        log2_error("Didn't find HTML mark!");
        xml_destroy(xml);
        return -1;
    }

    snprintf(info->html, sizeof(info->html), "%s", node->value);

    xml_destroy(xml);
    return 0;
}

/******************************************************************************
 **函数名称: parser_work_flow
 **功    能: 解析器处理流程
 **输入参数: 
 **     parser: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int parser_work_flow(parser_t *parser)
{
    int ret;
    char fname[FILE_PATH_MAX_LEN];
    parser_webpage_info_t *info = &parser->info;

    snprintf(fname, sizeof(fname), "../temp/webpage/%s", info->html);

    /* 1. 解析HTML文件 */
    parser->html = gumbo_html_parse(&parser->gumbo_ctx, fname);
    if (NULL == parser->html)
    {
        log_error(parser->log, "Parse html failed! fname:%s", fname);
        return -1;
    }

    /* 2. 提取超链接 */
    parser->result = gumbo_parse_href(&parser->gumbo_ctx, parser->html);
    if (NULL == parser->result)
    {
        gumbo_html_destroy(&parser->gumbo_ctx, parser->html);

        log_error(parser->log, "Parse href failed! fname:%s", fname);
        return -1;
    }

    /* 3. 深入处理超链接
     *  1. 判断超链接深度
     *  2. 判断超链接是否已被爬取
     *  3. 将超链接插入任务队列 */
    ret = parser_deep_handler(parser);
    if (0 != ret)
    {
        log_error(parser->log, "Deep handler failed! fname:%s", fname);
        return -1;
    }

    gumbo_result_destroy(&parser->gumbo_ctx, parser->result);
    gumbo_html_destroy(&parser->gumbo_ctx, parser->html);
    return 0;
}

/******************************************************************************
 **函数名称: parser_deep_handler
 **功    能: 深入分析和处理
 **输入参数: 
 **     parser: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 判断超链接深度
 **     2. 判断超链接是否已被爬取
 **     3. 将超链接插入任务队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/
static int parser_deep_handler(parser_t *parser)
{
    redisReply *r; 
    list_node_t *node = parser->result->list.head;

    while (NULL != node)
    {
        fprintf(stdout, "%s\n", (char *)node->data);

        /* 2. 取Undo任务数据 */
        r = redisCommand(parser->redis_ctx, "RPUSH CRWL_UNDO_TASKQ %s", (char *)node->data);
        if (REDIS_REPLY_NIL == r->type)
        {
            freeReplyObject(r);
            return 0;
        }

        freeReplyObject(r);

        node = node->next;
    }

    return 0;
}

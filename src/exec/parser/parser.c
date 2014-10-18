/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: parse.c
 ** 版本号: 1.0
 ** 描  述: 超链接的提取程序
 **         从爬取的网页中提取超链接
 ** 作  者: # Qifeng.zou # 2014.10.17 #
 ******************************************************************************/

#include <dir.h>
#include <dirent.h>

#include "common.h"

static int parser_trav_dir(parser_t *parser);
static int parser_work_flow(parser_t *parser, const char *fname);

int main(void)
{
    int ret;
    char *ptr;
    parser_t parser;
    char log_path[FILE_PATH_MAX_LEN];

    memset(&parser, 0, sizeof(parser));

    /* 1. 初始化日志模块 */
    log2_get_path(log_path, sizeof(log_path), basename(argv[0]));

    ret = log2_init(PARSER_LOG2_LEVEL, log_path);
    if (0 != ret)
    {
        fprintf(stderr, "Init log2 failed! level:%s path:%s\n",
                PARSER_LOG2_LEVEL, log_path);
        goto ERROR;
    }

    log_get_path(log_path, sizeof(log_path), basename(argv[0]));

    log = log_init(PARSER_LOG2_LEVEL, log_path);
    if (NULL == log)
    {
        log2_error("Init log failed!");
        goto ERROR;
    }

    /* 2. 初始化GUMBO对象 */
    ret = gumbo_init(&parser.gumbo_ctx);
    if (0 != ret)
    {
        log_error(parser.log, "Init gumbo failed!");
        return -1;
    }

    parser.log = log;

    while (1)
    {
        parser_trav_dir(parser);
        Sleep(5);
    }

    /* 3. 释放GUMBO对象 */
    gumbo_destroy(&parser->gumbo_ctx);
    return 0;
}

/******************************************************************************
 **函数名称: parser_trav_dir
 **功    能: 遍历目录
 **输入参数: 
 **     parser: 解析器对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.18 #
 ******************************************************************************/
static int parser_trav_dir(parser_t *parser)
{
    int ret;
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
        continue;
    }

    /* 2. 遍历文件 */
    while (NULL != (item = readdir(dir)))
    {
        snprintf(parser->finfo, sizeof(parser->finfo), "%s/%s", path, item->d_name); 

        /* 判断文件类型 */
        ret = stat(parser->finfo, &st);
        if (!S_ISREG(st.st_mode))
        {
            continue;
        }

        /* 调用解析处理流程 */
        parser_work_flow(parser);
    }

    closedir(dir);
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
    /* 1. 解析HTML文件 */
    parser->html = gumbo_html_parse(&parser->gumbo_ctx, parser->fname);
    if (NULL == parser->html)
    {
        log_error(parser->log, "Parse html failed! fname:%s", parser->fname);
        return -1;
    }

    /* 2. 提取超链接 */
    parser->result = gumbo_parse_href(&parser->gumbo_ctx, parser->html);
    if (NULL == parser->result)
    {
        gumbo_html_destroy(&parser->gumbo_ctx, parser->html);

        log_error(parser->log, "Parse href failed! fname:%s", parser->fname);
        return -1;
    }

    /* 3. 深入处理超链接
     *  1. 判断超链接深度
     *  2. 判断超链接是否已被爬取
     *  3. 将超链接插入任务队列 */
    ret = parser_deep_handler(parser);
    if (0 != ret)
    {
        log_error(parser->log, "Deep handler failed! fname:%s", parser->fname);
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
    return 0;
}

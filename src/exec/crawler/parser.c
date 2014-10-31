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

int main(int argc, char *argv[])
{
    int ret;
    log_cycle_t *log;
    crwl_conf_t conf;
    char log_path[FILE_NAME_MAX_LEN];

    daemon(1, 0);

    /* 1. 初始化日志模块 */
    log2_get_path(log_path, sizeof(log_path), basename(argv[0]));

    ret = log2_init(LOG_LEVEL_ERROR, log_path);
    if (0 != ret)
    {
        fprintf(stderr, "Init log2 failed!");
        return CRWL_ERR;
    }

    log_get_path(log_path, sizeof(log_path), basename(argv[0]));

    log = log_init(LOG_LEVEL_ERROR, log_path);
    if (NULL == log)
    {
        log2_error("Initialize log failed!");
        return CRWL_ERR;
    }

    /* 2. 加载配置信息 */
    ret = crwl_load_conf(&conf, "../conf/crawler.xml", log);
    if (CRWL_OK != ret)
    {
        log2_error("Initialize log failed!");
        return CRWL_ERR;
    }

    /* 3. 启动解析器 */
    return crwl_parser_exec(&conf);
}

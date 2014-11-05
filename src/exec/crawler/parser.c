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
    log_cycle_t *log;
    crwl_conf_t *conf;

    /* 1. 初始化日志模块 */
    log = crwl_init_log(argv[0]);
    if (NULL == log)
    {
        return CRWL_ERR;
    }

    /* 2. 加载配置信息 */
    conf = crwl_conf_creat("../conf/crawler.xml", log);
    if (NULL == conf)
    {
        log2_error("Initialize log failed!");
        return CRWL_ERR;
    }

#if !defined(__MEM_LEAK_CHECK__)
    daemon(1, 0);
#endif /*__MEM_LEAK_CHECK__*/

    /* 3. 启动解析器 */
    return crwl_parser_exec(conf, log);
}

/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.h
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__CRAWLER_H__)
#define __CRAWLER_H__

#include "log.h"
#include "common.h"

typedef struct
{
    int num;
    int thread_num;
    char man_ip[IP_ADDR_MAX_LEN];
}crwl_worker_t;

/* 爬虫配置信息 */
typedef struct
{
    int thread_num;                         /* 爬虫线程数 */
    char svrip[IP_ADDR_MAX_LEN];            /* 任务分发服务IP */
    int port;                               /* 任务分发服务端口 */
    char log_level_str[LOG_LEVEL_MAX_LEN];  /* 日志级别 */
}crwl_conf_t;

/* 爬虫服务信息 */
typedef struct
{
}crwl_svr_t;

#endif /*__CRAWLER_H__*/

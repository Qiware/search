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

#include <stdint.h>

#include "log.h"
#include "slab.h"
#include "list.h"
#include "queue.h"
#include "common.h"
#include "sck_api.h"
#include "hash_tab.h"
#include "crwl_priv.h"
#include "crwl_task.h"
#include "crwl_conf.h"
#include "thread_pool.h"

/* 发送数据的信息 */
typedef struct
{
    int type;                               /* 数据类型(crwl_data_type_e) */
    int length;                             /* 数据长度(报头+报体) */
} crwl_data_info_t;

/* 域名IP映射信息 */
typedef struct
{
    char host[URI_MAX_LEN];                 /* Host信息(域名) */

    int ip_num;                             /* IP地址数 */
#define CRWL_IP_MAX_NUM  (8)
    ipaddr_t ip[CRWL_IP_MAX_NUM];           /* 域名对应的IP地址 */
    time_t access_tm;                       /* 最近访问时间 */
} crwl_domain_ip_map_t;

/* 域名黑名单信息 */
typedef struct
{
    char host[URI_MAX_LEN];                 /* Host信息(域名) */

    time_t access_tm;                       /* 最近访问时间 */
} crwl_domain_blacklist_t;

/* 输入参数信息 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} crwl_opt_t;

/* 爬虫全局信息 */
typedef struct
{
    crwl_conf_t *conf;                      /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    slab_pool_t *slab;                      /* 内存池 */

    thread_pool_t *scheds;                  /* Sched线程池 */
    thread_pool_t *workers;                 /* Worker线程池 */

    queue_t **taskq;                        /* 任务队列(注: 与WORKER线程一一对象) */
    hash_tab_t *domain_ip_map;              /* 域名IP映射表: 通过域名找到IP地址 */
    hash_tab_t *domain_blacklist;           /* 域名黑名单 */
} crwl_cntx_t;

/* 对外接口 */
int crwl_getopt(int argc, char **argv, crwl_opt_t *opt);
int crwl_usage(const char *exec);
int crwl_proc_lock(void);
void crwl_set_signal(void);

log_cycle_t *crwl_init_log(char *fname);
crwl_cntx_t *crwl_cntx_init(char *pname, const char *path);
void crwl_cntx_destroy(crwl_cntx_t *ctx);
int crwl_startup(crwl_cntx_t *ctx);

int crwl_get_domain_ip_map(crwl_cntx_t *ctx, char *host, crwl_domain_ip_map_t *map);

#endif /*__CRAWLER_H__*/

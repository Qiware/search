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
#include "crawler.h"
#include "hash_tab.h"
#include "crwl_task.h"
#include "crwl_conf.h"
#include "xd_socket.h"
#include "thread_pool.h"

/* 宏定义 */
#define CRWL_TMOUT_SEC              (02)    /* 超时(秒) */
#define CRWL_TMOUT_USEC             (00)    /* 超时(微妙) */
#define CRWL_TMOUT_SCAN_SEC         (05)    /* 超时扫描间隔 */
#define CRWL_CONNECT_TMOUT_SEC      (00)    /* 连接超时时间 */

#define CRWL_THD_MAX_NUM            (64)    /* 最大线程数 */
#define CRWL_THD_DEF_NUM            (05)    /* 默认线程数 */
#define CRWL_THD_MIN_NUM            (01)    /* 最小线程数 */

#define CRWL_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define CRWL_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define CRWL_SYNC_SIZE  (CRWL_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define CRWL_CONN_MAX_NUM           (1024)  /* 最大网络连接数 */
#define CRWL_CONN_DEF_NUM           (128)   /* 默认网络连接数 */
#define CRWL_CONN_MIN_NUM           (1)     /* 最小网络连接数 */
#define CRWL_CONN_TMOUT_SEC         (15)    /* 连接超时时间(秒) */

#define CRWL_DOMAIN_IP_MAP_HASH_NUM (1777)  /* 域名IP映射表长度 */
#define CRWL_DOMAIN_BLACKLIST_HASH_NUM (10) /* 域名黑名单表长度 */
#define CRWL_DEPTH_NO_LIMIT         (-1)    /* 爬取深度无限制 */
#define CRWL_WEB_SVR_PORT           (80)    /* WEB服务器侦听端口 */


#define CRWL_TASK_QUEUE_MAX_NUM     (10000) /* 任务队列单元数 */
#define CRWL_DEF_CONF_PATH  "../conf/crawler.xml"   /* 默认配置路径 */

#define CRWL_EVENT_MAX_NUM          (2048)  /* 事件最大数 */

#define CRWL_TASK_STR_LEN           (8192)  /* TASK字串最大长度 */

#define crwl_get_task_str(str, size, uri, deep) /* TASK字串格式 */\
            snprintf(str, size, \
                "<TASK>" \
                    "<TYPE>%d</TYPE>"   /* Task类型 */\
                    "<BODY>" \
                        "<URI DEPTH=\"%d\">\"%s\"</URI>" /* 网页深度&URI */\
                    "</BODY>" \
                "</TASK>", \
                CRWL_TASK_DOWN_WEBPAGE_BY_URL, deep, uri);

#define crwl_write_webpage_finfo(fp, data)   /* 写入网页信息 */\
{ \
    fprintf(fp,  \
        "<WPI>\n" \
        "\t<URI DEPTH=\"%d\" IP=\"%s\" PORT=\"%d\">%s</URI>\n" \
        "\t<HTML SIZE=\"%lu\">\"%s.html\"</HTML>\n" \
        "</WPI>\n", \
        data->webpage.depth, data->webpage.ip, \
        data->webpage.port, data->webpage.uri, \
        data->webpage.size, data->webpage.fname); \
}

/* 错误码 */
typedef enum
{
    CRWL_OK = 0                             /* 正常 */
    , CRWL_SHOW_HELP                        /* 显示帮助信息 */
    , CRWL_SCK_CLOSE                        /* 套接字关闭 */

    , CRWL_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} crwl_err_code_e;

/* 数据类型 */
typedef enum
{
    CRWL_DATA_TYPE_UNKNOWN = 0              /* 未知数据 */
    , CRWL_HTTP_GET_REQ                     /* HTTP GET请求 */
} crwl_data_type_e;

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

    thread_pool_t *workers;                 /* 线程池对象 */
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

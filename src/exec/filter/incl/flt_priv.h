#if !defined(__FLT_PRIV_H__)
#define __FLT_PRIV_H__

#include "crwl_priv.h"

/* 宏定义 */
#define FLT_EVENT_TMOUT_MSEC       (2000)  /* 超时(毫秒) */
#define FLT_SCAN_TMOUT_SEC         (05)    /* 超时扫描间隔 */
#define FLT_CONNECT_TMOUT_SEC      (00)    /* 连接超时时间 */

#define FLT_THD_MAX_NUM            (64)    /* 最大线程数 */
#define FLT_THD_DEF_NUM            (05)    /* 默认线程数 */
#define FLT_THD_MIN_NUM            (01)    /* 最小线程数 */

#define FLT_SLAB_SIZE              (50 * MB)   /* SLAB内存池大小 */
#define FLT_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define FLT_SYNC_SIZE  (FLT_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define FLT_CONN_MAX_NUM           (1024)  /* 最大网络连接数 */
#define FLT_CONN_DEF_NUM           (128)   /* 默认网络连接数 */
#define FLT_CONN_MIN_NUM           (1)     /* 最小网络连接数 */
#define FLT_CONN_TMOUT_SEC         (15)    /* 连接超时时间(秒) */

#define FLT_DOMAIN_IP_MAP_HASH_MOD (1777)  /* 域名IP映射表模 */
#define FLT_DOMAIN_BLACKLIST_HASH_MOD (10) /* 域名黑名单表长度 */
#define FLT_DEPTH_NO_LIMIT         (-1)    /* 爬取深度无限制 */
#define FLT_WEB_SVR_PORT           (80)    /* WEB服务器侦听端口 */


#define FLT_WORKQ_MAX_NUM          (2000)  /* 工作队列单元数 */
#define FLT_DEF_CONF_PATH  "../conf/filter.xml"   /* 默认配置路径 */

#define FLT_EVENT_MAX_NUM          (2048)  /* 事件最大数 */

#define FLT_TASK_STR_LEN           (8192)  /* TASK字串最大长度 */

#define flt_get_task_str(str, size, uri, deep, ip, family) /* TASK字串格式 */\
            snprintf(str, size, \
                "<TASK>" \
                    "<TYPE>%d</TYPE>"   /* Task类型 */\
                    "<BODY>" \
                        "<IP FAMILY='%d'>%s</IP>"  /* IP地址和协议 */\
                        "<URI DEPTH='%d'>%s</URI>" /* 网页深度&URI */\
                    "</BODY>" \
                "</TASK>", \
                CRWL_TASK_DOWN_WEBPAGE, family, ip, deep, uri);

#define flt_write_webpage_finfo(fp, data)   /* 写入网页信息 */\
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
    FLT_OK = 0                             /* 正常 */
    , FLT_SHOW_HELP                        /* 显示帮助信息 */
    , FLT_SCK_CLOSE                        /* 套接字关闭 */

    , FLT_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} flt_err_code_e;

/* 数据类型 */
typedef enum
{
    FLT_DATA_TYPE_UNKNOWN = 0              /* 未知数据 */
    , FLT_HTTP_GET_REQ                     /* HTTP GET请求 */
} flt_data_type_e;

/* 输入参数信息 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} flt_opt_t;

/* 网页基本信息 */
typedef struct
{
    char fname[FILE_NAME_MAX_LEN];  /* 网页信息文件 */

    char uri[URI_MAX_LEN];          /* URI */
    uint32_t depth;                 /* 网页深度 */
    char ip[IP_ADDR_MAX_LEN];       /* IP地址 */
    int port;                       /* 端口号 */
    char html[FILE_NAME_MAX_LEN];   /* 网页存储名 */
    int size;                       /* 网页大小 */
} flt_webpage_info_t;

/* 域名IP映射信息 */
typedef struct
{
    char host[URI_MAX_LEN];                 /* Host信息(域名) */

    int ip_num;                             /* IP地址数 */
#define FLT_IP_MAX_NUM  (8)
    ipaddr_t ip[FLT_IP_MAX_NUM];           /* 域名对应的IP地址 */
    time_t access_tm;                       /* 最近访问时间 */
} flt_domain_ip_map_t;

/* 域名黑名单信息 */
typedef struct
{
    char host[URI_MAX_LEN];                 /* Host信息(域名) */

    time_t access_tm;                       /* 最近访问时间 */
} flt_domain_blacklist_t;

/* 对外接口 */
int flt_getopt(int argc, char **argv, flt_opt_t *opt);
int flt_usage(const char *exec);
int flt_proc_lock(void);
void flt_set_signal(void);
log_cycle_t *flt_init_log(char *fname);

int flt_domain_ip_map_cmp_cb(const char *domain, const flt_domain_ip_map_t *map);
int flt_domain_blacklist_cmp_cb(const char *domain, const flt_domain_blacklist_t *blacklist);

#endif /*__FLT_PRIV_H__*/

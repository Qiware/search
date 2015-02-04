#if !defined(__CRWL_PRIV_H__)
#define __CRWL_PRIV_H__

/* 宏定义 */
#define CRWL_EVENT_TMOUT_MSEC       (2000)  /* 超时(毫秒) */
#define CRWL_SCAN_TMOUT_SEC         (05)    /* 超时扫描间隔 */
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

#define CRWL_DOMAIN_IP_MAP_HASH_MOD (1777)  /* 域名IP映射表模 */
#define CRWL_DOMAIN_BLACKLIST_HASH_MOD (10) /* 域名黑名单表长度 */
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
                CRWL_TASK_DOWN_WEBPAGE, deep, uri);

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

#endif /*__CRWL_PRIV_H__*/

#if !defined(__SEARCH_H__)
#define __SEARCH_H__

#include "srch_conf.h"
#include "thread_pool.h"

/* 宏定义 */
#define SRCH_TMOUT_SEC              (02)    /* 超时(秒) */
#define SRCH_TMOUT_USEC             (00)    /* 超时(微妙) */
#define SRCH_TMOUT_SCAN_SEC         (05)    /* 超时扫描间隔 */
#define SRCH_CONNECT_TMOUT_SEC      (00)    /* 连接超时时间 */

#define SRCH_THD_MAX_NUM            (64)    /* 最大线程数 */
#define SRCH_THD_DEF_NUM            (05)    /* 默认线程数 */
#define SRCH_THD_MIN_NUM            (01)    /* 最小线程数 */

#define SRCH_SLAB_SIZE              (30 * MB)   /* SLAB内存池大小 */
#define SRCH_RECV_SIZE              (8 * KB)    /* 缓存SIZE(接收缓存) */
#define SRCH_SYNC_SIZE  (SRCH_RECV_SIZE >> 1)   /* 缓存同步SIZE */

#define SRCH_CONN_MAX_NUM           (1024)  /* 最大网络连接数 */
#define SRCH_CONN_DEF_NUM           (128)   /* 默认网络连接数 */
#define SRCH_CONN_MIN_NUM           (1)     /* 最小网络连接数 */
#define SRCH_CONN_TMOUT_SEC         (15)    /* 连接超时时间(秒) */

#define SRCH_DEF_CONF_PATH  "../conf/search.xml"   /* 默认配置路径 */

#define SRCH_EVENT_MAX_NUM          (2048)  /* 事件最大数 */


/* 错误码 */
typedef enum
{
    SRCH_OK = 0                             /* 正常 */
    , SRCH_SHOW_HELP                        /* 显示帮助信息 */
    , SRCH_SCK_CLOSE                        /* 套接字关闭 */

    , SRCH_ERR = ~0x7FFFFFFF                /* 失败、错误 */

} srch_err_code_e;

/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} srch_opt_t;

/* 搜索引擎对象 */
typedef struct
{
    srch_conf_t *conf;                      /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */

    thread_pool_t *workers;                 /* 线程池对象 */
} srch_cntx_t;

#endif /*__SEARCH_H__*/

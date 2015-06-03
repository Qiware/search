#if !defined(__PROB_CONF_H__)
#define __PROB_CONF_H__

#include "comm.h"
#include "queue.h"

/* 搜索引擎配置信息 */
typedef struct
{
    struct
    {
        int level;                          /* 日志级别 */
        int syslevel;                       /* 系统日志级别 */
    } log;                                  /* 日志配置 */
    struct
    {
        int max;                            /* 最大并发数 */
        int timeout;                        /* 连接超时时间 */
        int port;                           /* 侦听端口 */
    } connections;

    int agent_num;                          /* Agent线程数 */
    int worker_num;                         /* Worker线程数 */

    queue_conf_t connq;                     /* 连接队列 */
    queue_conf_t taskq;                     /* 任务队列 */
} prob_conf_t;

prob_conf_t *prob_conf_load(const char *path, log_cycle_t *log);
#define prob_conf_destroy(conf)             /* 销毁配置对象 */\
{ \
    free(conf); \
}

#endif /*__PROB_CONF_H__*/

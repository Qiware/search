#if !defined(__CRWL_TASK_H__)
#define __CRWL_TASK_H__

#include <stdint.h>
#include <pthread.h>

#include "queue.h"

/* 任务类型 */
typedef enum
{
    CRWL_TASK_TYPE_UNKNOWN                  /* 未知任务 */
    , CRWL_TASK_DOWN_WEBPAGE_BY_URL         /* 通过URL加载网页 */
    , CRWL_TASK_DOWN_WEBPAGE_BY_IP          /* 通过IP加载网页 */

    , CRWL_TASK_TYPE_TOTAL                  /* 任务总数 */
} crwl_task_type_e;

/* 爬虫任务 */
typedef struct
{
    crwl_task_type_e type;                  /* 任务类型 */
    uint32_t length;                        /* 数据长度(头+体) */
} crwl_task_t;

/* 爬虫队列 */
typedef struct
{
    pthread_rwlock_t lock;                  /* 任务队列锁(可用SPIN锁替换) */
    Queue_t queue;                          /* 任务队列 */
} crwl_task_queue_t;

/* 通过URL加载网页 */
typedef struct
{
    char uri[URL_MAX_LEN];                  /* URI */
    int deep;                               /* 当前URI的深度(0:表示深度未知) */
    int port;                               /* 端口号 */
} crwl_task_load_webpage_by_uri_t;

/* 通过IP加载网页 */
typedef struct
{
    int type;                               /* IP类型(IPV4或IPV6) */
    char ipaddr[IP_ADDR_MAX_LEN];           /* IP地址 */
    int port;                               /* 端口号 */
} crwl_task_load_webpage_by_ip_t;

/* TASK空间 */
typedef union
{
    crwl_task_load_webpage_by_uri_t uri;
    crwl_task_load_webpage_by_ip_t ip;
} crwl_task_space_u;

int crwl_task_queue_init(crwl_task_queue_t *tq, int max);
int crwl_task_queue_push(crwl_task_queue_t *tq, void *addr);
void *crwl_task_queue_pop(crwl_task_queue_t *tq);
void crwl_task_queue_destroy(crwl_task_queue_t *tq);
#endif /*__CRWL_TASK_H__*/

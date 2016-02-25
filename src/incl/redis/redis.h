#if !defined(__REDIS_H__)
#define __REDIS_H__

#include "comm.h"
#include "list.h"
#include <hiredis/hiredis.h>

#include "redis_hash.h"
#include "redis_list.h"

#define REDIS_CONN_TMOUT_SEC    (30)    /* 连接超时:秒 */
#define REDIS_CONN_TMOUT_USEC   (0)     /* 连接超时:微妙 */

/* Redis配置 */
typedef struct
{
    char ip[IP_ADDR_MAX_LEN];           /* IP */
    int port;                           /* 端口号 */
    char passwd[PASSWD_MAX_LEN];        /* 密码 */
} redis_conf_t;

/* Redis集群对象 */
typedef struct
{
    int num;                            /* REDIS对象数 */
#define REDIS_MASTER_IDX (0)            /* MASTER索引 */
    redisContext **redis;               /* REDIS对象(注: [0]为Master [1~num-1]为Slave */
} redis_clst_t;

redis_clst_t *redis_clst_init(const redis_conf_t *conf, int num);
void redis_clst_destroy(redis_clst_t *clst);

#endif /*__REDIS_H__*/

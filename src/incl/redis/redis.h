#if !defined(__REDIS_H__)
#define __REDIS_H__

#include "comm.h"
#include "list.h"
#include <hiredis/hiredis.h>

#include "redis_hash.h"
#include "redis_list.h"

/* Redis配置 */
typedef struct
{
    char ip[IP_ADDR_MAX_LEN];           /* IP */
    int port;                           /* 端口号 */
    char passwd[PASSWD_MAX_LEN];        /* 密码 */
} redis_conf_t;

redisContext *redis_init(const redis_conf_t *conf, int sec);
#define redis_destroy(redis) redisFree(redis)

#endif /*__REDIS_H__*/

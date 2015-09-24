#if !defined(__REDIS_HASH_H__)
#define __REDIS_HASH_H__

#include "comm.h"
#include "list.h"
#include <hiredis/hiredis.h>

bool redis_hsetnx(redisContext *ctx, const char *hname, const char *key, const char *value);
int redis_hlen(redisContext *ctx, const char *hname);

#endif /*__REDIS_HASH_H__*/

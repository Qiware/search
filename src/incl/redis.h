#if !defined(__REDIS_H__)
#define __REDIS_H__

#include <hiredis/hiredis.h>

#include "common.h"

bool redis_hsetnx(redisContext *ctx, const char *hash, const char *key, const char *value);

#endif /*__HTTP_H__*/

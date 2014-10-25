#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "log.h"
#include "redis.h"
#include "xd_str.h"
#include "common.h"

/******************************************************************************
 **函数名称: redis_hsetnx
 **功    能: 设置HASH表中指定KEY的值(前提: 当指定KEY不存在时才更新)
 **输入参数: 
 **     ctx: Redis信息
 **     hash: HASH表名
 **     key: 指定KEY
 **     value: 设置值
 **输出参数:
 **返    回: true:更新成功 false:更新失败(已存在)
 **实现描述: 
 **     HSETNX:
 **         1. 表示新的Value被设置了新值
 **         0. 表示Key已经存在，该命令没有进行任何操作。
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.26 #
 ******************************************************************************/
bool redis_hsetnx(redisContext *ctx,
        const char *hash, const char *key, const char *value)
{
    redisReply *r;

    r = redisCommand(ctx, "HSETNX %s %s %s", hash, key, value);
    if (REDIS_REPLY_INTEGER != r->type)
    {
        freeReplyObject(r);
        return true;
    }

    if (1 == r->integer)
    {
        freeReplyObject(r);
        return false;
    }

    freeReplyObject(r);
    return true;
}
